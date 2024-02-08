#include <android/log.h>
#include <unistd.h>
#include "dobby.h"
#include "json.hpp"
#include "zygisk.hpp"

#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, "PIF", __VA_ARGS__)

#define CLASSES_DEX "/data/adb/modules/playintegrityfix/classes.dex"

#define PIF_JSON "/data/adb/pif.json"

#define PIF_JSON_DEFAULT "/data/adb/modules/playintegrityfix/pif.json"

static std::string FIRST_API_LEVEL, SECURITY_PATCH, BUILD_ID;

typedef void (*T_Callback)(void *, const char *, const char *, uint32_t);

static std::map<void *, T_Callback> callbacks;

static void modify_callback(void *cookie, const char *name, const char *value, uint32_t serial) {

    if (cookie == nullptr || name == nullptr || value == nullptr ||
        !callbacks.contains(cookie))
        return;

    std::string_view prop(name);

    if (prop.ends_with("security_patch")) {

        if (!SECURITY_PATCH.empty()) {
            value = SECURITY_PATCH.c_str();
        }
        LOGD("[%s]: %s", name, value);

    } else if (prop.ends_with("api_level")) {

        if (!FIRST_API_LEVEL.empty()) {
            value = FIRST_API_LEVEL.c_str();
        }
        LOGD("[%s]: %s", name, value);

    } else if (prop.ends_with("build.id")) {

        if (!BUILD_ID.empty()) {
            value = BUILD_ID.c_str();
        }
        LOGD("[%s]: %s", name, value);

    } else if (prop == "sys.usb.state") {

        value = "none";
        LOGD("[%s]: %s", name, value);

    }

    return callbacks[cookie](cookie, name, value, serial);
}

static void (*o_system_property_read_callback)(const void *, T_Callback, void *);

static void
my_system_property_read_callback(const void *pi, T_Callback callback, void *cookie) {
    if (pi == nullptr || callback == nullptr || cookie == nullptr) {
        return o_system_property_read_callback(pi, callback, cookie);
    }
    callbacks[cookie] = callback;
    return o_system_property_read_callback(pi, modify_callback, cookie);
}

static void doHook() {
    void *handle = DobbySymbolResolver(nullptr, "__system_property_read_callback");
    if (handle == nullptr) {
        LOGD("Couldn't hook '__system_property_read_callback'. Report to @chiteroman");
        return;
    }
    DobbyHook(handle, (dobby_dummy_func_t) my_system_property_read_callback,
              (dobby_dummy_func_t *) &o_system_property_read_callback);
    LOGD("Found and hooked '__system_property_read_callback' at %p", handle);
}

class PlayIntegrityFix : public zygisk::ModuleBase {
public:
    void onLoad(zygisk::Api *api, JNIEnv *env) override {
        this->api = api;
        this->env = env;
    }

    void preAppSpecialize(zygisk::AppSpecializeArgs *args) override {

        auto dir = env->GetStringUTFChars(args->app_data_dir, nullptr);

        if (dir == nullptr) {
            api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
            return;
        }

        bool isGms = std::string_view(dir).ends_with("/com.google.android.gms");

        env->ReleaseStringUTFChars(args->app_data_dir, dir);

        if (!isGms) {
            api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
            return;
        }

        api->setOption(zygisk::FORCE_DENYLIST_UNMOUNT);

        auto name = env->GetStringUTFChars(args->nice_name, nullptr);

        bool isGmsUnstable = std::string_view(name) == "com.google.android.gms.unstable";

        env->ReleaseStringUTFChars(args->nice_name, name);

        if (!isGmsUnstable) {
            api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
            return;
        }

        long dexSize = 0, jsonSize = 0;

        int fd = api->connectCompanion();

        read(fd, &dexSize, sizeof(long));
        read(fd, &jsonSize, sizeof(long));

        LOGD("Dex file size: %ld", dexSize);
        LOGD("Json file size: %ld", jsonSize);

        if (dexSize > 0) {
            dexVector.resize(dexSize);
            read(fd, dexVector.data(), dexSize);
        } else {
            close(fd);
            LOGD("Dex file empty!");
            api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
            return;
        }

        std::vector<char> jsonVector;
        if (jsonSize > 0) {
            jsonVector.resize(jsonSize);
            read(fd, jsonVector.data(), jsonSize);
        } else {
            close(fd);
            LOGD("Json file empty!");
            api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
            return;
        }

        close(fd);

        json = nlohmann::json::parse(jsonVector, nullptr, false, true);
    }

    void postAppSpecialize(const zygisk::AppSpecializeArgs *args) override {
        if (dexVector.empty() || json.empty()) return;

        parseJson();

        doHook();

        injectDex();
    }

    void preServerSpecialize(zygisk::ServerSpecializeArgs *args) override {
        api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
    }

private:
    zygisk::Api *api;
    JNIEnv *env;
    std::vector<char> dexVector;
    nlohmann::json json;

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

    void parseJson() {
        if (json.contains("FIRST_API_LEVEL")) {

            if (json["FIRST_API_LEVEL"].is_number_integer()) {

                FIRST_API_LEVEL = std::to_string(json["FIRST_API_LEVEL"].get<int>());

            } else if (json["FIRST_API_LEVEL"].is_string()) {

                FIRST_API_LEVEL = json["FIRST_API_LEVEL"].get<std::string>();
            }

            json.erase("FIRST_API_LEVEL");

        } else if (json.contains("DEVICE_INITIAL_SDK_INT")) {

            if (json["DEVICE_INITIAL_SDK_INT"].is_number_integer()) {

                FIRST_API_LEVEL = std::to_string(json["DEVICE_INITIAL_SDK_INT"].get<int>());

            } else if (json["DEVICE_INITIAL_SDK_INT"].is_string()) {

                FIRST_API_LEVEL = json["DEVICE_INITIAL_SDK_INT"].get<std::string>();
            }

        } else {

            LOGD("JSON file doesn't contain FIRST_API_LEVEL or DEVICE_INITIAL_SDK_INT keys :(");
        }

        if (json.contains("SECURITY_PATCH")) {

            if (json["SECURITY_PATCH"].is_string()) {

                SECURITY_PATCH = json["SECURITY_PATCH"].get<std::string>();
            }

        } else {

            LOGD("JSON file doesn't contain SECURITY_PATCH key :(");
        }

        if (json.contains("ID")) {

            if (json["ID"].is_string()) {

                BUILD_ID = json["ID"].get<std::string>();
            }

        } else if (json.contains("BUILD_ID")) {

            if (json["BUILD_ID"].is_string()) {

                BUILD_ID = json["BUILD_ID"].get<std::string>();
            }

            json["ID"] = BUILD_ID;
            json.erase("BUILD_ID");

        } else {

            LOGD("JSON file doesn't contain ID/BUILD_ID keys :(");
        }
    }
};

static void companion(int fd) {

    long dexSize = 0;
    long jsonSize = 0;

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
    if (jsonFile == nullptr) jsonFile = fopen(PIF_JSON_DEFAULT, "rb");

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