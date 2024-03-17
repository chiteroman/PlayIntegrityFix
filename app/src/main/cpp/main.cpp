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

    if (!prop.starts_with("persist") && !prop.starts_with("cache") && !prop.starts_with("debug")) {
        LOGD("[%s]: %s", name, value);
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

        const char *dir, *name;
        bool isGms, isGmsUnstable;

        if (!args) goto exit;

        dir = env->GetStringUTFChars(args->app_data_dir, nullptr);

        if (!dir) goto exit;

        isGms = std::string_view(dir).ends_with("/com.google.android.gms");

        env->ReleaseStringUTFChars(args->app_data_dir, dir);

        if (isGms) {
            name = env->GetStringUTFChars(args->nice_name, nullptr);

            if (!name) goto exit;

            isGmsUnstable = strcmp(name, "com.google.android.gms.unstable") == 0;

            if (isGmsUnstable) {

                long dexSize = 0, jsonSize = 0;

                int fd = api->connectCompanion();

                read(fd, &dexSize, sizeof(long));
                read(fd, &jsonSize, sizeof(long));

                LOGD("Dex file size: %ld", dexSize);
                LOGD("Json file size: %ld", jsonSize);

                if (dexSize < 1 || jsonSize < 1) {
                    close(fd);
                    LOGD("Invalid files!");
                    goto exit;
                }

                dexVector.resize(dexSize);
                read(fd, dexVector.data(), dexSize);

                std::vector<uint8_t> jsonVector;

                jsonVector.resize(jsonSize);
                read(fd, jsonVector.data(), jsonSize);

                close(fd);

                json = nlohmann::json::parse(jsonVector, nullptr, false, true);

                parseJson();

                return;

            } else {
                api->setOption(zygisk::FORCE_DENYLIST_UNMOUNT);
                goto exit;
            }

        } else {
            goto exit;
        }

        exit:
        api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
    }

    void postAppSpecialize(const zygisk::AppSpecializeArgs *args) override {
        if (dexVector.empty() || json.empty()) return;

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