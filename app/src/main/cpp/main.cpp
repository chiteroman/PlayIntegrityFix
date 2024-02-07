#include <android/log.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <string_view>
#include "cJSON.h"
#include "shadowhook.h"
#include "zygisk.hpp"

#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, "PIF", __VA_ARGS__)

#define CLASSES_DEX "/data/adb/modules/playintegrityfix/classes.dex"

#define PIF_JSON "/data/adb/pif.json"

#define PIF_JSON_DEFAULT "/data/adb/modules/playintegrityfix/pif.json"

static char FIRST_API_LEVEL[3]; // 1 - 34 (Max 2 chars) (Check: https://apilevels.com/)
static char SECURITY_PATCH[11]; // 0000-00-00 (Max 10 chars)
static char BUILD_ID[20];

typedef void (*T_Callback)(void *, const char *, const char *, uint32_t);

static T_Callback o_callback = nullptr;

static void modify_callback(void *cookie, const char *name, const char *value, uint32_t serial) {

    if (cookie == nullptr || name == nullptr || value == nullptr || o_callback == nullptr) return;

    std::string_view prop(name);

    if (prop.ends_with("security_patch")) {

        if (SECURITY_PATCH[0] != 0) {
            value = SECURITY_PATCH;
        }
        LOGD("[%s]: %s", name, value);

    } else if (prop.ends_with("api_level")) {

        if (FIRST_API_LEVEL[0] != 0) {
            value = FIRST_API_LEVEL;
        }
        LOGD("[%s]: %s", name, value);

    } else if (prop.ends_with("build.id")) {

        if (BUILD_ID[0] != 0) {
            value = BUILD_ID;
        }
        LOGD("[%s]: %s", name, value);

    } else if (prop == "sys.usb.state") {

        value = "none";
        LOGD("[%s]: %s", name, value);

    }

    return o_callback(cookie, name, value, serial);
}

static void (*o_system_property_read_callback)(const void *, T_Callback, void *);

static void
my_system_property_read_callback(const void *pi, T_Callback callback, void *cookie) {
    if (pi == nullptr || callback == nullptr || cookie == nullptr) {
        return o_system_property_read_callback(pi, callback, cookie);
    }
    o_callback = callback;
    return o_system_property_read_callback(pi, modify_callback, cookie);
}

static void doHook() {
    void *handle = shadowhook_hook_sym_name("libc.so", "__system_property_read_callback",
                                            reinterpret_cast<void *>(my_system_property_read_callback),
                                            reinterpret_cast<void **>(&o_system_property_read_callback));
    if (handle == nullptr) {
        LOGD("Couldn't hook '__system_property_read_callback'. Report to @chiteroman");
        return;
    }
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

        bool isGmsUnstable = strcmp(name, "com.google.android.gms.unstable") == 0;

        env->ReleaseStringUTFChars(args->nice_name, name);

        if (!isGmsUnstable) {
            api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
            return;
        }

        long jsonSize = 0;

        int fd = api->connectCompanion();

        read(fd, &dexSize, sizeof(long));
        read(fd, &jsonSize, sizeof(long));

        LOGD("Dex file size: %ld", dexSize);
        LOGD("Json file size: %ld", jsonSize);

        if (dexSize > 0) {
            dexBuffer = new char[dexSize];
            read(fd, dexBuffer, dexSize);
        } else {
            close(fd);
            LOGD("Dex file empty!");
            api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
            return;
        }

        char *jsonBuffer;
        if (jsonSize > 0) {
            jsonBuffer = new char[jsonSize];
            read(fd, jsonBuffer, jsonSize);
        } else {
            close(fd);
            LOGD("Json file empty!");
            api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
            return;
        }

        close(fd);

        json = cJSON_ParseWithLength(jsonBuffer, jsonSize);

        if (json == nullptr) {
            LOGD("Error parsing json data!");
            delete[] jsonBuffer;
            delete[] dexBuffer;
            api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
        }
    }

    void postAppSpecialize(const zygisk::AppSpecializeArgs *args) override {
        if (dexBuffer == nullptr || json == nullptr || dexSize < 1) return;

        shadowhook_init(SHADOWHOOK_MODE_UNIQUE, false);

        parseJson();

        doHook();

        injectDex();

        delete[] dexBuffer;
        cJSON_Delete(json);
    }

    void preServerSpecialize(zygisk::ServerSpecializeArgs *args) override {
        api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
    }

private:
    zygisk::Api *api = nullptr;
    JNIEnv *env = nullptr;
    char *dexBuffer = nullptr;
    long dexSize = 0;
    cJSON *json = nullptr;

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
        auto buffer = env->NewDirectByteBuffer(dexBuffer, dexSize);
        auto dexCl = env->NewObject(dexClClass, dexClInit, buffer, systemClassLoader);

        LOGD("load class");
        auto loadClass = env->GetMethodID(clClass, "loadClass",
                                          "(Ljava/lang/String;)Ljava/lang/Class;");
        auto entryClassName = env->NewStringUTF("es.chiteroman.playintegrityfix.EntryPoint");
        auto entryClassObj = env->CallObjectMethod(dexCl, loadClass, entryClassName);

        auto entryPointClass = (jclass) entryClassObj;

        LOGD("call init");
        auto entryInit = env->GetStaticMethodID(entryPointClass, "init", "(Ljava/lang/String;)V");
        auto str = env->NewStringUTF(cJSON_PrintUnformatted(json));
        env->CallStaticVoidMethod(entryPointClass, entryInit, str);
    }

    void parseJson() {
        cJSON *firstApiLevel = cJSON_GetObjectItem(json, "FIRST_API_LEVEL");
        if (firstApiLevel) {
            if (cJSON_IsNumber(firstApiLevel)) {
                snprintf(FIRST_API_LEVEL, sizeof(FIRST_API_LEVEL), "%d", firstApiLevel->valueint);
            } else if (cJSON_IsString(firstApiLevel)) {
                strcpy(FIRST_API_LEVEL, firstApiLevel->valuestring);
            }
        } else {
            cJSON *deviceInitialSdkInt = cJSON_GetObjectItem(json, "DEVICE_INITIAL_SDK_INT");
            if (deviceInitialSdkInt) {
                if (cJSON_IsNumber(deviceInitialSdkInt)) {
                    snprintf(FIRST_API_LEVEL, sizeof(FIRST_API_LEVEL), "%d",
                             deviceInitialSdkInt->valueint);
                } else if (cJSON_IsString(deviceInitialSdkInt)) {
                    strcpy(FIRST_API_LEVEL, deviceInitialSdkInt->valuestring);
                }
            } else {
                LOGD("Couldn't parse FIRST_API_LEVEL neither DEVICE_INITIAL_SDK_INT");
            }
        }

        cJSON *securityPatch = cJSON_GetObjectItem(json, "SECURITY_PATCH");
        if (securityPatch && cJSON_IsString(securityPatch)) {
            strcpy(SECURITY_PATCH, securityPatch->valuestring);
        } else {
            LOGD("Couldn't parse SECURITY_PATCH");
        }

        cJSON *id = cJSON_GetObjectItem(json, "ID");
        if (id && cJSON_IsString(id)) {
            strcpy(BUILD_ID, id->valuestring);
        } else {
            cJSON *buildId = cJSON_GetObjectItem(json, "BUILD_ID");
            if (buildId && cJSON_IsString(buildId)) {
                strcpy(BUILD_ID, buildId->valuestring);
            } else {
                LOGD("Couldn't parse ID");
            }
        }
    }
};

static void companion(int fd) {

    long dexSize = 0;
    long jsonSize = 0;

    char *dexBuffer = nullptr;
    char *jsonBuffer = nullptr;

    FILE *dexFile = fopen(CLASSES_DEX, "rb");

    if (dexFile) {
        fseek(dexFile, 0, SEEK_END);
        dexSize = ftell(dexFile);
        fseek(dexFile, 0, SEEK_SET);

        dexBuffer = new char[dexSize];
        fread(dexBuffer, 1, dexSize, dexFile);

        fclose(dexFile);
    }

    FILE *jsonFile = fopen(PIF_JSON, "rb");
    if (jsonFile == nullptr) jsonFile = fopen(PIF_JSON_DEFAULT, "rb");

    if (jsonFile) {
        fseek(jsonFile, 0, SEEK_END);
        jsonSize = ftell(jsonFile);
        fseek(jsonFile, 0, SEEK_SET);

        jsonBuffer = new char[jsonSize];
        fread(jsonBuffer, 1, jsonSize, jsonFile);

        fclose(jsonFile);
    }

    write(fd, &dexSize, sizeof(long));
    write(fd, &jsonSize, sizeof(long));

    write(fd, dexBuffer, dexSize);
    write(fd, jsonBuffer, jsonSize);

    delete[] dexBuffer;
    delete[] jsonBuffer;
}

REGISTER_ZYGISK_MODULE(PlayIntegrityFix)

REGISTER_ZYGISK_COMPANION(companion)