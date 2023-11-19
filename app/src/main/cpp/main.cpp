#include <android/log.h>
#include <sys/system_properties.h>
#include <unistd.h>
#include <string>
#include <vector>
#include <filesystem>

#include "zygisk.hpp"
#include "shadowhook.h"

#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, "PIF/Native", __VA_ARGS__)

#define FIRST_API_LEVEL "32"

#define DEX_FILE_PATH "/data/adb/modules/playintegrityfix/classes.dex"

#define PROP_FILE_PATH "/data/adb/modules/playintegrityfix/pif.prop"

typedef void (*T_Callback)(void *, const char *, const char *, uint32_t);

static volatile T_Callback propCallback = nullptr;

static void modify_callback(void *cookie, const char *name, const char *value, uint32_t serial) {

    if (cookie == nullptr || name == nullptr || value == nullptr || propCallback == nullptr) return;

    std::string_view prop(name);

    if (prop.compare("ro.product.first_api_level") == 0) {
        LOGD("Property '%s' with value '%s' is now spoofed to '%s'", name, value, FIRST_API_LEVEL);
        value = FIRST_API_LEVEL;
    }

    if (!prop.starts_with("cache")) LOGD("[%s] -> %s", name, value);

    return propCallback(cookie, name, value, serial);
}

static void (*o_system_property_read_callback)(const prop_info *, T_Callback, void *);

static void
my_system_property_read_callback(const prop_info *pi, T_Callback callback, void *cookie) {

    if (pi == nullptr || callback == nullptr || cookie == nullptr) {
        return o_system_property_read_callback(pi, callback, cookie);
    }
    propCallback = callback;
    return o_system_property_read_callback(pi, modify_callback, cookie);
}

static void doHook() {
    shadowhook_init(SHADOWHOOK_MODE_UNIQUE, true);
    void *handle = shadowhook_hook_sym_name(
            "libc.so",
            "__system_property_read_callback",
            reinterpret_cast<void *>(my_system_property_read_callback),
            reinterpret_cast<void **>(&o_system_property_read_callback)
    );
    if (handle == nullptr) {
        LOGD("Couldn't find '__system_property_read_callback' handle. Report to @chiteroman");
        return;
    }
    LOGD("Found '__system_property_read_callback' handle at %p", handle);
}

static bool needHook() {
    char rawApi[2];
    if (__system_property_get("ro.product.first_api_level", rawApi) < 1) return true;
    int api = std::stoi(rawApi);
    return api > 32;
}

class PlayIntegrityFix : public zygisk::ModuleBase {
public:
    void onLoad(zygisk::Api *api, JNIEnv *env) override {
        this->api = api;
        this->env = env;
    }

    void preAppSpecialize(zygisk::AppSpecializeArgs *args) override {
        auto rawProcess = env->GetStringUTFChars(args->nice_name, nullptr);

        std::string_view process(rawProcess);

        bool isGms = process.starts_with("com.google.android.gms");
        isGmsUnstable = process.compare("com.google.android.gms.unstable") == 0;

        env->ReleaseStringUTFChars(args->nice_name, rawProcess);

        if (isGms) api->setOption(zygisk::FORCE_DENYLIST_UNMOUNT);

        if (isGmsUnstable) {

            auto rawDir = env->GetStringUTFChars(args->app_data_dir, nullptr);
            std::string dir(rawDir);
            env->ReleaseStringUTFChars(args->app_data_dir, rawDir);

            LOGD("GMS data dir: %s", dir.c_str());

            int fd = api->connectCompanion();

            int strSize = static_cast<int>(dir.size());

            write(fd, &strSize, sizeof(strSize));
            write(fd, dir.data(), dir.size());

            dir.clear();
            dir.shrink_to_fit();

            long size;
            read(fd, &size, sizeof(size));

            moduleDex.resize(size);
            read(fd, moduleDex.data(), size);

            close(fd);

            hook = needHook();

            if (hook) return;
        }

        api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
    }

    void postAppSpecialize(const zygisk::AppSpecializeArgs *args) override {
        if (!isGmsUnstable) return;
        if (hook) doHook();
        if (!moduleDex.empty()) injectDex();
    }

    void preServerSpecialize(zygisk::ServerSpecializeArgs *args) override {
        api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
    }

private:
    zygisk::Api *api = nullptr;
    JNIEnv *env = nullptr;
    bool isGmsUnstable = false;
    bool hook = false;
    std::vector<char> moduleDex;

    void injectDex() {
        LOGD("get system classloader");
        auto clClass = env->FindClass("java/lang/ClassLoader");
        auto getSystemClassLoader = env->GetStaticMethodID(clClass, "getSystemClassLoader",
                                                           "()Ljava/lang/ClassLoader;");
        auto systemClassLoader = env->CallStaticObjectMethod(clClass, getSystemClassLoader);

        LOGD("create buffer");
        auto buf = env->NewDirectByteBuffer(moduleDex.data(), static_cast<jlong>(moduleDex.size()));
        LOGD("create class loader");
        auto dexClClass = env->FindClass("dalvik/system/InMemoryDexClassLoader");
        auto dexClInit = env->GetMethodID(dexClClass, "<init>",
                                          "(Ljava/nio/ByteBuffer;Ljava/lang/ClassLoader;)V");
        auto dexCl = env->NewObject(dexClClass, dexClInit, buf, systemClassLoader);

        LOGD("load class");
        auto loadClass = env->GetMethodID(clClass, "loadClass",
                                          "(Ljava/lang/String;)Ljava/lang/Class;");
        auto entryClassName = env->NewStringUTF("es.chiteroman.playintegrityfix.EntryPoint");
        auto entryClassObj = env->CallObjectMethod(dexCl, loadClass, entryClassName);

        LOGD("call init");
        auto entryClass = (jclass) entryClassObj;
        auto entryInit = env->GetStaticMethodID(entryClass, "init", "()V");
        env->CallStaticVoidMethod(entryClass, entryInit);

        LOGD("Injected %d bytes to the process", static_cast<int>(moduleDex.size()));
    }
};

static void companion(int fd) {
    int strSize;
    read(fd, &strSize, sizeof(strSize));

    std::string propFile;

    propFile.resize(strSize);

    read(fd, propFile.data(), strSize);

    propFile = propFile + "/cache/pif.prop";

    std::filesystem::copy_file(PROP_FILE_PATH, propFile,
                               std::filesystem::copy_options::overwrite_existing);

    std::filesystem::permissions(propFile, std::filesystem::perms::owner_read |
                                           std::filesystem::perms::group_read |
                                           std::filesystem::perms::others_read);

    propFile.clear();
    propFile.shrink_to_fit();

    FILE *file = fopen(DEX_FILE_PATH, "rb");

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char buffer[size];
    fread(buffer, 1, size, file);
    fclose(file);

    write(fd, &size, sizeof(size));
    write(fd, buffer, size);
}

REGISTER_ZYGISK_MODULE(PlayIntegrityFix)

REGISTER_ZYGISK_COMPANION(companion)