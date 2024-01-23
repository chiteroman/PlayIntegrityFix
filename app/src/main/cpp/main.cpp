#include <android/log.h>
#include <unistd.h>
#include <map>
#include <fstream>
#include "zygisk.hpp"
#include "json.hpp"
#include "dobby.h"

#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, "PIF", __VA_ARGS__)

#define CLASSES_DEX "/data/adb/modules/playintegrityfix/classes.dex"

#define PIF_JSON "/data/adb/pif.json"

static JavaVM *jvm = nullptr;

static jclass entryPointClass = nullptr;

static jmethodID spoofFieldsMethod = nullptr;

static void spoofFields() {

    if (jvm == nullptr) {
        LOGD("JavaVM is null!");
        return;
    }

    JNIEnv *env;
    jvm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6);

    if (env != nullptr && entryPointClass != nullptr && spoofFieldsMethod != nullptr) {

        env->CallStaticVoidMethod(entryPointClass, spoofFieldsMethod);

        LOGD("[Zygisk] Call Java spoofFields!");
    }

    if (env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
    }
}

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

    } else if (prop.ends_with("api_level")) {

        if (!FIRST_API_LEVEL.empty()) {

            value = FIRST_API_LEVEL.c_str();
        }

    } else if (prop.ends_with("build.id")) {

        if (!BUILD_ID.empty()) {

            value = BUILD_ID.c_str();
        }

    } else if (prop == "sys.usb.state") {

        value = "none";
    }

    if (prop == "ro.product.first_api_level") spoofFields();

    if (!prop.starts_with("debug") && !prop.starts_with("cache") && !prop.starts_with("persist")) {

        LOGD("[%s] -> %s", name, value);
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
        LOGD("Couldn't find '__system_property_read_callback' handle. Report to @chiteroman");
        return;
    }
    DobbyHook(
            handle,
            reinterpret_cast<dobby_dummy_func_t>(my_system_property_read_callback),
            reinterpret_cast<dobby_dummy_func_t *>(&o_system_property_read_callback));
    LOGD("Found '__system_property_read_callback' handle at %p", handle);
}

class PlayIntegrityFix : public zygisk::ModuleBase {
public:
    void onLoad(zygisk::Api *api, JNIEnv *env) override {
        this->api = api;
        this->env = env;
    }

void preAppSpecialize(zygisk::AppSpecializeArgs *args) override {
    if (!args->app_data_dir) return;
    const char* app_data_dir = env->GetStringUTFChars(args->app_data_dir, nullptr);

    if (std::string_view(app_data_dir).ends_with("/com.google.android.gms")) {
        const char* name = env->GetStringUTFChars(args->nice_name, nullptr);
        bool should_load_dex = strcmp(name, "com.google.android.gms.unstable") == 0;
        env->ReleaseStringUTFChars(args->nice_name, name);

        api->setOption(zygisk::FORCE_DENYLIST_UNMOUNT);

        if (!should_load_dex) {
            env->ReleaseStringUTFChars(args->app_data_dir, app_data_dir);
            return;
        }

        long dexSize = 0, jsonSize = 0;
        int fd = api->connectCompanion();

        read(fd, &dexSize, sizeof(long));
        read(fd, &jsonSize, sizeof(long));

        LOGD("Dex file size: %ld", dexSize);
        LOGD("Json file size: %ld", jsonSize);

        dex_vector.resize(dexSize);
        read(fd, dex_vector.data(), dexSize);

        std::vector<char> json_vector(jsonSize);
        read(fd, json_vector.data(), jsonSize);

        close(fd);

        std::string_view json_str(json_vector.data(), json_vector.size());
        json = nlohmann::json::parse(json_str, nullptr, false, true);

        parseJson();
    }

    env->ReleaseStringUTFChars(args->app_data_dir, app_data_dir);
}

void postAppSpecialize(const zygisk::AppSpecializeArgs *args) override {
    if (dex_vector.empty() || json.empty()) {
        api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
        return;
    }

    doHook();
    injectDex();

    dex_vector.clear();
    json.clear();
}

void preServerSpecialize(zygisk::ServerSpecializeArgs *args) override {
    api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
}

private:
    zygisk::Api *api = nullptr;
    JNIEnv *env = nullptr;
    std::vector<char> dex_vector;
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
    auto buffer = env->NewDirectByteBuffer(dex_vector.data(), dex_vector.size()); // Fixed line
    auto dexCl = env->NewObject(dexClClass, dexClInit, buffer, systemClassLoader);

        LOGD("load class");
        auto loadClass = env->GetMethodID(clClass, "loadClass",
                                          "(Ljava/lang/String;)Ljava/lang/Class;");
        auto entryClassName = env->NewStringUTF("es.chiteroman.playintegrityfix.EntryPoint");
        auto entryClassObj = env->CallObjectMethod(dexCl, loadClass, entryClassName);

        entryPointClass = (jclass) entryClassObj;

        LOGD("call init");
        auto entryInit = env->GetStaticMethodID(entryPointClass, "init", "(Ljava/lang/String;)V");
        auto str = env->NewStringUTF(json.dump().c_str());
        env->CallStaticVoidMethod(entryPointClass, entryInit, str);

        spoofFieldsMethod = env->GetStaticMethodID(entryPointClass, "spoofFields", "()V");
    }

    void parseJson() {
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

        } else {

            LOGD("JSON file doesn't contain ID/BUILD_ID keys :(");
        }
    }
};

static void companion(int fd) {

    std::ifstream dexFile(CLASSES_DEX, std::ios::binary);
    std::ifstream jsonFile(PIF_JSON);

    std::vector<char> dexVector((std::istreambuf_iterator<char>(dexFile)),
                                std::istreambuf_iterator<char>());
    std::vector<char> jsonVector((std::istreambuf_iterator<char>(jsonFile)),
                                 std::istreambuf_iterator<char>());

    long dexSize = dexVector.size();
    long jsonSize = jsonVector.size();

    write(fd, &dexSize, sizeof(long));
    write(fd, &jsonSize, sizeof(long));

    write(fd, dexVector.data(), dexSize);
    write(fd, jsonVector.data(), jsonSize);
}

REGISTER_ZYGISK_MODULE(PlayIntegrityFix)

REGISTER_ZYGISK_COMPANION(companion)
