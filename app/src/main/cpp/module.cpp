#include <android/log.h>
#include <sys/system_properties.h>
#include <unistd.h>

#include "zygisk.hpp"
#include "dobby.h"
#include "json.hpp"

#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, "PIF/Native", __VA_ARGS__)

#define DEF_SECURITY_PATCH "2018-02-05"
#define DEF_FIRST_API_LEVEL "23"
#define DEF_VNDK_VERSION "23"
#define DEF_BUILD_ID "NMF26F"

static std::string SECURITY_PATCH, FIRST_API_LEVEL, VNDK_VERSION, BUILD_ID;

typedef void (*T_Callback)(void *, const char *, const char *, uint32_t);

static std::map<void *, T_Callback> callbacks;

static void modify_callback(void *cookie, const char *name, const char *value, uint32_t serial) {

    if (cookie == nullptr || name == nullptr || value == nullptr ||
        !callbacks.contains(cookie))
        return;

    std::string_view prop(name);

    if (prop.ends_with("security_patch")) {
        if (SECURITY_PATCH.empty()) {
            value = DEF_SECURITY_PATCH;
        } else {
            value = SECURITY_PATCH.c_str();
        }
        LOGD("[%s]: %s", name, value);
    } else if (prop.ends_with("api_level")) {
        if (FIRST_API_LEVEL.empty()) {
            value = DEF_FIRST_API_LEVEL;
        } else {
            value = FIRST_API_LEVEL.c_str();
        }
        LOGD("[%s]: %s", name, value);
    } else if (prop.ends_with("vndk.version")) {
        if (VNDK_VERSION.empty()) {
            value = DEF_VNDK_VERSION;
        } else {
            value = VNDK_VERSION.c_str();
        }
        LOGD("[%s]: %s", name, value);
    } else if (prop == "ro.build.id") {
        if (BUILD_ID.empty()) {
            value = DEF_BUILD_ID;
        } else {
            value = BUILD_ID.c_str();
        }
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

        auto rawProcess = env->GetStringUTFChars(args->nice_name, nullptr);
        std::string process(rawProcess);
        env->ReleaseStringUTFChars(args->nice_name, rawProcess);

        if (process.starts_with("com.google.android.gms")) {

            api->setOption(zygisk::FORCE_DENYLIST_UNMOUNT);

            if (process == "com.google.android.gms.unstable") {

                int fd = api->connectCompanion();

                read(fd, &dexSize, sizeof(dexSize));
                read(fd, &jsonSize, sizeof(jsonSize));

                auto vectorSize = dexSize + jsonSize;

                if (vectorSize > 0) {
                    vector.resize(vectorSize);
                    read(fd, vector.data(), vectorSize);
                } else {
                    LOGD("Couldn't read classes.dex");
                    api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
                }

                close(fd);
                return;
            }
        }

        api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
    }

    void postAppSpecialize(const zygisk::AppSpecializeArgs *args) override {
        if (vector.empty()) return;

        LOGD("Read %ld bytes of classes.dex!", dexSize);
        LOGD("Read %ld bytes of pif.json!", jsonSize);

        if (jsonSize > 0) parseJson();

        doHook();

        injectDex();

        vector.clear();
        json.clear();
    }

    void preServerSpecialize(zygisk::ServerSpecializeArgs *args) override {
        api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
    }

private:
    zygisk::Api *api = nullptr;
    JNIEnv *env = nullptr;
    long dexSize = 0, jsonSize = 0;
    std::vector<char> vector;
    nlohmann::json json;

    void parseJson() {
        std::string_view data(vector.cbegin() + dexSize, vector.cend());
        json = nlohmann::json::parse(data, nullptr, false, true);

        if (json.contains("SECURITY_PATCH")) {
            if (json["SECURITY_PATCH"].is_null() || json["SECURITY_PATCH"].empty()) {
                LOGD("SECURITY_PATCH is null or empty");
            } else {
                SECURITY_PATCH = json["SECURITY_PATCH"].get<std::string>();
            }
        }

        if (json.contains("FIRST_API_LEVEL")) {
            if (json["FIRST_API_LEVEL"].is_null() || json["FIRST_API_LEVEL"].empty()) {
                LOGD("FIRST_API_LEVEL is null or empty");
            } else {
                FIRST_API_LEVEL = json["FIRST_API_LEVEL"].get<std::string>();
            }
            json.erase("FIRST_API_LEVEL");
        }

        if (json.contains("BUILD_ID")) {
            if (json["BUILD_ID"].is_null() || json["BUILD_ID"].empty()) {
                LOGD("BUILD_ID is null or empty");
            } else {
                BUILD_ID = json["BUILD_ID"].get<std::string>();
            }
            json.erase("BUILD_ID");
        }

        if (json.contains("VNDK_VERSION")) {
            if (json["VNDK_VERSION"].is_null() || json["VNDK_VERSION"].empty()) {
                LOGD("VNDK_VERSION is null or empty");
            } else {
                VNDK_VERSION = json["VNDK_VERSION"].get<std::string>();
            }
            json.erase("VNDK_VERSION");
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
        auto buff = env->NewDirectByteBuffer(vector.data(), dexSize);
        auto dexCl = env->NewObject(dexClClass, dexClInit, buff, systemClassLoader);

        LOGD("load class");
        auto loadClass = env->GetMethodID(clClass, "loadClass",
                                          "(Ljava/lang/String;)Ljava/lang/Class;");
        auto entryClassName = env->NewStringUTF("es.chiteroman.playintegrityfix.EntryPoint");
        auto entryClassObj = env->CallObjectMethod(dexCl, loadClass, entryClassName);

        auto entryClass = (jclass) entryClassObj;

        LOGD("call init");
        auto str = env->NewStringUTF(json.dump().c_str());
        auto entryInit = env->GetStaticMethodID(entryClass, "init", "(Ljava/lang/String;)V");
        env->CallStaticVoidMethod(entryClass, entryInit, str);
    }
};

static void companion(int fd) {
    long dexSize = 0, jsonSize = 0;
    std::vector<char> vector;

    FILE *dex = fopen("/data/adb/modules/playintegrityfix/classes.dex", "rb");

    if (dex) {
        fseek(dex, 0, SEEK_END);
        dexSize = ftell(dex);
        fseek(dex, 0, SEEK_SET);

        vector.resize(dexSize);
        fread(vector.data(), 1, dexSize, dex);

        fclose(dex);
    }

    FILE *json = fopen("/data/adb/pif.json", "rb");

    if (json) {
        fseek(json, 0, SEEK_END);
        jsonSize = ftell(json);
        fseek(json, 0, SEEK_SET);

        vector.resize(dexSize + jsonSize);
        fread(vector.data() + dexSize, 1, jsonSize, json);

        fclose(json);
    }

    write(fd, &dexSize, sizeof(dexSize));
    write(fd, &jsonSize, sizeof(jsonSize));

    write(fd, vector.data(), vector.size());
}

REGISTER_ZYGISK_MODULE(PlayIntegrityFix)

REGISTER_ZYGISK_COMPANION(companion)
