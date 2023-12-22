#include <android/log.h>
#include <sys/system_properties.h>
#include <unistd.h>
#include <filesystem>

#include "zygisk.hpp"
#include "dobby.h"

#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, "PIF/Native", __VA_ARGS__)

#define CLASSES_DEX "/data/adb/modules/playintegrityfix/classes.dex"

#define PIF_JSON "/data/adb/pif.json"

#define PIF_PROP "/data/adb/pif.prop"

typedef void (*T_Callback)(void *, const char *, const char *, uint32_t);

static volatile T_Callback o_callback = nullptr;

static void modify_callback(void *cookie, const char *name, const char *value, uint32_t serial) {

    if (cookie == nullptr || name == nullptr || o_callback == nullptr) return;

    if (std::string_view(name).ends_with("api_level")) {

        value = "23";
        LOGD("Set '%s' to '%s'", name, value);
    }

    return o_callback(cookie, name, value, serial);
}

static void (*o_system_property_read_callback)(const prop_info *, T_Callback, void *);

static void
my_system_property_read_callback(const prop_info *pi, T_Callback callback, void *cookie) {
    if (pi == nullptr || callback == nullptr || cookie == nullptr) {
        return o_system_property_read_callback(pi, callback, cookie);
    }
    o_callback = callback;
    return o_system_property_read_callback(pi, modify_callback, cookie);
}

static void doHook() {
    void *handle = DobbySymbolResolver(nullptr, "__system_property_read_callback");
    if (handle == nullptr) {
        LOGD("Couldn't find '__system_property_read_callback' handle. Report to @chiteroman");
        return;
    }
    LOGD("Found '__system_property_read_callback' handle at %p", handle);
    DobbyHook(
            handle,
            reinterpret_cast<dobby_dummy_func_t>(my_system_property_read_callback),
            reinterpret_cast<dobby_dummy_func_t *>(&o_system_property_read_callback)
    );
}

class PlayIntegrityFix : public zygisk::ModuleBase {
public:
    void onLoad(zygisk::Api *api, JNIEnv *env) override {
        this->api = api;
        this->env = env;
    }

    void preAppSpecialize(zygisk::AppSpecializeArgs *args) override {

        auto name = env->GetStringUTFChars(args->nice_name, nullptr);

        if (name && strncmp(name, "com.google.android.gms", 22) == 0) {

            api->setOption(zygisk::FORCE_DENYLIST_UNMOUNT);

            if (strcmp(name, "com.google.android.gms.unstable") == 0) {

                auto rawDir = env->GetStringUTFChars(args->app_data_dir, nullptr);
                dir = rawDir;
                env->ReleaseStringUTFChars(args->app_data_dir, rawDir);

                long size = dir.size();
                bool done = false;

                int fd = api->connectCompanion();

                write(fd, &size, sizeof(long));
                write(fd, dir.data(), size);

                read(fd, &done, sizeof(bool));

                close(fd);

                LOGD("Files copied: %d", done);

                goto clear;
            }
        }

        api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);

        clear:
        env->ReleaseStringUTFChars(args->nice_name, name);
    }

    void postAppSpecialize(const zygisk::AppSpecializeArgs *args) override {
        if (dir.empty()) return;

        doHook();

        std::string classesDex(dir + "/classes.dex");

        FILE *dexFile = fopen(classesDex.c_str(), "rb");

        if (dexFile == nullptr) {

            LOGD("classes.dex doesn't exist... This is weird.");
            dir.clear();
            api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
            return;
        }

        fclose(dexFile);

        LOGD("get system classloader");
        auto clClass = env->FindClass("java/lang/ClassLoader");
        auto getSystemClassLoader = env->GetStaticMethodID(clClass, "getSystemClassLoader",
                                                           "()Ljava/lang/ClassLoader;");
        auto systemClassLoader = env->CallStaticObjectMethod(clClass, getSystemClassLoader);

        LOGD("create class loader");
        auto dexClClass = env->FindClass("dalvik/system/PathClassLoader");
        auto dexClInit = env->GetMethodID(dexClClass, "<init>",
                                          "(Ljava/lang/String;Ljava/lang/ClassLoader;)V");
        auto dexStr = env->NewStringUTF(classesDex.c_str());
        auto dexCl = env->NewObject(dexClClass, dexClInit, dexStr, systemClassLoader);

        LOGD("load class");
        auto loadClass = env->GetMethodID(clClass, "loadClass",
                                          "(Ljava/lang/String;)Ljava/lang/Class;");
        auto entryClassName = env->NewStringUTF("es.chiteroman.playintegrityfix.EntryPoint");
        auto entryClassObj = env->CallObjectMethod(dexCl, loadClass, entryClassName);

        auto entryClass = (jclass) entryClassObj;

        LOGD("call init");
        auto entryInit = env->GetStaticMethodID(entryClass, "init", "(Ljava/lang/String;)V");
        auto str = env->NewStringUTF(findFileInDirectory().c_str());
        env->CallStaticVoidMethod(entryClass, entryInit, str);

        dir.clear();
    }

    void preServerSpecialize(zygisk::ServerSpecializeArgs *args) override {
        api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
    }

private:
    zygisk::Api *api = nullptr;
    JNIEnv *env = nullptr;
    std::string dir;

    std::string findFileInDirectory() {

        std::string jsonFilePath = dir + "/pif.json";
        std::string propFilePath = dir + "/pif.prop";

        FILE *jsonFile = fopen(jsonFilePath.c_str(), "r");
        if (jsonFile) {
            fclose(jsonFile);
            return jsonFilePath;
        }

        FILE *propFile = fopen(propFilePath.c_str(), "r");
        if (propFile) {
            fclose(propFile);
            return propFilePath;
        }

        return dir;
    }
};

static void companion(int fd) {

    long size = 0;
    std::string dir;

    read(fd, &size, sizeof(long));

    dir.resize(size);

    read(fd, dir.data(), size);

    LOGD("[ROOT] GMS dir: %s", dir.c_str());

    std::string classesDex(dir + "/classes.dex");

    bool a = std::filesystem::copy_file(CLASSES_DEX, classesDex,
                                        std::filesystem::copy_options::overwrite_existing);

    std::filesystem::permissions(classesDex, std::filesystem::perms::owner_read |
                                             std::filesystem::perms::group_read |
                                             std::filesystem::perms::others_read);

    bool b = false;

    if (std::filesystem::exists(PIF_JSON)) {

        std::string pifJson(dir + "/pif.json");

        b = std::filesystem::copy_file(PIF_JSON, pifJson,
                                       std::filesystem::copy_options::overwrite_existing);

        std::filesystem::permissions(pifJson, std::filesystem::perms::owner_read |
                                              std::filesystem::perms::group_read |
                                              std::filesystem::perms::others_read);

    } else if (std::filesystem::exists(PIF_PROP)) {

        std::string pifProp(dir + "/pif.prop");

        b = std::filesystem::copy_file(PIF_PROP, pifProp,
                                       std::filesystem::copy_options::overwrite_existing);

        std::filesystem::permissions(pifProp, std::filesystem::perms::owner_read |
                                              std::filesystem::perms::group_read |
                                              std::filesystem::perms::others_read);
    }

    bool done = a && b;

    write(fd, &done, sizeof(bool));
}

REGISTER_ZYGISK_MODULE(PlayIntegrityFix)

REGISTER_ZYGISK_COMPANION(companion)
