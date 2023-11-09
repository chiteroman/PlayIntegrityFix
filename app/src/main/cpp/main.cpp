#include <unistd.h>
#include <android/log.h>
#include <sys/system_properties.h>
#include <vector>
#include <string>
#include <map>
#include <fstream>

#include "zygisk.hpp"
#include "dobby.h"

#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, "PIF/Native", __VA_ARGS__)

inline static const std::map<std::string, std::string> PROPS_MAP = {
        {"ro.product.first_api_level",  "24"},
        {"ro.secure",                   "1"},
        {"ro.debuggable",               "0"},
        {"sys.usb.state",               "none"},
        {"ro.boot.verifiedbootstate",   "green"},
        {"ro.boot.flash.locked",        "1"},
        {"ro.boot.vbmeta.device_state", "locked"}
};

typedef void (*T_Callback)(void *, const char *, const char *, uint32_t);

static std::map<void *, T_Callback> map;

static void modify_callback(void *cookie, const char *name, const char *value, uint32_t serial) {

    if (name != nullptr) {
        std::string prop(name);

        if (PROPS_MAP.contains(prop)) value = PROPS_MAP.at(prop).c_str();

        if (!prop.starts_with("cache")) LOGD("[%s] -> %s", name, value);

        prop.clear();
        prop.shrink_to_fit();
    }

    return map[cookie](cookie, name, value, serial);
}

static void (*o_system_property_read_callback)(const prop_info *,
                                               T_Callback,
                                               void *);

static void my_system_property_read_callback(const prop_info *pi,
                                             T_Callback callback,
                                             void *cookie) {

    if (pi == nullptr || callback == nullptr || cookie == nullptr) {
        return o_system_property_read_callback(pi, callback, cookie);
    }
    map[cookie] = callback;
    return o_system_property_read_callback(pi, modify_callback, cookie);
}

static void doHook() {
    LOGD("Starting to hook...");
    auto handle = DobbySymbolResolver(nullptr, "__system_property_read_callback");

    if (handle == nullptr) {
        LOGD("Couldn't get __system_property_read_callback handle.");
        return;
    } else {
        LOGD("Got __system_property_read_callback handle and hooked it at %p", handle);
    }

    DobbyHook(handle, (void *) my_system_property_read_callback,
              (void **) &o_system_property_read_callback);
}

class PlayIntegrityFix : public zygisk::ModuleBase {
public:
    void onLoad(zygisk::Api *api, JNIEnv *env) override {
        this->api = api;
        this->env = env;
    }

    void preAppSpecialize(zygisk::AppSpecializeArgs *args) override {
        auto rawProcess = env->GetStringUTFChars(args->nice_name, nullptr);
        std::string process(rawProcess);
        env->ReleaseStringUTFChars(args->nice_name, rawProcess);

        if (process.starts_with("com.google.android.gms")) {

            api->setOption(zygisk::FORCE_DENYLIST_UNMOUNT);

            if (process == "com.google.android.gms.unstable") {
                isGmsUnstable = true;

                int fd = api->connectCompanion();

                long size;
                read(fd, &size, sizeof(size));

                if (size > 0) {

                    LOGD("Received %ld bytes from socket", size);

                    char buffer[size];
                    read(fd, buffer, size);
                    buffer[size] = 0;

                    moduleDex.insert(moduleDex.end(), buffer, buffer + size);

                } else {
                    LOGD("Received invalid bytes from socket. Does classes.dex file exist?");
                }

                close(fd);
            }
        }

        process.clear();
        process.shrink_to_fit();

        if (!isGmsUnstable) api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
    }

    void postAppSpecialize(const zygisk::AppSpecializeArgs *args) override {
        if (!isGmsUnstable) return;

        doHook();

        if (!moduleDex.empty()) injectDex();
    }

    void preServerSpecialize(zygisk::ServerSpecializeArgs *args) override {
        api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
    }

private:
    zygisk::Api *api = nullptr;
    JNIEnv *env = nullptr;
    bool isGmsUnstable = false;
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
    std::ifstream ifs("/data/adb/modules/playintegrityfix/classes.dex",
                      std::ios::binary | std::ios::ate);

    if (ifs.bad()) {
        long i = -1;
        write(fd, &i, sizeof(i));
        close(fd);
        return;
    }

    long size = ifs.tellg();
    ifs.seekg(std::ios::beg);

    char buffer[size];
    ifs.read(buffer, size);
    buffer[size] = 0;

    ifs.close();

    write(fd, &size, sizeof(size));
    write(fd, buffer, size);

    close(fd);
}

REGISTER_ZYGISK_MODULE(PlayIntegrityFix)

REGISTER_ZYGISK_COMPANION(companion)