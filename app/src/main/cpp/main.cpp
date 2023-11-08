#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <android/log.h>
#include <sys/system_properties.h>

#include "zygisk.hpp"

#if defined(__arm__)

#include "shadowhook.h"

#elif defined(__aarch64__)

#include "shadowhook.h"

#elif defined(__i386__)

#include "dobby.h"

#elif defined(__x86_64__)

#include "dobby.h"

#endif

#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, "PIF/Native", __VA_ARGS__)

void (*o_callback)(void *, const char *, const char *, uint32_t);

static void modify_callback(void *cookie, const char *name, const char *value, uint32_t serial) {

    if (strcmp(name, "ro.product.first_api_level") == 0) value = "24";

    if (strncmp(name, "cache", 5) != 0) LOGD("[%s] -> %s | (%p) (%d)", name, value, cookie, serial);

    return o_callback(cookie, name, value, serial);
}

static void (*o_system_property_read_callback)(const prop_info *,
                                               void (*)(void *, const char *,
                                                        const char *, uint32_t),
                                               void *);

static void my_system_property_read_callback(const prop_info *pi,
                                             void (*callback)(void *cookie, const char *name,
                                                              const char *value, uint32_t serial),
                                             void *cookie) {
    o_callback = callback;
    return o_system_property_read_callback(pi, modify_callback, cookie);
}

static void doHook() {
    LOGD("Starting to hook...");
    void *handle;

#if defined(__arm__)
    shadowhook_init(SHADOWHOOK_MODE_UNIQUE, true);
    handle = shadowhook_hook_sym_name("libc.so", "__system_property_read_callback",
                                      (void *) my_system_property_read_callback,
                                      (void **) &o_system_property_read_callback);
#elif defined(__aarch64__)
    shadowhook_init(SHADOWHOOK_MODE_UNIQUE, true);
    handle = shadowhook_hook_sym_name("libc.so", "__system_property_read_callback",
                                      (void *) my_system_property_read_callback,
                                      (void **) &o_system_property_read_callback);
#elif defined(__i386__)
    handle = DobbySymbolResolver(nullptr, "__system_property_read_callback");
#elif defined(__x86_64__)
    handle = DobbySymbolResolver(nullptr, "__system_property_read_callback");
#endif

    if (handle == nullptr) {
        LOGD("Couldn't get __system_property_read_callback handle.");
    } else {
        LOGD("Got __system_property_read_callback handle and hooked it at %p", handle);
    }

#if defined(__i386__)
    DobbyHook(handle, (void *) my_system_property_read_callback,
              (void **) &o_system_property_read_callback);
#elif defined(__x86_64__)
    DobbyHook(handle, (void *) my_system_property_read_callback,
              (void **) &o_system_property_read_callback);
#endif
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

        auto process = env->GetStringUTFChars(args->nice_name, nullptr);

        if (process != nullptr) {
            isGms = strncmp(process, "com.google.android.gms", 22) == 0;
            isGmsUnstable = strcmp(process, "com.google.android.gms.unstable") == 0;
        }

        env->ReleaseStringUTFChars(args->nice_name, process);

        if (!isGms) {
            api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
            return;
        }

        api->setOption(zygisk::FORCE_DENYLIST_UNMOUNT);

        if (!isGmsUnstable) {
            api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
            return;
        }

        auto fd = api->connectCompanion();

        read(fd, &moduleDexSize, sizeof(moduleDexSize));

        moduleDex = static_cast<char *>(malloc(moduleDexSize));

        read(fd, moduleDex, moduleDexSize);

        close(fd);

        moduleDex[moduleDexSize] = 0;
    }

    void postAppSpecialize(const zygisk::AppSpecializeArgs *args) override {
        if (moduleDex == nullptr || moduleDexSize == 0) return;
        doHook();
        injectDex();
    }

    void preServerSpecialize(zygisk::ServerSpecializeArgs *args) override {
        api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
    }

private:
    zygisk::Api *api = nullptr;
    JNIEnv *env = nullptr;
    char *moduleDex = nullptr;
    long moduleDexSize = 0;

    void injectDex() {
        LOGD("get system classloader");
        auto clClass = env->FindClass("java/lang/ClassLoader");
        auto getSystemClassLoader = env->GetStaticMethodID(clClass, "getSystemClassLoader",
                                                           "()Ljava/lang/ClassLoader;");
        auto systemClassLoader = env->CallStaticObjectMethod(clClass, getSystemClassLoader);

        LOGD("create buffer");
        auto buf = env->NewDirectByteBuffer(moduleDex, moduleDexSize);
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
        free(moduleDex);
        env->DeleteLocalRef(clClass);
        env->DeleteLocalRef(systemClassLoader);
        env->DeleteLocalRef(buf);
        env->DeleteLocalRef(dexClClass);
        env->DeleteLocalRef(dexCl);
        env->DeleteLocalRef(entryClassName);
        env->DeleteLocalRef(entryClassObj);
        env->DeleteLocalRef(entryClass);
    }
};

static void companion(int fd) {
    FILE *file = fopen("/data/adb/modules/playintegrityfix/classes.dex", "rb");

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char buffer[size];
    fread(buffer, size, 1, file);
    fclose(file);

    buffer[size] = 0;

    write(fd, &size, sizeof(size));
    write(fd, buffer, size);

    close(fd);
}

REGISTER_ZYGISK_MODULE(PlayIntegrityFix)

REGISTER_ZYGISK_COMPANION(companion)