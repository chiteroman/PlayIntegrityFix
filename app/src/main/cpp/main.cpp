#include <android/log.h>
#include <sys/system_properties.h>
#include <unistd.h>

#include "zygisk.hpp"
#include "dobby.h"
#include "json.hpp"

#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, "PIF/Native", __VA_ARGS__)

#define DEX_FILE_PATH "/data/adb/modules/playintegrityfix/classes.dex"

#define PROP_FILE_PATH "/data/adb/modules/playintegrityfix/pif.json"

#define DEF_FIRST_API_LEVEL "23"

#define DEF_SECURITY_PATCH "2017-08-05"

static std::string SECURITY_PATCH, FIRST_API_LEVEL;

typedef void (*T_Callback)(void *, const char *, const char *, uint32_t);

static std::map<void *, T_Callback> callbacks;

static void modify_callback(void *cookie, const char *name, const char *value, uint32_t serial) {

    if (cookie == nullptr || name == nullptr || value == nullptr ||
        !callbacks.contains(cookie))
        return;

    std::string_view prop(name);

    if (prop.ends_with("api_level")) {
        if (FIRST_API_LEVEL.empty()) {
            value = nullptr;
        } else {
            value = FIRST_API_LEVEL.c_str();
        }
    } else if (prop.ends_with("security_patch")) {
        if (SECURITY_PATCH.empty()) {
            value = nullptr;
        } else {
            value = SECURITY_PATCH.c_str();
        }
    }

    if (!prop.starts_with("cache") && !prop.starts_with("debug")) LOGD("[%s] -> %s", name, value);

    return callbacks[cookie](cookie, name, value, serial);
}

static void (*o_system_property_read_callback)(const prop_info *, T_Callback, void *);

static void
my_system_property_read_callback(const prop_info *pi, T_Callback callback, void *cookie) {
    if (pi == nullptr || callback == nullptr || cookie == nullptr) {
        return o_system_property_read_callback(pi, callback, cookie);
    }
    callbacks[cookie] = callback;
    return o_system_property_read_callback(pi, modify_callback, cookie);
}

static void doHook() {
    void *handle = DobbySymbolResolver("libc.so", "__system_property_read_callback");
    if (handle == nullptr) {
        LOGD("Couldn't find '__system_property_read_callback' handle. Report to @chiteroman");
        return;
    }
    LOGD("Found '__system_property_read_callback' handle at %p", handle);
    DobbyHook(handle, reinterpret_cast<void *>(my_system_property_read_callback),
              reinterpret_cast<void **>(&o_system_property_read_callback));
}

class PlayIntegrityFix : public zygisk::ModuleBase {
public:
    void onLoad(zygisk::Api *api, JNIEnv *env) override {
        this->api = api;
        this->env = env;
    }

    void preAppSpecialize(zygisk::AppSpecializeArgs *args) override {
        bool isGms = false;
        bool isGmsUnstable = false;

        auto rawProcess = env->GetStringUTFChars(args->nice_name, nullptr);
        if (rawProcess) {
            std::string_view process(rawProcess);
            isGms = process.starts_with("com.google.android.gms");
            isGmsUnstable = process.compare("com.google.android.gms.unstable") == 0;
        }
        env->ReleaseStringUTFChars(args->nice_name, rawProcess);

        if (!isGms) {
            api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
            return;
        }

        api->setOption(zygisk::FORCE_DENYLIST_UNMOUNT);

        if (!isGmsUnstable) {
            api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
            return;
        }

        auto rawDir = env->GetStringUTFChars(args->app_data_dir, nullptr);
        path = rawDir;
        env->ReleaseStringUTFChars(args->app_data_dir, rawDir);

        int fd = api->connectCompanion();

        write(fd, path.data(), path.size());

        read(fd, nullptr, 0);

        close(fd);
    }

    void postAppSpecialize(const zygisk::AppSpecializeArgs *args) override {
        if (path.empty()) return;

        std::string prop(path + "/cache/pif.json");
        if (std::filesystem::exists(prop)) {
            FILE *file = fopen(prop.c_str(), "r");
            nlohmann::json json = nlohmann::json::parse(file, nullptr, false);
            fclose(file);
            LOGD("Read %d props from pif.json", static_cast<int>(json.size()));
            SECURITY_PATCH = json["SECURITY_PATCH"].get<std::string>();
            FIRST_API_LEVEL = json["FIRST_API_LEVEL"].get<std::string>();
            json.clear();
        } else {
            LOGD("File pif.json doesn't exist, using default values");
            SECURITY_PATCH = DEF_SECURITY_PATCH;
            FIRST_API_LEVEL = DEF_FIRST_API_LEVEL;
        }

        std::string dex(path + "/cache/classes.dex");
        if (std::filesystem::exists(dex)) {
            inject();
        }

        doHook();
    }

    void preServerSpecialize(zygisk::ServerSpecializeArgs *args) override {
        api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
    }

private:
    zygisk::Api *api = nullptr;
    JNIEnv *env = nullptr;
    std::string path;

    void inject() {
        LOGD("get system classloader");
        auto clClass = env->FindClass("java/lang/ClassLoader");
        auto getSystemClassLoader = env->GetStaticMethodID(clClass, "getSystemClassLoader",
                                                           "()Ljava/lang/ClassLoader;");
        auto systemClassLoader = env->CallStaticObjectMethod(clClass, getSystemClassLoader);

        LOGD("create class loader");
        auto pathClClass = env->FindClass("dalvik/system/PathClassLoader");
        auto pathClInit = env->GetMethodID(pathClClass, "<init>",
                                           "(Ljava/lang/String;Ljava/lang/ClassLoader;)V");
        std::string dexPath(path + "/cache/classes.dex");
        auto moduleDex = env->NewStringUTF(dexPath.c_str());
        auto dexCl = env->NewObject(pathClClass, pathClInit, moduleDex, systemClassLoader);

        LOGD("load class");
        auto loadClass = env->GetMethodID(clClass, "loadClass",
                                          "(Ljava/lang/String;)Ljava/lang/Class;");
        auto entryClassName = env->NewStringUTF("es.chiteroman.playintegrityfix.EntryPoint");
        auto entryClassObj = env->CallObjectMethod(dexCl, loadClass, entryClassName);

        auto entryClass = (jclass) entryClassObj;

        LOGD("read props");
        auto readProps = env->GetStaticMethodID(entryClass, "readProps", "(Ljava/lang/String;)V");
        std::string prop(path + "/cache/pif.json");
        auto javaPath = env->NewStringUTF(prop.c_str());
        env->CallStaticVoidMethod(entryClass, readProps, javaPath);

        LOGD("call init");
        auto entryInit = env->GetStaticMethodID(entryClass, "init", "()V");
        env->CallStaticVoidMethod(entryClass, entryInit);

        path.clear();
    }
};

static void companion(int fd) {
    char buffer[256];
    long size = read(fd, buffer, sizeof(buffer));

    std::string path(buffer, size);
    LOGD("[ROOT] Got GMS dir: %s", path.c_str());

    std::string dex(path + "/cache/classes.dex");
    std::string prop(path + "/cache/pif.json");

    if (std::filesystem::copy_file(DEX_FILE_PATH, dex,
                                   std::filesystem::copy_options::overwrite_existing)) {
        std::filesystem::permissions(dex, std::filesystem::perms::owner_read |
                                          std::filesystem::perms::group_read |
                                          std::filesystem::perms::others_read);
    }

    if (std::filesystem::copy_file(PROP_FILE_PATH, prop,
                                   std::filesystem::copy_options::overwrite_existing)) {
        std::filesystem::permissions(prop, std::filesystem::perms::owner_read |
                                           std::filesystem::perms::group_read |
                                           std::filesystem::perms::others_read);
    }

    write(fd, nullptr, 0);
}

REGISTER_ZYGISK_MODULE(PlayIntegrityFix)

REGISTER_ZYGISK_COMPANION(companion)