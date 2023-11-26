#include <android/log.h>
#include <sys/system_properties.h>
#include <unistd.h>

#include "zygisk.hpp"
#include "shadowhook.h"
#include "json.hpp"

#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, "PIF/Native", __VA_ARGS__)

#define DEX_FILE_PATH "/data/adb/modules/playintegrityfix/classes.dex"

#define JSON_FILE_PATH "/data/adb/modules/playintegrityfix/pif.json"

static std::string FIRST_API_LEVEL, SECURITY_PATCH;

typedef void (*T_Callback)(void *, const char *, const char *, uint32_t);

static std::map<void *, T_Callback> callbacks;

static void modify_callback(void *cookie, const char *name, const char *value, uint32_t serial) {

    if (cookie == nullptr || name == nullptr || value == nullptr ||
        !callbacks.contains(cookie))
        return;

    std::string_view prop(name);

    if (prop.ends_with("api_level")) {
        if (FIRST_API_LEVEL == "NULL") {
            value = nullptr;
        } else {
            value = FIRST_API_LEVEL.c_str();
        }
        LOGD("[%s] -> %s", name, value);
    } else if (prop.ends_with("security_patch")) {
        if (SECURITY_PATCH == "NULL") {
            value = nullptr;
        } else {
            value = SECURITY_PATCH.c_str();
        }
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
        bool isGms = false;
        bool isGmsUnstable = false;

        auto rawProcess = env->GetStringUTFChars(args->nice_name, nullptr);
        if (rawProcess) {
            std::string_view process(rawProcess);
            isGms = process.starts_with("com.google.android.gms");
            isGmsUnstable = process.compare("com.google.android.gms.unstable") == 0;
        }
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

        long size = 0;
        int fd = api->connectCompanion();

        read(fd, &size, sizeof(long));

        if (size < 1) {
            close(fd);
            LOGD("Couldn't read from file descriptor 'classes.dex' file!");
            api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
            return;
        }

        dexVector.resize(size);

        read(fd, dexVector.data(), size);

        size = 0;

        read(fd, &size, sizeof(long));

        if (size < 1) {
            close(fd);
            LOGD("Couldn't read from file descriptor 'pif.json' file!");
            api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
            return;
        }

        propVector.resize(size);

        read(fd, propVector.data(), size);

        close(fd);

        LOGD("Read from file descriptor file 'classes.dex' -> %d bytes",
             static_cast<int>(dexVector.size()));
        LOGD("Read from file descriptor file 'pif.json' -> %d bytes",
             static_cast<int>(propVector.size()));
    }

    void postAppSpecialize(const zygisk::AppSpecializeArgs *args) override {
        if (dexVector.empty() || propVector.empty()) return;

        readJson();

        inject();

        doHook();

        dexVector.clear();
        propVector.clear();
    }

    void preServerSpecialize(zygisk::ServerSpecializeArgs *args) override {
        api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
    }

private:
    zygisk::Api *api = nullptr;
    JNIEnv *env = nullptr;
    std::vector<char> dexVector, propVector;

    void readJson() {
        std::string data(propVector.cbegin(), propVector.cend());
        nlohmann::json json = nlohmann::json::parse(data, nullptr, false, true);

        auto getStringFromJson = [&json](const std::string &key) {
            return json.contains(key) && !json[key].is_null() ? json[key].get<std::string>()
                                                              : "NULL";
        };

        SECURITY_PATCH = getStringFromJson("SECURITY_PATCH");
        FIRST_API_LEVEL = getStringFromJson("FIRST_API_LEVEL");
    }

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
        auto buffer = env->NewDirectByteBuffer(dexVector.data(),
                                               static_cast<jlong>(dexVector.size()));
        auto dexCl = env->NewObject(dexClClass, dexClInit, buffer, systemClassLoader);

        LOGD("load class");
        auto loadClass = env->GetMethodID(clClass, "loadClass",
                                          "(Ljava/lang/String;)Ljava/lang/Class;");
        auto entryClassName = env->NewStringUTF("es.chiteroman.playintegrityfix.EntryPoint");
        auto entryClassObj = env->CallObjectMethod(dexCl, loadClass, entryClassName);

        auto entryClass = (jclass) entryClassObj;

        LOGD("read json");
        auto readProps = env->GetStaticMethodID(entryClass, "readJson",
                                                "(Ljava/lang/String;)V");
        std::string data(propVector.cbegin(), propVector.cend());
        auto javaStr = env->NewStringUTF(data.c_str());
        env->CallStaticVoidMethod(entryClass, readProps, javaStr);

        LOGD("call init");
        auto entryInit = env->GetStaticMethodID(entryClass, "init", "()V");
        env->CallStaticVoidMethod(entryClass, entryInit);
    }
};

static void companion(int fd) {
    FILE *dex = fopen(DEX_FILE_PATH, "rb");

    fseek(dex, 0, SEEK_END);
    long dexSize = ftell(dex);
    fseek(dex, 0, SEEK_SET);

    char dexBuffer[dexSize];
    fread(dexBuffer, 1, dexSize, dex);

    fclose(dex);

    FILE *json = fopen(JSON_FILE_PATH, "r");

    fseek(json, 0, SEEK_END);
    long jsonSize = ftell(json);
    fseek(json, 0, SEEK_SET);

    char jsonBuffer[jsonSize];
    fread(jsonBuffer, 1, jsonSize, json);

    fclose(json);

    dexBuffer[dexSize] = 0;
    jsonBuffer[jsonSize] = 0;

    write(fd, &dexSize, sizeof(long));
    write(fd, dexBuffer, dexSize);

    write(fd, &jsonSize, sizeof(long));
    write(fd, jsonBuffer, jsonSize);
}


REGISTER_ZYGISK_MODULE(PlayIntegrityFix)

REGISTER_ZYGISK_COMPANION(companion)