#include <android/log.h>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/system_properties.h>
#include <string_view>
#include <vector>
#include <map>

#include "zygisk.hpp"
#include "dobby.h"

#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, "SNFix/JNI", __VA_ARGS__)

typedef void (*T_Callback)(void *, const char *, const char *, uint32_t);

std::map<void *, T_Callback> map;

static void
handle_system_property(void *cookie, const char *name, const char *value, uint32_t serial) {

    std::string_view prop(name);

    if (prop.compare("ro.product.first_api_level") == 0) value = "25";
    else if (prop.compare("ro.boot.verifiedbootstate") == 0) value = "green";
    else if (prop.compare("ro.secure") == 0 || prop.compare("ro.boot.flash.locked") == 0)
        value = "1";
    else if (prop.compare("ro.debuggable") == 0) value = "0";
    else if (prop.compare("ro.boot.vbmeta.device_state") == 0) value = "locked";
    else if (prop.compare("sys.usb.state") == 0) value = "none";

    if (!prop.starts_with("cache")) LOGD("[%s] -> %s", name, value);

    map[cookie](cookie, name, value, serial);
}

static void (*o_hook)(const prop_info *, T_Callback, void *);

static void my_hook(const prop_info *pi, T_Callback callback, void *cookie) {
    if (cookie == nullptr) {
        o_hook(pi, callback, cookie);
        return;
    }
    map[cookie] = callback;
    o_hook(pi, handle_system_property, cookie);
}

static void createHook() {
    LOGD("Trying to get __system_property_read_callback handle...");
    void *handle = DobbySymbolResolver(nullptr, "__system_property_read_callback");
    if (handle == nullptr) {
        LOGD("Couldn't get __system_property_read_callback, report to @chiteroman");
        return;
    }
    DobbyHook(handle, (void *) my_hook, (void **) &o_hook);
    LOGD("Hooked __system_property_read_callback at %p", handle);
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
        bool isGmsUnstable = process.compare("com.google.android.gms.unstable") == 0;

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

        auto fd = api->connectCompanion();

        long size = -1;
        read(fd, &size, sizeof(size));

        char buffer[size];
        read(fd, buffer, size);

        close(fd);

        buffer[size] = 0;

        moduleDex.insert(moduleDex.end(), buffer, buffer + size);
    }

    void postAppSpecialize(const zygisk::AppSpecializeArgs *args) override {
        if (moduleDex.empty()) return;

        createHook();
        injectDex();
    }

    void preServerSpecialize(zygisk::ServerSpecializeArgs *args) override {
        api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
    }

private:
    zygisk::Api *api = nullptr;
    JNIEnv *env = nullptr;
    std::vector<char> moduleDex;

    void injectDex() {
        LOGD("get system classloader");
        auto clClass = env->FindClass("java/lang/ClassLoader");
        auto getSystemClassLoader = env->GetStaticMethodID(clClass, "getSystemClassLoader",
                                                           "()Ljava/lang/ClassLoader;");
        auto systemClassLoader = env->CallStaticObjectMethod(clClass, getSystemClassLoader);

        LOGD("create buffer");
        auto buf = env->NewDirectByteBuffer(moduleDex.data(), moduleDex.size());
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
        moduleDex.clear();
        moduleDex.shrink_to_fit();
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