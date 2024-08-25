#include <android/log.h>
#include <sys/system_properties.h>
#include <unistd.h>
#include <vector>
#include <filesystem>
#include "zygisk.hpp"
#include "dobby.h"
#include "cJSON.h"

#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, "PIF", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "PIF", __VA_ARGS__)

#define DEX_PATH "/data/adb/modules/playintegrityfix/classes.dex"

#define PIF_JSON "/data/adb/pif.json"

#define PIF_JSON_DEFAULT "/data/adb/modules/playintegrityfix/pif.json"

#define TS_PATH "/data/adb/modules/tricky_store"

static ssize_t xread(int fd, void *buffer, size_t count) {
    ssize_t total = 0;
    char *buf = (char *) buffer;
    while (count > 0) {
        ssize_t ret = read(fd, buf, count);
        if (ret < 0) return -1;
        buf += ret;
        total += ret;
        count -= ret;
    }
    return total;
}

static ssize_t xwrite(int fd, void *buffer, size_t count) {
    ssize_t total = 0;
    char *buf = (char *) buffer;
    while (count > 0) {
        ssize_t ret = write(fd, buf, count);
        if (ret < 0) return -1;
        buf += ret;
        total += ret;
        count -= ret;
    }
    return total;
}

static std::string DEVICE_INITIAL_SDK_INT;
static std::string SECURITY_PATCH;
static std::string BUILD_ID;

static bool DEBUG = false;

typedef void (*T_Callback)(void *, const char *, const char *, uint32_t);

static volatile T_Callback o_callback = nullptr;

static void modify_callback(void *cookie, const char *name, const char *value, uint32_t serial) {

    if (cookie == nullptr || name == nullptr || value == nullptr || o_callback == nullptr)
        return;

    std::string_view prop(name);

    bool print = false;

    if (prop == "init.svc.adbd") {
        value = "stopped";
        print = true;
    } else if (prop == "sys.usb.state") {
        value = "mtp";
        print = true;
    } else if (prop.ends_with("api_level") && !DEVICE_INITIAL_SDK_INT.empty()) {
        value = DEVICE_INITIAL_SDK_INT.c_str();
        print = true;
    } else if (prop.ends_with(".security_patch") && !SECURITY_PATCH.empty()) {
        value = SECURITY_PATCH.c_str();
        print = true;
    } else if (prop.ends_with(".build.id") && !BUILD_ID.empty()) {
        value = BUILD_ID.c_str();
        print = true;
    }

    if (print || DEBUG) LOGD("[%s]: %s", name, value);

    return o_callback(cookie, name, value, serial);
}

static void (*o_system_property_read_callback)(const prop_info *, T_Callback, void *) = nullptr;

static void
my_system_property_read_callback(const prop_info *pi, T_Callback callback, void *cookie) {
    if (pi && callback && cookie) o_callback = callback;
    return o_system_property_read_callback(pi, modify_callback, cookie);
}

static void doHook() {
    void *handle = DobbySymbolResolver(nullptr, "__system_property_read_callback");
    if (!handle) {
        LOGE("error resolving __system_property_read_callback symbol!");
        return;
    }
    if (DobbyHook(handle, (void *) my_system_property_read_callback,
                  (void **) &o_system_property_read_callback)) {
        LOGE("hook __system_property_read_callback failed!");
        return;
    }
    LOGD("hook __system_property_read_callback success at %p", handle);
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

        auto dir = env->GetStringUTFChars(args->app_data_dir, nullptr);

        if (!dir) {
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

        if (!name) {
            api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
            return;
        }

        bool isGmsUnstable = std::string_view(name) == "com.google.android.gms.unstable";

        env->ReleaseStringUTFChars(args->nice_name, name);

        if (!isGmsUnstable) {
            api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
            return;
        }

        int fd = api->connectCompanion();

        int dexSize = 0, jsonSize = 0;
        std::vector<uint8_t> jsonVector;

        xread(fd, &dexSize, sizeof(int));
        xread(fd, &jsonSize, sizeof(int));

        if (dexSize > 0) {
            dexVector.resize(dexSize);
            xread(fd, dexVector.data(), dexSize * sizeof(uint8_t));
        }

        if (jsonSize > 0) {
            jsonVector.resize(jsonSize);
            xread(fd, jsonVector.data(), jsonSize * sizeof(uint8_t));
            std::string strJson(jsonVector.cbegin(), jsonVector.cend());
            json = cJSON_ParseWithLength(strJson.c_str(), strJson.size());
        }

        bool trickyStore = false;
        xread(fd, &trickyStore, sizeof(trickyStore));

        close(fd);

        LOGD("Dex file size: %d", dexSize);
        LOGD("Json file size: %d", jsonSize);

        parseJSON();

        if (trickyStore) {
            LOGD("TrickyStore module installed and enabled, disabling spoofProps and spoofProvider");
            spoofProps = false;
            spoofProvider = false;
        }
    }

    void postAppSpecialize(const zygisk::AppSpecializeArgs *args) override {
        if (dexVector.empty()) return;

        UpdateBuildFields();

        if (spoofProps) doHook();
        else api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);

        if (spoofProvider || spoofSignature) injectDex();
        else
            LOGD("Dex file won't be injected due spoofProvider and spoofSignature are false");

        cJSON_Delete(json);
        dexVector.clear();
        dexVector.shrink_to_fit();
    }

    void preServerSpecialize(zygisk::ServerSpecializeArgs *args) override {
        api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
    }

private:
    zygisk::Api *api = nullptr;
    JNIEnv *env = nullptr;
    std::vector<uint8_t> dexVector;
    cJSON *json = nullptr;
    bool spoofProps = true;
    bool spoofProvider = true;
    bool spoofSignature = false;

    void parseJSON() {
        if (!json) return;

        const cJSON *api_level = cJSON_GetObjectItemCaseSensitive(json, "DEVICE_INITIAL_SDK_INT");
        const cJSON *security_patch = cJSON_GetObjectItemCaseSensitive(json, "SECURITY_PATCH");
        const cJSON *build_id = cJSON_GetObjectItemCaseSensitive(json, "ID");
        const cJSON *isDebug = cJSON_GetObjectItemCaseSensitive(json, "DEBUG");
        const cJSON *spoof_props = cJSON_GetObjectItemCaseSensitive(json, "spoofProps");
        const cJSON *spoof_provider = cJSON_GetObjectItemCaseSensitive(json, "spoofProvider");
        const cJSON *spoof_signature = cJSON_GetObjectItemCaseSensitive(json, "spoofSignature");

        if (api_level) {
            if (cJSON_IsNumber(api_level)) {
                DEVICE_INITIAL_SDK_INT = std::to_string(api_level->valueint);
            } else if (cJSON_IsString(api_level)) {
                DEVICE_INITIAL_SDK_INT = api_level->valuestring;
            }
            cJSON_DeleteItemFromObjectCaseSensitive(json, "DEVICE_INITIAL_SDK_INT");
        }

        if (security_patch && cJSON_IsString(security_patch)) {
            SECURITY_PATCH = security_patch->valuestring;
        }

        if (build_id && cJSON_IsString(build_id)) {
            BUILD_ID = build_id->valuestring;
        }

        if (isDebug && cJSON_IsBool(isDebug)) {
            DEBUG = cJSON_IsTrue(isDebug);
            cJSON_DeleteItemFromObjectCaseSensitive(json, "DEBUG");
        }

        if (spoof_props && cJSON_IsBool(spoof_props)) {
            spoofProps = cJSON_IsTrue(spoof_props);
            cJSON_DeleteItemFromObjectCaseSensitive(json, "spoofProps");
        }

        if (spoof_provider && cJSON_IsBool(spoof_provider)) {
            spoofProvider = cJSON_IsTrue(spoof_provider);
            cJSON_DeleteItemFromObjectCaseSensitive(json, "spoofProvider");
        }

        if (spoof_signature && cJSON_IsBool(spoof_signature)) {
            spoofSignature = cJSON_IsTrue(spoof_signature);
            cJSON_DeleteItemFromObjectCaseSensitive(json, "spoofSignature");
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
        auto buffer = env->NewDirectByteBuffer(dexVector.data(),
                                               static_cast<jlong>(dexVector.size()));
        auto dexCl = env->NewObject(dexClClass, dexClInit, buffer, systemClassLoader);

        LOGD("load class");
        auto loadClass = env->GetMethodID(clClass, "loadClass",
                                          "(Ljava/lang/String;)Ljava/lang/Class;");
        auto entryClassName = env->NewStringUTF("es.chiteroman.playintegrityfix.EntryPoint");
        auto entryClassObj = env->CallObjectMethod(dexCl, loadClass, entryClassName);

        auto entryPointClass = (jclass) entryClassObj;

        LOGD("call init");
        auto entryInit = env->GetStaticMethodID(entryPointClass, "init", "(Ljava/lang/String;ZZ)V");
        auto jsonStr = env->NewStringUTF(cJSON_Print(json));
        env->CallStaticVoidMethod(entryPointClass, entryInit, jsonStr, spoofProvider,
                                  spoofSignature);
    }

    void UpdateBuildFields() {
        jclass buildClass = env->FindClass("android/os/Build");
        jclass versionClass = env->FindClass("android/os/Build$VERSION");

        cJSON *currentElement = nullptr;
        cJSON_ArrayForEach(currentElement, json) {
            const char *key = currentElement->string;

            if (cJSON_IsString(currentElement)) {
                const char *value = currentElement->valuestring;
                jfieldID fieldID = env->GetStaticFieldID(buildClass, key, "Ljava/lang/String;");

                if (env->ExceptionCheck()) {
                    env->ExceptionClear();

                    fieldID = env->GetStaticFieldID(versionClass, key, "Ljava/lang/String;");

                    if (env->ExceptionCheck()) {
                        env->ExceptionClear();
                        continue;
                    }
                }

                if (fieldID != nullptr) {
                    jstring jValue = env->NewStringUTF(value);

                    env->SetStaticObjectField(buildClass, fieldID, jValue);
                    if (env->ExceptionCheck()) {
                        env->ExceptionClear();
                        continue;
                    }

                    LOGD("Set '%s' to '%s'", key, value);
                }
            }
        }
    }
};

static std::vector<uint8_t> readFile(const char *path) {
    FILE *file = fopen(path, "rb");

    if (!file) return {};

    auto size = std::filesystem::file_size(path);

    std::vector<uint8_t> vector(size);

    fread(vector.data(), 1, size, file);

    fclose(file);

    return vector;
}

static void companion(int fd) {

    std::vector<uint8_t> dex, json;

    if (std::filesystem::exists(DEX_PATH)) {
        dex = readFile(DEX_PATH);
    }

    if (std::filesystem::exists(PIF_JSON)) {
        json = readFile(PIF_JSON);
    } else if (std::filesystem::exists(PIF_JSON_DEFAULT)) {
        json = readFile(PIF_JSON_DEFAULT);
    }

    int dexSize = static_cast<int>(dex.size());
    int jsonSize = static_cast<int>(json.size());

    xwrite(fd, &dexSize, sizeof(int));
    xwrite(fd, &jsonSize, sizeof(int));

    if (dexSize > 0) {
        xwrite(fd, dex.data(), dexSize * sizeof(uint8_t));
    }

    if (jsonSize > 0) {
        xwrite(fd, json.data(), jsonSize * sizeof(uint8_t));
    }

    bool trickyStore = std::filesystem::exists(TS_PATH) &&
                       !std::filesystem::exists(std::string(TS_PATH) + "/disable");
    xwrite(fd, &trickyStore, sizeof(trickyStore));
}

REGISTER_ZYGISK_MODULE(PlayIntegrityFix)

REGISTER_ZYGISK_COMPANION(companion)
