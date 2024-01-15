#include <android/log.h>
#include <sys/system_properties.h>
#include <unistd.h>
#include <mutex>
#include "zygisk.hpp"
#include "dobby.h"
#include "json.hpp"

#define LOG_TAG "PIF/Native"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

const char* const CLASSES_DEX = "/data/adb/modules/playintegrityfix/classes.dex";
const char* const PIF_JSON = "/data/adb/pif.json";
const char* const PIF_JSON_2 = "/data/adb/modules/playintegrityfix/pif.json";

static std::string FIRST_API_LEVEL = "";
static std::string SECURITY_PATCH = "";
static std::string BUILD_ID = "";

typedef void (*T_Callback)(void *, const char *, const char *, uint32_t);

static std::map<void *, T_Callback> callbacks;
static std::mutex callbacks_mutex;

static void modify_callback(void *cookie, const char *name, const char *value, uint32_t serial) {
    if (cookie == nullptr || name == nullptr || value == nullptr) {
        return;
    }

    std::lock_guard<std::mutex> lock(callbacks_mutex);
    if (!callbacks.contains(cookie)) {
        return;
    }

    std::string_view prop(name);

    if (prop.ends_with("security_patch") && !SECURITY_PATCH.empty()) {
        value = SECURITY_PATCH.c_str();
    } else if (prop.ends_with("api_level") && !FIRST_API_LEVEL.empty()) {
        value = FIRST_API_LEVEL.c_str();
    } else if (prop.ends_with("build.id") && !BUILD_ID.empty()) {
        value = BUILD_ID.c_str();
    } else if (prop == "sys.usb.state") {
        value = "none";
    }

    if (!prop.starts_with("debug") && !prop.starts_with("cache") && !prop.starts_with("persist")) {
        LOGD("[%s] -> %s", name, value);
    }

    callbacks[cookie](cookie, name, value, serial);
}

static void (*o_system_property_read_callback)(const prop_info *, T_Callback, void *);

static void my_system_property_read_callback(const prop_info *pi, T_Callback callback, void *cookie) {
    if (pi == nullptr || callback == nullptr || cookie == nullptr) {
        return o_system_property_read_callback(pi, callback, cookie);
    }
    {
        std::lock_guard<std::mutex> lock(callbacks_mutex);
        callbacks[cookie] = callback;
    }
    o_system_property_read_callback(pi, modify_callback, cookie);
}

static void doHook() {
    void *handle = DobbySymbolResolver("libc.so", "__system_property_read_callback");
    if (handle == nullptr) {
        LOGD("Couldn't find '__system_property_read_callback' handle. Report to @chiteroman");
        return;
    }
    LOGD("Found '__system_property_read_callback' handle at %p", handle);
    DobbyHook(
        handle,
        reinterpret_cast<dobby_dummy_func_t>(my_system_property_read_callback),
        reinterpret_cast<dobby_dummy_func_t *>(&o_system_property_read_callback)
    );
}

class PlayIntegrityFix : public zygisk::ModuleBase {
public:
    void onLoad(zygisk::Api *api, JNIEnv *env) override {
        this->api = api;
        this->env = env;
    }

    void preAppSpecialize(zygisk::AppSpecializeArgs *args) override {
        const char* const GMS_PACKAGE_NAME = "com.google.android.gms";
        const char* const GMS_UNSTABLE_PACKAGE_NAME = "com.google.android.gms.unstable";
        const size_t GMS_PACKAGE_NAME_LENGTH = 22;

        auto name = env->GetStringUTFChars(args->nice_name, nullptr);

        if (name == nullptr) {
            api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
            return;
        }

        if (strncmp(name, GMS_PACKAGE_NAME, GMS_PACKAGE_NAME_LENGTH) != 0) {
            env->ReleaseStringUTFChars(args->nice_name, name);
            api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
            return;
        }

        api->setOption(zygisk::FORCE_DENYLIST_UNMOUNT);

        if (strcmp(name, GMS_UNSTABLE_PACKAGE_NAME) != 0) {
            env->ReleaseStringUTFChars(args->nice_name, name);
            api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
            return;
        }

        env->ReleaseStringUTFChars(args->nice_name, name);

        long dexSize = 0, jsonSize = 0;

        int fd = api->connectCompanion();

        if (read(fd, &dexSize, sizeof(long)) != sizeof(long) || read(fd, &jsonSize, sizeof(long)) != sizeof(long)) {
            LOGD("Error reading dex or json size");
            close(fd);
            return;
        }

        LOGD("Dex file size: %ld", dexSize);
        LOGD("Json file size: %ld", jsonSize);

        vector.resize(dexSize);
        if (read(fd, vector.data(), dexSize) != dexSize) {
            LOGD("Error reading dex data");
            close(fd);
            return;
        }

        std::vector<char> jsonVector(jsonSize);
        if (read(fd, jsonVector.data(), jsonSize) != jsonSize) {
            LOGD("Error reading json data");
            close(fd);
            return;
        }

        close(fd);

        std::string_view jsonStr(jsonVector.cbegin(), jsonVector.cend());
        json = nlohmann::json::parse(jsonStr, nullptr, false, true);

        if (json.contains("FIRST_API_LEVEL")) {
            if (json["FIRST_API_LEVEL"].is_number_integer()) {
                FIRST_API_LEVEL = std::to_string(json["FIRST_API_LEVEL"].get<int>());
            } else if (json["FIRST_API_LEVEL"].is_string()) {
                FIRST_API_LEVEL = json["FIRST_API_LEVEL"].get<std::string>();
            }
            json.erase("FIRST_API_LEVEL");
        } else {
            LOGD("JSON file doesn't contain FIRST_API_LEVEL key :(");
        }

        if (json.contains("SECURITY_PATCH") && json["SECURITY_PATCH"].is_string()) {
            SECURITY_PATCH = json["SECURITY_PATCH"].get<std::string>();
        } else {
            LOGD("JSON file doesn't contain SECURITY_PATCH key :(");
        }

        if (json.contains("ID") && json["ID"].is_string()) {
            BUILD_ID = json["ID"].get<std::string>();
        } else if (json.contains("BUILD_ID") && json["BUILD_ID"].is_string()) {
            BUILD_ID = json["BUILD_ID"].get<std::string>();
        } else {
            LOGD("JSON file doesn't contain ID/BUILD_ID keys :(");
        }
    }

    void postAppSpecialize(const zygisk::AppSpecializeArgs *args) override {
        if (vector.empty() || json.empty()) return;

        doHook();

        LOGD("get system classloader");
        auto clClass = env->FindClass("java/lang/ClassLoader");
        auto getSystemClassLoader = env->GetStaticMethodID(clClass, "getSystemClassLoader",
                                                           "()Ljava/lang/ClassLoader;");
        auto systemClassLoader = env->CallStaticObjectMethod(clClass, getSystemClassLoader);

        LOGD("create class loader");
        auto dexClClass = env->FindClass("dalvik/system/InMemoryDexClassLoader");
        auto dexClInit = env->GetMethodID(dexClClass, "<init>",
                                          "(Ljava/nio/ByteBuffer;Ljava/lang/ClassLoader;)V");
        auto buffer = env->NewDirectByteBuffer(vector.data(), vector.size());
        auto dexCl = env->NewObject(dexClClass, dexClInit, buffer, systemClassLoader);

        LOGD("load class");
        auto loadClass = env->GetMethodID(clClass, "loadClass",
                                          "(Ljava/lang/String;)Ljava/lang/Class;");
        auto entryClassName = env->NewStringUTF("es.chiteroman.playintegrityfix.EntryPoint");
        auto entryClassObj = env->CallObjectMethod(dexCl, loadClass, entryClassName);

        auto entryClass = (jclass) entryClassObj;

        LOGD("call init");
        auto entryInit = env->GetStaticMethodID(entryClass, "init", "(Ljava/lang/String;)V");
        auto str = env->NewStringUTF(json.dump().c_str());
        env->CallStaticVoidMethod(entryClass, entryInit, str);

        vector.clear();
        json.clear();
    }

    void preServerSpecialize(zygisk::ServerSpecializeArgs *args) override {
        api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
    }

private:
    zygisk::Api *api = nullptr;
    JNIEnv *env = nullptr;
    std::vector<char> vector;
    nlohmann::json json;
};

static void companion(int fd) {
    long dexSize = 0, jsonSize = 0;
    std::vector<char> dexVector, jsonVector;

    FILE *dexFile = fopen(CLASSES_DEX, "rb");

    if (dexFile) {

        fseek(dexFile, 0, SEEK_END);
        dexSize = ftell(dexFile);
        fseek(dexFile, 0, SEEK_SET);

        dexVector.resize(dexSize);
        fread(dexVector.data(), 1, dexSize, dexFile);

        fclose(dexFile);
    }

    FILE *jsonFile = fopen(PIF_JSON, "rb");
    if (jsonFile == nullptr) jsonFile = fopen(PIF_JSON_2, "rb");

    if (jsonFile) {

        fseek(jsonFile, 0, SEEK_END);
        jsonSize = ftell(jsonFile);
        fseek(jsonFile, 0, SEEK_SET);

        jsonVector.resize(jsonSize);
        fread(jsonVector.data(), 1, jsonSize, jsonFile);

        fclose(jsonFile);
    }

    write(fd, &dexSize, sizeof(long));
    write(fd, &jsonSize, sizeof(long));

    write(fd, dexVector.data(), dexSize);
    write(fd, jsonVector.data(), jsonSize);
}

REGISTER_ZYGISK_MODULE(PlayIntegrityFix)
REGISTER_ZYGISK_COMPANION(companion)
