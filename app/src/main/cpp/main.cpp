#include <android/log.h>
#include <unistd.h>
#include <string>
#include <string_view>
#include <vector>
#include "zygisk.hpp"
#include "dobby.h"
#include "json.hpp"

#define LOG_TAG "PIF/Native"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

#define CLASSES_DEX "/data/adb/modules/playintegrityfix/classes.dex"
#define PIF_JSON "/data/adb/pif.json"

static std::string FIRST_API_LEVEL, SECURITY_PATCH, BUILD_ID;

typedef void (*T_Callback)(void *, const char *, const char *, uint32_t);
static T_Callback original_callback = nullptr;

static void modify_callback(void *cookie, const char *name, const char *value, uint32_t serial) {
    if (!cookie || !name || !value || !original_callback) return;

    std::string_view prop(name);
    const char* new_value = value;

    if (prop.ends_with("security_patch") && !SECURITY_PATCH.empty()) {
        new_value = SECURITY_PATCH.c_str();
    } else if (prop.ends_with("api_level") && !FIRST_API_LEVEL.empty()) {
        new_value = FIRST_API_LEVEL.c_str();
    } else if (prop.ends_with("build.id") && !BUILD_ID.empty()) {
        new_value = BUILD_ID.c_str();
    } else if (prop == "sys.usb.state") {
        new_value = "none";
    }

    if (!prop.starts_with("debug") && !prop.starts_with("cache") && !prop.starts_with("persist")) {
        LOGD("[%s] -> %s", name, new_value);
    }

    original_callback(cookie, name, new_value, serial);
}

static void (*original_system_property_read_callback)(const void *, T_Callback, void *);

static void my_system_property_read_callback(const void *pi, T_Callback callback, void *cookie) {
    if (!pi || !callback || !cookie) {
        original_system_property_read_callback(pi, callback, cookie);
        return;
    }
    original_callback = callback;
    original_system_property_read_callback(pi, modify_callback, cookie);
}

static void doHook() {
    void *handle = DobbySymbolResolver("libc.so", "__system_property_read_callback");
    if (!handle) {
        LOGD("Couldn't find '__system_property_read_callback' handle. Report to @chiteroman");
        return;
    }
    LOGD("Found '__system_property_read_callback' handle at %p", handle);
    DobbyHook(handle, (dobby_dummy_func_t)my_system_property_read_callback, (dobby_dummy_func_t *)&original_system_property_read_callback);
}

static bool ends_with(const std::string& str, const std::string& suffix) {
    return str.size() >= suffix.size() && str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
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

        if (ends_with(app_data_dir, "/com.google.android.gms")) {
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

            std::string_view json_str(json_vector.begin(), json_vector.end());
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
        jclass clClass = env->FindClass("java/lang/ClassLoader");
        jmethodID getSystemClassLoader = env->GetStaticMethodID(clClass, "getSystemClassLoader", "()Ljava/lang/ClassLoader;");
        jobject systemClassLoader = env->CallStaticObjectMethod(clClass, getSystemClassLoader);

        jclass dexClClass = env->FindClass("dalvik/system/InMemoryDexClassLoader");
        jmethodID dexClInit = env->GetMethodID(dexClClass, "<init>", "(Ljava/nio/ByteBuffer;Ljava/lang/ClassLoader;)V");
        jobject buffer = env->NewDirectByteBuffer(dex_vector.data(), dex_vector.size());
        jobject dexCl = env->NewObject(dexClClass, dexClInit, buffer, systemClassLoader);

        jmethodID loadClass = env->GetMethodID(clClass, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
        jstring entryClassName = env->NewStringUTF("es.chiteroman.playintegrityfix.EntryPoint");
        jclass entryClass = (jclass)env->CallObjectMethod(dexCl, loadClass, entryClassName);

        jmethodID entryInit = env->GetStaticMethodID(entryClass, "init", "(Ljava/lang/String;)V");
        jstring str = env->NewStringUTF(json.dump().c_str());
        env->CallStaticVoidMethod(entryClass, entryInit, str);
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
            json.erase("SECURITY_PATCH");
        } else {
            LOGD("JSON file doesn't contain SECURITY_PATCH key :(");
        }

        if (json.contains("BUILD_ID")) {
            if (json["BUILD_ID"].is_string()) {
                BUILD_ID = json["BUILD_ID"].get<std::string>();
            }
            json.erase("BUILD_ID");
        } else {
            LOGD("JSON file doesn't contain BUILD_ID key :(");
        }
    }
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

    FILE *jsonFile = fopen(PIF_JSON, "r");
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
