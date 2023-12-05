#include <android/log.h>
#include <sys/system_properties.h>
#include <unistd.h>
#include <string_view>

#include "zygisk.hpp"
#include "shadowhook.h"

#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, "PIF/Native", __VA_ARGS__)

typedef void (*T_Callback)(void *, const char *, const char *, uint32_t);

T_Callback o_callback = nullptr;

void modify_callback(void *cookie, const char *name, const char *value, uint32_t serial) {

    if (cookie == nullptr || name == nullptr || value == nullptr || o_callback == nullptr) return;

    std::string_view prop(name);

    if (prop.ends_with("api_level")) {
        value = "21";
    } else if (prop.ends_with("security_patch")) {
        value = "2020-05-05";
    } else if (prop == "ro.build.id") {
        value = "QQ2A.200501.001.B3";
    }

    if (!prop.starts_with("cache") && !prop.starts_with("debug")) LOGD("[%s]: %s", name, value);

    return o_callback(cookie, name, value, serial);
}

void (*o_system_property_read_callback)(const prop_info *, T_Callback, void *);

void my_system_property_read_callback(const prop_info *pi, T_Callback callback, void *cookie) {
    if (pi == nullptr || callback == nullptr || cookie == nullptr) {
        return o_system_property_read_callback(pi, callback, cookie);
    }
    o_callback = callback;
    return o_system_property_read_callback(pi, modify_callback, cookie);
}

void doHook() {
    shadowhook_init(SHADOWHOOK_MODE_UNIQUE, false);
    void *handle = shadowhook_hook_sym_name("libc.so", "__system_property_read_callback",
                                            reinterpret_cast<void *>(my_system_property_read_callback),
                                            reinterpret_cast<void **>(&o_system_property_read_callback));
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
        auto rawProcess = env->GetStringUTFChars(args->nice_name, nullptr);

        std::string_view process(rawProcess);

        if (process.starts_with("com.google.android.gms")) {

            api->setOption(zygisk::FORCE_DENYLIST_UNMOUNT);

            if (process == "com.google.android.gms.unstable") {

                int fd = api->connectCompanion();

                read(fd, &bufferSize, sizeof(int));

                if (bufferSize > 0) {
                    buffer = static_cast<char *>(calloc(1, bufferSize));
                    read(fd, buffer, bufferSize);
                } else {
                    api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
                    LOGD("Couldn't read classes.dex");
                }

                close(fd);

            } else api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);

        } else api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);

        env->ReleaseStringUTFChars(args->nice_name, rawProcess);
    }

    void postAppSpecialize(const zygisk::AppSpecializeArgs *args) override {
        if (bufferSize < 1 || buffer == nullptr) return;

        LOGD("Read from fd: %d bytes!", bufferSize);

        doHook();

        inject();

        free(buffer);
    }

    void preServerSpecialize(zygisk::ServerSpecializeArgs *args) override {
        api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
    }

private:
    zygisk::Api *api = nullptr;
    JNIEnv *env = nullptr;
    char *buffer = nullptr;
    int bufferSize = 0;

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
        auto buff = env->NewDirectByteBuffer(buffer, bufferSize);
        auto dexCl = env->NewObject(dexClClass, dexClInit, buff, systemClassLoader);

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
    int dexSize = 0;
    char *buffer = nullptr;

    FILE *dex = fopen("/data/adb/modules/playintegrityfix/classes.dex", "rb");

    if (dex) {
        fseek(dex, 0, SEEK_END);
        dexSize = static_cast<int>(ftell(dex));
        fseek(dex, 0, SEEK_SET);

        buffer = static_cast<char *>(calloc(1, dexSize));
        fread(buffer, 1, dexSize, dex);

        fclose(dex);
    }

    write(fd, &dexSize, sizeof(int));
    write(fd, buffer, dexSize);

    free(buffer);
}

REGISTER_ZYGISK_MODULE(PlayIntegrityFix)

REGISTER_ZYGISK_COMPANION(companion)