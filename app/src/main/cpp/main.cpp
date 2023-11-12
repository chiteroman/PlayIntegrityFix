#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <android/log.h>
#include <sys/system_properties.h>

#include "zygisk.hpp"
#include "dobby.h"

#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, "PIF/Native", __VA_ARGS__)

#define FIRST_API_LEVEL "25"

static void (*o_callback)(void *, const char *, const char *, uint32_t);

static void modify_callback(void *cookie, const char *name, const char *value, uint32_t serial) {

    if (o_callback == nullptr || cookie == nullptr || name == nullptr || value == nullptr) return;

    if (strcmp(name, "ro.product.first_api_level") == 0) {
        LOGD("[%s] Original: %s | Mod: %s", name, value, FIRST_API_LEVEL);
        return o_callback(cookie, name, FIRST_API_LEVEL, serial);
    }

    return o_callback(cookie, name, value, serial);
}

static void (*o_system_property_read_callback)(const prop_info *,
                                               void (*)(void *, const char *, const char *,
                                                        uint32_t), void *);

static void
my_system_property_read_callback(const prop_info *pi,
                                 void (*callback)(void *cookie, const char *name, const char *value,
                                                  uint32_t serial), void *cookie) {

    if (pi == nullptr || callback == nullptr || cookie == nullptr) {
        return o_system_property_read_callback(pi, callback, cookie);
    }
    o_callback = callback;
    return o_system_property_read_callback(pi, modify_callback, cookie);
}

static void doHook() {
    LOGD("Starting to hook...");
    void *handle = DobbySymbolResolver(nullptr, "__system_property_read_callback");

    if (handle == nullptr) {
        LOGD("Couldn't get __system_property_read_callback handle.");
        return;
    }

    DobbyHook(
            handle,
            reinterpret_cast<void *>(my_system_property_read_callback),
            reinterpret_cast<void **>(&o_system_property_read_callback)
    );

    LOGD("Got __system_property_read_callback handle and hooked it at %p", handle);
}

static bool dontInject() {
    char host[PROP_VALUE_MAX];
    if (__system_property_get("ro.build.host", host) < 1) return false;
    return strcmp(host, "xiaomi.eu") == 0 || strcmp(host, "EliteDevelopment") == 0;
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

        if (isGmsUnstable && !dontInject()) {

            int fd = api->connectCompanion();

            long size = 0;
            read(fd, &size, sizeof(size));

            if (size > 0) {

                moduleDex = static_cast<char *>(malloc(size));

                read(fd, moduleDex, size);

                close(fd);

                return;
            }

            close(fd);
        }

        api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
    }

    void postAppSpecialize(const zygisk::AppSpecializeArgs *args) override {
        if (!isGmsUnstable) return;

        doHook();

        if (moduleDex == nullptr) return;

        injectDex();
    }

    void preServerSpecialize(zygisk::ServerSpecializeArgs *args) override {
        api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
    }

private:
    zygisk::Api *api = nullptr;
    JNIEnv *env = nullptr;
    bool isGmsUnstable = false;
    char *moduleDex = nullptr;

    void injectDex() {
        LOGD("get system classloader");
        auto clClass = env->FindClass("java/lang/ClassLoader");
        auto getSystemClassLoader = env->GetStaticMethodID(clClass, "getSystemClassLoader",
                                                           "()Ljava/lang/ClassLoader;");
        auto systemClassLoader = env->CallStaticObjectMethod(clClass, getSystemClassLoader);

        LOGD("create buffer");
        auto buf = env->NewDirectByteBuffer(moduleDex, sizeof(moduleDex));
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
    }
};

static void companion(int fd) {
    FILE *file = fopen("/data/adb/modules/playintegrityfix/classes.dex", "rb");

    if (file == nullptr) {
        long i = 0;
        write(fd, &i, sizeof(i));
        return;
    }

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