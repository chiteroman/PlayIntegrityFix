#include <android/log.h>
#include <sys/system_properties.h>
#include <unistd.h>

#include "zygisk.hpp"
#include "dobby.h"
#include "json.hpp"

#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, "PIF/Native", __VA_ARGS__)

#define DEX_FILE_PATH "/data/adb/modules/playintegrityfix/classes.dex"

#define JSON_FILE_PATH "/data/adb/pif.json"

#define JSON_TEMP_FILE "/data/adb/modules/playintegrityfix/temp"

static std::string FIRST_API_LEVEL, SECURITY_PATCH;

typedef void (*T_Callback)(void *, const char *, const char *, uint32_t);

static std::map<void *, T_Callback> callbacks;

static void modify_callback(void *cookie, const char *name, const char *value, uint32_t serial) {

    if (cookie == nullptr || name == nullptr || value == nullptr ||
        !callbacks.contains(cookie))
        return;

    std::string_view prop(name);

    if (prop.ends_with("api_level")) {
        if (!FIRST_API_LEVEL.empty()) {
            value = FIRST_API_LEVEL.c_str();
            LOGD("[%s]: %s", name, value);
        }
    }

    if (prop.ends_with("security_patch")) {
        if (!SECURITY_PATCH.empty()) {
            value = SECURITY_PATCH.c_str();
            LOGD("[%s]: %s", name, value);
        }
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
    void *handle = DobbySymbolResolver(nullptr, "__system_property_read_callback");
    if (handle == nullptr) {
        LOGD("Couldn't find '__system_property_read_callback' handle. Report to @chiteroman");
        return;
    }
    LOGD("Found '__system_property_read_callback' handle at %p", handle);
    DobbyHook(handle, (dobby_dummy_func_t) my_system_property_read_callback,
              (dobby_dummy_func_t *) &o_system_property_read_callback);
}

class PlayIntegrityFix : public zygisk::ModuleBase {
public:
    void onLoad(zygisk::Api *api, JNIEnv *env) override {
        this->api = api;
        this->env = env;
    }

    void preAppSpecialize(zygisk::AppSpecializeArgs *args) override {
        bool isGms = false, isGmsUnstable = false;

        auto process = env->GetStringUTFChars(args->nice_name, nullptr);

        if (process) {
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

        long dexSize = 0, jsonSize = 0;
        int fd = api->connectCompanion();

        read(fd, &dexSize, sizeof(long));
        read(fd, &jsonSize, sizeof(long));

        if (dexSize < 1) {
            close(fd);
            LOGD("Couldn't read classes.dex");
            api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
            return;
        }

        if (jsonSize < 1) {
            close(fd);
            LOGD("Couldn't read pif.json");
            api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
            return;
        }

        LOGD("Read from file descriptor file 'classes.dex' -> %ld bytes", dexSize);
        LOGD("Read from file descriptor file 'pif.json' -> %ld bytes", jsonSize);

        dexVector.resize(dexSize);
        read(fd, dexVector.data(), dexSize);

        std::vector<char> jsonVector(jsonSize);
        read(fd, jsonVector.data(), jsonSize);

        close(fd);

        std::string data(jsonVector.cbegin(), jsonVector.cend());
        json = nlohmann::json::parse(data, nullptr, false, true);

        jsonVector.clear();
        data.clear();
    }

    void postAppSpecialize(const zygisk::AppSpecializeArgs *args) override {
        if (dexVector.empty() || json.empty()) return;

        readJson();

        doHook();

        inject();

        dexVector.clear();
        json.clear();
    }

    void preServerSpecialize(zygisk::ServerSpecializeArgs *args) override {
        api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
    }

private:
    zygisk::Api *api = nullptr;
    JNIEnv *env = nullptr;
    std::vector<char> dexVector;
    nlohmann::json json;

    void readJson() {
        LOGD("JSON contains %d keys!", static_cast<int>(json.size()));

        if (json.contains("SECURITY_PATCH")) {
            if (json["SECURITY_PATCH"].is_null()) {
                LOGD("Key SECURITY_PATCH is null!");
            } else if (json["SECURITY_PATCH"].is_string()) {
                SECURITY_PATCH = json["SECURITY_PATCH"].get<std::string>();
            } else {
                LOGD("Error parsing SECURITY_PATCH!");
            }
        } else {
            LOGD("Key SECURITY_PATCH doesn't exist in JSON file!");
        }

        if (json.contains("FIRST_API_LEVEL")) {
            if (json["FIRST_API_LEVEL"].is_null()) {
                LOGD("Key FIRST_API_LEVEL is null!");
            } else if (json["FIRST_API_LEVEL"].is_string()) {
                FIRST_API_LEVEL = json["FIRST_API_LEVEL"].get<std::string>();
            } else {
                LOGD("Error parsing FIRST_API_LEVEL!");
            }
            json.erase("FIRST_API_LEVEL");
        } else {
            LOGD("Key FIRST_API_LEVEL doesn't exist in JSON file!");
        }
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

        LOGD("call init");
        auto entryInit = env->GetStaticMethodID(entryClass, "init", "(Ljava/lang/String;)V");
        auto javaStr = env->NewStringUTF(json.dump().c_str());
        env->CallStaticVoidMethod(entryClass, entryInit, javaStr);
    }
};

static void companion(int fd) {
    long dexSize = 0, jsonSize = 0;
    std::vector<char> dexVector, jsonVector;

    FILE *dex = fopen(DEX_FILE_PATH, "rb");

    if (dex) {
        fseek(dex, 0, SEEK_END);
        dexSize = ftell(dex);
        fseek(dex, 0, SEEK_SET);

        dexVector.resize(dexSize);
        fread(dexVector.data(), 1, dexSize, dex);

        fclose(dex);
    }

    if (std::filesystem::exists(JSON_FILE_PATH)) {
        FILE *json = fopen(JSON_FILE_PATH, "rb");

        if (json) {
            fseek(json, 0, SEEK_END);
            jsonSize = ftell(json);
            fseek(json, 0, SEEK_SET);

            jsonVector.resize(jsonSize);
            fread(jsonVector.data(), 1, jsonSize, json);

            fclose(json);
        }
    } else if (std::filesystem::exists(JSON_TEMP_FILE)) {
        FILE *json = fopen(JSON_TEMP_FILE, "rb");

        if (json) {
            fseek(json, 0, SEEK_END);
            jsonSize = ftell(json);
            fseek(json, 0, SEEK_SET);

            jsonVector.resize(jsonSize);
            fread(jsonVector.data(), 1, jsonSize, json);

            fclose(json);
        }
    }

    write(fd, &dexSize, sizeof(long));
    write(fd, &jsonSize, sizeof(long));

    write(fd, dexVector.data(), dexSize);
    write(fd, jsonVector.data(), jsonSize);

    dexVector.clear();
    jsonVector.clear();
}

REGISTER_ZYGISK_MODULE(PlayIntegrityFix)

REGISTER_ZYGISK_COMPANION(companion)