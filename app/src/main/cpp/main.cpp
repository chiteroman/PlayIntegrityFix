#include <android/log.h>
#include <sys/system_properties.h>
#include <unistd.h>
#include "dobby.h"
#include "json.hpp"
#include "zygisk.hpp"

#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, "PIF", __VA_ARGS__)

#define CLASSES_DEX "/data/adb/modules/playintegrityfix/classes.dex"

#define PIF_JSON "/data/adb/pif.json"

#define PIF_JSON_DEFAULT "/data/adb/modules/playintegrityfix/pif.json"

static std::string DEVICE_INITIAL_SDK_INT, SECURITY_PATCH, ID;

typedef void (*T_Callback)(void *, const char *, const char *, uint32_t);

static T_Callback o_callback = nullptr;

static void modify_callback(void *cookie, const char *name, const char *value, uint32_t serial) {

    if (cookie == nullptr || name == nullptr || value == nullptr || o_callback == nullptr) return;

    std::string_view prop(name);

    if (prop.ends_with("api_level") && !DEVICE_INITIAL_SDK_INT.empty()) {
        value = DEVICE_INITIAL_SDK_INT.c_str();
    } else if (prop.ends_with(".security_patch") && !SECURITY_PATCH.empty()) {
        value = SECURITY_PATCH.c_str();
    } else if (prop.ends_with(".id") && !ID.empty()) {
        value = ID.c_str();
    } else if (prop == "sys.usb.state") {
        value = "none";
    }

    if (!prop.starts_with("persist") && !prop.starts_with("cache") && !prop.starts_with("debug")) {
        LOGD("[%s]: %s", name, value);
    }

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
    void *handle = DobbySymbolResolver(nullptr, "__system_property_read_callback");
    if (handle == nullptr) {
        LOGD("Couldn't hook __system_property_read_callback");
        return;
    }
    DobbyHook(handle, (void *) my_system_property_read_callback,
              (void **) &o_system_property_read_callback);
    LOGD("Found and hooked __system_property_read_callback at %p", handle);
}

class PlayIntegrityFix : public zygisk::ModuleBase {
public:
    void onLoad(zygisk::Api *api, JNIEnv *env) override {
        this->api = api;
        this->env = env;
    }

    void preAppSpecialize(zygisk::AppSpecializeArgs *args) override {

        if (!args) {
            api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
            return;
        }

        const char *rawDir = env->GetStringUTFChars(args->app_data_dir, nullptr);

        if (!rawDir) {
            api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
            return;
        }

        const char *rawName = env->GetStringUTFChars(args->nice_name, nullptr);

        if (!rawName) {
            env->ReleaseStringUTFChars(args->app_data_dir, rawDir);
            api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
            return;
        }

        std::string dir(rawDir);
        std::string name(rawName);

        env->ReleaseStringUTFChars(args->app_data_dir, rawDir);
        env->ReleaseStringUTFChars(args->nice_name, rawName);

        if (!dir.ends_with("/com.google.android.gms")) {
            api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
            return;
        }

        api->setOption(zygisk::FORCE_DENYLIST_UNMOUNT);

        if (name != "com.google.android.gms.unstable") {
            api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
            return;
        }

        long dexSize = 0, jsonSize = 0;

        int fd = api->connectCompanion();

        read(fd, &dexSize, sizeof(long));
        read(fd, &jsonSize, sizeof(long));

        LOGD("Dex file size: %ld", dexSize);
        LOGD("Json file size: %ld", jsonSize);

        if (dexSize < 1 || jsonSize < 1) {
            close(fd);
            api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
            return;
        }

        dexVector.resize(dexSize);
        read(fd, dexVector.data(), dexSize);

        std::vector<uint8_t> jsonVector;

        jsonVector.resize(jsonSize);
        read(fd, jsonVector.data(), jsonSize);

        close(fd);

        json = nlohmann::json::parse(jsonVector, nullptr, false, true);
    }

    void postAppSpecialize(const zygisk::AppSpecializeArgs *args) override {
        if (dexVector.empty() || json.empty()) return;

        parseJson();

        injectDex();

        doHook();
    }

    void preServerSpecialize(zygisk::ServerSpecializeArgs *args) override {
        api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
    }

private:
    zygisk::Api *api = nullptr;
    JNIEnv *env = nullptr;
    std::vector<uint8_t> dexVector;
    nlohmann::json json;

    void parseJson() {
        if (json.contains("DEVICE_INITIAL_SDK_INT")) {
            DEVICE_INITIAL_SDK_INT = json["DEVICE_INITIAL_SDK_INT"].get<std::string>();
            json.erase("DEVICE_INITIAL_SDK_INT"); // You can't modify field value
        }
        if (json.contains("SECURITY_PATCH")) {
            SECURITY_PATCH = json["SECURITY_PATCH"].get<std::string>();
        }
        if (json.contains("ID")) {
            ID = json["ID"].get<std::string>();
        }
    }

    void injectDex() {
        LOGD("get system classloader");
        auto clClass = env->FindClass("java/lang/ClassLoader");
        auto getSystemClassLoader = env->GetStaticMethodID(clClass, "getSystemClassLoader",
                                                           "()Ljava/lang/ClassLoader;");
        auto systemClassLoader = env->CallStaticObjectMethod(clClass, getSystemClassLoader);

        LOGD("create class loader");
        auto dexClClass = env->FindClass("dalvik/system/InMemoryDexClassLoader");
        auto dexClInit = env->GetMethodID(dexClClass, "<init>",
                                          "(Ljava/nio/ByteBuffer;Ljava/lang/ClassLoader;)V");
        auto buffer = env->NewDirectByteBuffer(dexVector.data(), dexVector.size());
        auto dexCl = env->NewObject(dexClClass, dexClInit, buffer, systemClassLoader);

        LOGD("load class");
        auto loadClass = env->GetMethodID(clClass, "loadClass",
                                          "(Ljava/lang/String;)Ljava/lang/Class;");
        auto entryClassName = env->NewStringUTF("es.chiteroman.playintegrityfix.EntryPoint");
        auto entryClassObj = env->CallObjectMethod(dexCl, loadClass, entryClassName);

        auto entryPointClass = (jclass) entryClassObj;

        LOGD("call init");
        auto entryInit = env->GetStaticMethodID(entryPointClass, "init", "(Ljava/lang/String;)V");
        auto str = env->NewStringUTF(json.dump().c_str());
        env->CallStaticVoidMethod(entryPointClass, entryInit, str);
    }
};

static std::vector<uint8_t> readFile(const char *path) {

    std::vector<uint8_t> vector;

    FILE *file = fopen(path, "rb");

    if (file) {
        fseek(file, 0, SEEK_END);
        long size = ftell(file);
        fseek(file, 0, SEEK_SET);

        vector.resize(size);
        fread(vector.data(), 1, size, file);
        fclose(file);
    } else {
        LOGD("Couldn't read %s file!", path);
    }

    return vector;
}

static void companion(int fd) {

    std::vector<uint8_t> dexVector, jsonVector;

    dexVector = readFile(CLASSES_DEX);

    jsonVector = readFile(PIF_JSON);

    if (jsonVector.empty()) jsonVector = readFile(PIF_JSON_DEFAULT);

    long dexSize = dexVector.size();
    long jsonSize = jsonVector.size();

    write(fd, &dexSize, sizeof(long));
    write(fd, &jsonSize, sizeof(long));

    write(fd, dexVector.data(), dexSize);
    write(fd, jsonVector.data(), jsonSize);
}

REGISTER_ZYGISK_MODULE(PlayIntegrityFix)

REGISTER_ZYGISK_COMPANION(companion)