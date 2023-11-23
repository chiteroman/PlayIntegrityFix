#include <android/log.h>
#include <sys/system_properties.h>
#include <unistd.h>
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <sstream>

#include "zygisk.hpp"
#include "shadowhook.h"

#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, "PIF/Native", __VA_ARGS__)

#define DEX_FILE_PATH "/data/adb/modules/playintegrityfix/classes.dex"

#define PROP_FILE_PATH "/data/adb/modules/playintegrityfix/pif.prop"

static std::string SECURITY_PATCH, FIRST_API_LEVEL;

typedef void (*T_Callback)(void *, const char *, const char *, uint32_t);

static T_Callback o_callback = nullptr;

static void modify_callback(void *cookie, const char *name, const char *value, uint32_t serial) {

    if (cookie == nullptr || name == nullptr || value == nullptr || o_callback == nullptr) return;

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
    shadowhook_init(SHADOWHOOK_MODE_UNIQUE, true);
    void *handle = shadowhook_hook_sym_name(
            "libc.so",
            "__system_property_read_callback",
            (void *) my_system_property_read_callback,
            (void **) &o_system_property_read_callback
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
        auto rawProcess = env->GetStringUTFChars(args->nice_name, nullptr);

        std::string_view process(rawProcess);

        bool isGms = process.starts_with("com.google.android.gms");
        isGmsUnstable = process.compare("com.google.android.gms.unstable") == 0;

        env->ReleaseStringUTFChars(args->nice_name, rawProcess);

        if (isGms) api->setOption(zygisk::FORCE_DENYLIST_UNMOUNT);

        if (!isGmsUnstable) {
            api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
            return;
        }

        int fd = api->connectCompanion();

        long size = 0;
        read(fd, &size, sizeof(size));

        if (size < 1) {
            close(fd);
            LOGD("Couldn't read classes.dex from socket! File exists?");
            return;
        }

        moduleDex.resize(size);
        read(fd, moduleDex.data(), size);

        int mapSize = 0;
        read(fd, &mapSize, sizeof(mapSize));

        if (mapSize < 1) {
            close(fd);
            SECURITY_PATCH = "2017-08-05";
            FIRST_API_LEVEL = "23";
            LOGD("Couldn't read pif.prop from socket! File exists?");
            return;
        }

        for (int i = 0; i < size; ++i) {
            int keyLenght, valueLenght;
            std::string key, value;

            read(fd, &keyLenght, sizeof(keyLenght));
            read(fd, &valueLenght, sizeof(valueLenght));

            key.resize(keyLenght);
            value.resize(valueLenght);

            read(fd, key.data(), keyLenght);
            read(fd, value.data(), valueLenght);

            props[key] = value;
        }

        LOGD("Received from socket %d props!", static_cast<int>(props.size()));

        for (const auto &item: props) {
            if (item.first == "SECURITY_PATCH") {
                SECURITY_PATCH = item.second;
            } else if (item.first == "FIRST_API_LEVEL") {
                FIRST_API_LEVEL = item.second;
            }
        }

        close(fd);
    }

    void postAppSpecialize(const zygisk::AppSpecializeArgs *args) override {
        if (!isGmsUnstable) return;

        doHook();

        inject();

        LOGD("clean");
        moduleDex.clear();
        moduleDex.shrink_to_fit();
        props.clear();
    }

    void preServerSpecialize(zygisk::ServerSpecializeArgs *args) override {
        api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
    }

private:
    zygisk::Api *api = nullptr;
    JNIEnv *env = nullptr;
    bool isGmsUnstable = false;
    std::map<std::string, std::string> props;
    std::vector<char> moduleDex;

    void inject() {
        if (moduleDex.empty()) {
            LOGD("Dex not loaded in memory");
            return;
        }

        LOGD("Preparing to inject %d bytes to the process", static_cast<int>(moduleDex.size()));

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

        auto entryClass = (jclass) entryClassObj;

        if (!props.empty()) {
            LOGD("call add prop");
            auto addProp = env->GetStaticMethodID(entryClass, "addProp",
                                                  "(Ljava/lang/String;Ljava/lang/String;)V");
            for (const auto &item: props) {
                auto key = env->NewStringUTF(item.first.c_str());
                auto value = env->NewStringUTF(item.second.c_str());
                env->CallStaticVoidMethod(entryClass, addProp, key, value);
            }
        }

        LOGD("call init");
        auto entryInit = env->GetStaticMethodID(entryClass, "init", "()V");
        env->CallStaticVoidMethod(entryClass, entryInit);
    }
};

static void companion(int fd) {
    std::ifstream dex(DEX_FILE_PATH, std::ios::binary | std::ios::ate);

    long size = static_cast<long>(dex.tellg());
    dex.seekg(std::ios::beg);

    char buffer[size];
    dex.read(buffer, size);

    dex.close();

    write(fd, &size, sizeof(size));
    write(fd, buffer, size);

    std::ifstream prop(PROP_FILE_PATH);

    if (prop.bad()) {
        prop.close();
        int i = 0;
        write(fd, &i, sizeof(i));
        return;
    }

    std::map<std::string, std::string> props;

    std::string line;
    while (std::getline(prop, line)) {
        std::istringstream iss(line);
        std::string key, value;

        if (std::getline(iss, key, '=') && std::getline(iss, value)) {
            props[key] = value;
        }
    }

    prop.close();

    int mapSize = static_cast<int>(props.size());

    write(fd, &mapSize, sizeof(mapSize));

    for (const auto &item: props) {
        int keyLenght = static_cast<int>(item.first.size());
        int valueLenght = static_cast<int>(item.second.size());

        write(fd, &keyLenght, sizeof(keyLenght));
        write(fd, &valueLenght, sizeof(valueLenght));

        write(fd, item.first.data(), keyLenght);
        write(fd, item.second.data(), valueLenght);
    }
}

REGISTER_ZYGISK_MODULE(PlayIntegrityFix)

REGISTER_ZYGISK_COMPANION(companion)