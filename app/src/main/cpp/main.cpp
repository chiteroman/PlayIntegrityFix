#include <stdlib.h>
#include <string.h>
#include <android/log.h>
#include <sys/system_properties.h>

#include "zygisk.hpp"
#include "classes_dex.h"

#if defined(__arm__) || defined(__aarch64__)

#include "shadowhook.h"

#endif

#if defined(__i386__) || defined(__x86_64__)

#include "dobby.h"

#endif

#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, "PIF/Native", __VA_ARGS__)

static void (*o_callback)(void *, const char *, const char *, uint32_t);

static void modify_callback(void *cookie, const char *name, const char *value, uint32_t serial) {

    if (cookie != nullptr && name != nullptr) {

        if (strstr(name, "api_level") != nullptr) value = "25";

        if (strncmp(name, "cache", 5) != 0) LOGD("[%s] -> %s", name, value);
    }

    return o_callback(cookie, name, value, serial);
}

static void (*o_system_property_read_callback)(const prop_info *,
                                               void (*)(void *, const char *, const char *,
                                                        uint32_t),
                                               void *);

static void my_system_property_read_callback(const prop_info *pi,
                                             void (*callback)(void *, const char *, const char *,
                                                              uint32_t),
                                             void *cookie) {

    if (pi == nullptr || callback == nullptr || cookie == nullptr) {
        return o_system_property_read_callback(pi, callback, cookie);
    }
    o_callback = callback;
    return o_system_property_read_callback(pi, modify_callback, cookie);
}

static void doHook() {
    LOGD("Starting to hook...");
    void *handle;

#if defined(__arm__) || defined(__aarch64__)
    shadowhook_init(SHADOWHOOK_MODE_UNIQUE, true);
    handle = shadowhook_hook_sym_name(
            "libc.so",
            "__system_property_read_callback",
            reinterpret_cast<void *>(my_system_property_read_callback),
            reinterpret_cast<void **>(&o_system_property_read_callback)
    );
#endif

#if defined(__i386__) || defined(__x86_64__)
    handle = DobbySymbolResolver(nullptr, "__system_property_read_callback");
#endif

    if (handle == nullptr) {
        LOGD("Couldn't get __system_property_read_callback handle.");
        return;
    }

#if defined(__i386__) || defined(__x86_64__)
        DobbyHook(
                handle,
                reinterpret_cast<void *>(my_system_property_read_callback),
                reinterpret_cast<void **>(&o_system_property_read_callback)
        );
#endif

    LOGD("Got __system_property_read_callback handle and hooked it at %p", handle);
}

class PlayIntegrityFix : public zygisk::ModuleBase {
public:
    void onLoad(zygisk::Api *api, JNIEnv *env) override {
        this->api = api;
        this->env = env;
    }

    void preAppSpecialize(zygisk::AppSpecializeArgs *args) override {
        bool isGms = false;

        auto process = env->GetStringUTFChars(args->nice_name, nullptr);
        if (process != nullptr) {
            isGms = strncmp(process, "com.google.android.gms", 22) == 0;
            isGmsUnstable = strcmp(process, "com.google.android.gms.unstable") == 0;
        }
        env->ReleaseStringUTFChars(args->nice_name, process);

        if (isGms) api->setOption(zygisk::FORCE_DENYLIST_UNMOUNT);

        if (!isGmsUnstable) api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
    }

    void postAppSpecialize(const zygisk::AppSpecializeArgs *args) override {
        if (!isGmsUnstable) return;

        doHook();
        injectDex();
    }

    void preServerSpecialize(zygisk::ServerSpecializeArgs *args) override {
        api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
    }

private:
    zygisk::Api *api = nullptr;
    JNIEnv *env = nullptr;
    bool isGmsUnstable = false;

    void injectDex() {
        LOGD("get system classloader");
        auto clClass = env->FindClass("java/lang/ClassLoader");
        auto getSystemClassLoader = env->GetStaticMethodID(clClass, "getSystemClassLoader",
                                                           "()Ljava/lang/ClassLoader;");
        auto systemClassLoader = env->CallStaticObjectMethod(clClass, getSystemClassLoader);

        LOGD("create buffer");
        auto buf = env->NewDirectByteBuffer(classes_dex, classes_dex_len);
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

        LOGD("clean");
        env->DeleteLocalRef(clClass);
        env->DeleteLocalRef(systemClassLoader);
        env->DeleteLocalRef(buf);
        env->DeleteLocalRef(dexClClass);
        env->DeleteLocalRef(dexCl);
        env->DeleteLocalRef(entryClassName);
        env->DeleteLocalRef(entryClassObj);
    }
};

REGISTER_ZYGISK_MODULE(PlayIntegrityFix)