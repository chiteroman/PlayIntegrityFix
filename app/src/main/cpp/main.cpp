#include <android/log.h>
#include <sys/system_properties.h>
#include <unistd.h>
#include <string_view>
#include <map>

#include "zygisk.hpp"
#include "shadowhook.h"

#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, "PIF/Native", __VA_ARGS__)

#define FIRST_API_LEVEL "23"

#define SECURITY_PATCH "2018-01-05"

typedef void (*T_Callback)(void *, const char *, const char *, uint32_t);

static std::map<void *, T_Callback> callbacks;

static void modify_callback(void *cookie, const char *name, const char *value, uint32_t serial) {

    if (cookie == nullptr || name == nullptr || value == nullptr ||
        !callbacks.contains(cookie))
        return;

    std::string_view prop(name);

    if (prop.ends_with("api_level")) {
        value = FIRST_API_LEVEL;
        LOGD("[%s] -> %s", name, value);
    } else if (prop.ends_with("security_patch")) {
        value = SECURITY_PATCH;
        LOGD("[%s] -> %s", name, value);
    }

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
    shadowhook_init(SHADOWHOOK_MODE_UNIQUE, false);
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

class PlayIntegrityFix : public zygisk::ModuleBase {
public:
    void onLoad(zygisk::Api *api, JNIEnv *env) override {
        this->api = api;
        this->env = env;
    }

    void preAppSpecialize(zygisk::AppSpecializeArgs *args) override {
        bool isGms = false, isGmsUnstable = false;

        auto rawProcess = env->GetStringUTFChars(args->nice_name, nullptr);

        if (rawProcess) {
            std::string_view process(rawProcess);

            isGms = process.starts_with("com.google.android.gms");
            isGmsUnstable = process.compare("com.google.android.gms.unstable") == 0;
        }

        env->ReleaseStringUTFChars(args->nice_name, rawProcess);

        if (isGms) api->setOption(zygisk::FORCE_DENYLIST_UNMOUNT);

        if (isGmsUnstable) {
            long size = 0;
            int fd = api->connectCompanion();

            read(fd, &size, sizeof(long));

            if (size > 0) {
                vector.resize(size);
                read(fd, vector.data(), size);
                LOGD("Read %ld bytes from fd!", size);
            } else {
                LOGD("Couldn't read classes.dex from fd!");
                api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
                return;
            }

            close(fd);

            return;
        }

        api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
    }

    void postAppSpecialize(const zygisk::AppSpecializeArgs *args) override {
        if (vector.empty()) return;

        doHook();

        inject();

        vector.clear();
    }

    void preServerSpecialize(zygisk::ServerSpecializeArgs *args) override {
        api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
    }

private:
    zygisk::Api *api = nullptr;
    JNIEnv *env = nullptr;
    std::vector<char> vector;

    void inject() {
        LOGD("get system classloader");
        auto clClass = env->FindClass("java/lang/ClassLoader");
        auto getSystemClassLoader = env->GetStaticMethodID(clClass, "getSystemClassLoader",
                                                           "()Ljava/lang/ClassLoader;");
        auto systemClassLoader = env->CallStaticObjectMethod(clClass, getSystemClassLoader);

        LOGD("create class loader");
        auto dexClClass = env->FindClass("dalvik/system/InMemoryDexClassLoader");
        auto dexClInit = env->GetMethodID(dexClClass, "<init>",
                                          "(Ljava/nio/ByteBuffer;Ljava/lang/ClassLoader;)V");
        auto buffer = env->NewDirectByteBuffer(vector.data(), static_cast<jlong>(vector.size()));
        auto dexCl = env->NewObject(dexClClass, dexClInit, buffer, systemClassLoader);

        LOGD("load class");
        auto loadClass = env->GetMethodID(clClass, "loadClass",
                                          "(Ljava/lang/String;)Ljava/lang/Class;");
        auto entryClassName = env->NewStringUTF("es.chiteroman.playintegrityfix.EntryPoint");
        auto entryClassObj = env->CallObjectMethod(dexCl, loadClass, entryClassName);

        auto entryClass = (jclass) entryClassObj;

        LOGD("call init");
        auto entryInit = env->GetStaticMethodID(entryClass, "init", "()V");
        env->CallStaticVoidMethod(entryClass, entryInit);
    }
};

static void companion(int fd) {
    long size = 0;
    std::vector<char> vector;

    FILE *file = fopen("/data/adb/modules/playintegrityfix/classes.dex", "rb");

    if (file) {
        fseek(file, 0, SEEK_END);
        size = ftell(file);
        fseek(file, 0, SEEK_SET);

        vector.resize(size);
        fread(vector.data(), 1, size, file);

        fclose(file);
    }

    write(fd, &size, sizeof(long));
    write(fd, vector.data(), size);
}

REGISTER_ZYGISK_MODULE(PlayIntegrityFix)

REGISTER_ZYGISK_COMPANION(companion)