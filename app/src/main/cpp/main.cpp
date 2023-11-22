#include <android/log.h>
#include <sys/system_properties.h>
#include <unistd.h>
#include <string>
#include <vector>
#include <map>

#include "zygisk.hpp"
#include "dobby.h"

#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, "PIF/Native", __VA_ARGS__)

static std::string SECURITY_PATCH, FIRST_API_LEVEL;

#define DEX_FILE_PATH "/data/adb/modules/playintegrityfix/classes.dex"

#define PROP_FILE_PATH "/data/adb/modules/playintegrityfix/pif.prop"

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

        int mapSize;
        read(fd, &mapSize, sizeof(mapSize));

        for (int i = 0; i < mapSize; ++i) {
            int keyLenght, valueLenght;
            std::string key, value;

            read(fd, &keyLenght, sizeof(keyLenght));
            read(fd, &valueLenght, sizeof(valueLenght));

            key.resize(keyLenght);
            value.resize(valueLenght);

            read(fd, key.data(), keyLenght);
            read(fd, value.data(), valueLenght);

            props.insert({key, value});
        }

        long size;
        read(fd, &size, sizeof(size));

        char buffer[size];
        read(fd, buffer, size);

        close(fd);

        moduleDex.insert(moduleDex.end(), buffer, buffer + size);

        LOGD("Received from socket %d props!", static_cast<int>(props.size()));

        for (const auto &item: props) {
            if (item.first == "SECURITY_PATCH") {
                SECURITY_PATCH = item.second;
            } else if (item.first == "FIRST_API_LEVEL") {
                FIRST_API_LEVEL = item.second;
            }
        }
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

        if (props.empty()) {
            LOGD("No props loaded in memory");
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

        LOGD("call add prop");
        auto addProp = env->GetStaticMethodID(entryClass, "addProp",
                                              "(Ljava/lang/String;Ljava/lang/String;)V");
        for (const auto &item: props) {
            auto key = env->NewStringUTF(item.first.c_str());
            auto value = env->NewStringUTF(item.second.c_str());
            env->CallStaticVoidMethod(entryClass, addProp, key, value);
        }

        LOGD("call init");
        auto entryInit = env->GetStaticMethodID(entryClass, "init", "()V");
        env->CallStaticVoidMethod(entryClass, entryInit);
    }
};

static void parsePropsFile(int fd) {
    LOGD("Proceed to parse '%s' file", PROP_FILE_PATH);

    std::map<std::string, std::string> props;

    FILE *file = fopen(PROP_FILE_PATH, "r");

    char line[256];

    while (fgets(line, sizeof(line), file)) {

        std::string key, value;

        char *data = strtok(line, "=");

        while (data) {
            if (key.empty()) {
                key = data;
            } else {
                value = data;
            }
            data = strtok(nullptr, "=");
        }

        key.erase(std::remove_if(key.begin(), key.end(),
                                 [](unsigned char x) { return std::isspace(x); }), key.end());
        value.erase(std::remove(value.begin(), value.end(), '\n'), value.cend());

        props.insert({key, value});

        key.clear();
        key.shrink_to_fit();

        value.clear();
        value.shrink_to_fit();
    }

    fclose(file);

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

    props.clear();
}

static void companion(int fd) {
    parsePropsFile(fd);

    FILE *dex = fopen(DEX_FILE_PATH, "rb");

    fseek(dex, 0, SEEK_END);
    long size = ftell(dex);
    fseek(dex, 0, SEEK_SET);

    char buffer[size];
    fread(buffer, 1, size, dex);

    fclose(dex);

    buffer[size] = '\0';

    write(fd, &size, sizeof(size));
    write(fd, buffer, size);
}

REGISTER_ZYGISK_MODULE(PlayIntegrityFix)

REGISTER_ZYGISK_COMPANION(companion)