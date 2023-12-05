#include <android/log.h>
#include <sys/system_properties.h>
#include <unistd.h>
#include <fstream>

#include "zygisk.hpp"
#include "shadowhook.h"
#include "json.hpp"

#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, "PIF/Native", __VA_ARGS__)

#define DEX_FILE_PATH "/data/adb/modules/playintegrityfix/classes.dex"

#define JSON_FILE_PATH "/data/adb/pif.json"

static std::string FIRST_API_LEVEL, SECURITY_PATCH, BUILD_ID, VNDK_VERSION;

typedef void (*T_Callback)(void *, const char *, const char *, uint32_t);

static T_Callback o_callback = nullptr;

static void modify_callback(void *cookie, const char *name, const char *value, uint32_t serial) {

    if (cookie == nullptr || name == nullptr || value == nullptr || o_callback == nullptr) return;

    std::string_view prop(name);

    if (prop.ends_with("api_level")) {
        if (!FIRST_API_LEVEL.empty()) {
            value = FIRST_API_LEVEL.c_str();
        }
    } else if (prop.ends_with("security_patch")) {
        if (!SECURITY_PATCH.empty()) {
            value = SECURITY_PATCH.c_str();
        }
    } else if (prop.ends_with("vndk.version")) {
        if (!VNDK_VERSION.empty()) {
            value = VNDK_VERSION.c_str();
        }
    } else if (prop == "ro.build.id") {
        if (!BUILD_ID.empty()) {
            value = BUILD_ID.c_str();
        }
    }

    LOGD("[%s]: %s", name, value);

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
    shadowhook_init(SHADOWHOOK_MODE_UNIQUE, false);
    void *handle = shadowhook_hook_sym_name("libc.so", "__system_property_read_callback",
                                            reinterpret_cast<void *>(my_system_property_read_callback),
                                            reinterpret_cast<void **>(&o_system_property_read_callback));
    if (handle == nullptr) {
        LOGD("Couldn't find '__system_property_read_callback' handle. Report to @chiteroman");
        return;
    }
    LOGD("Found '__system_property_read_callback' handle at %p", handle);
}

class PlayIntegrityFix : public zygisk::ModuleBase {
public:
    void onLoad(zygisk::Api *api, JNIEnv *env) override {
        this->api = api;
        this->env = env;
    }

    void preAppSpecialize(zygisk::AppSpecializeArgs *args) override {
        auto rawProc = env->GetStringUTFChars(args->nice_name, nullptr);

        std::string_view process(rawProc);

        if (process.starts_with("com.google.android.gms")) {

            api->setOption(zygisk::FORCE_DENYLIST_UNMOUNT);

            if (process == "com.google.android.gms.unstable") {

                int dexSize = 0, jsonSize = 0;
                int fd = api->connectCompanion();

                read(fd, &dexSize, sizeof(int));
                read(fd, &jsonSize, sizeof(int));

                if (dexSize > 0 && jsonSize > 0) {
                    dexVector.resize(dexSize);
                    read(fd, dexVector.data(), dexSize);

                    jsonVector.resize(jsonSize);
                    read(fd, jsonVector.data(), jsonSize);
                } else {
                    LOGD("Couldn't load files in memory!");
                    api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
                }

                close(fd);

            } else api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);

        } else api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);

        env->ReleaseStringUTFChars(args->nice_name, rawProc);
    }

    void postAppSpecialize(const zygisk::AppSpecializeArgs *args) override {
        if (dexVector.empty() || jsonVector.empty()) return;

        doHook();

        LOGD("Read %d bytes of classes.dex", static_cast<int>(dexVector.size()));
        LOGD("Read %d bytes of pif.json", static_cast<int>(jsonVector.size()));

        std::string_view data(jsonVector.cbegin(), jsonVector.cend());
        nlohmann::json json = nlohmann::json::parse(data, nullptr, false, true);

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
        } else {
            LOGD("Key FIRST_API_LEVEL doesn't exist in JSON file!");
        }

        if (json.contains("BUILD_ID")) {
            if (json["BUILD_ID"].is_null()) {
                LOGD("Key BUILD_ID is null!");
            } else if (json["BUILD_ID"].is_string()) {
                BUILD_ID = json["BUILD_ID"].get<std::string>();
            } else {
                LOGD("Error parsing BUILD_ID!");
            }
        } else {
            LOGD("Key BUILD_ID doesn't exist in JSON file!");
        }

        if (json.contains("VNDK_VERSION")) {
            if (json["VNDK_VERSION"].is_null()) {
                LOGD("Key VNDK_VERSION is null!");
            } else if (json["VNDK_VERSION"].is_string()) {
                VNDK_VERSION = json["VNDK_VERSION"].get<std::string>();
            } else {
                LOGD("Error parsing VNDK_VERSION!");
            }
        } else {
            LOGD("Key VNDK_VERSION doesn't exist in JSON file!");
        }

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

        dexVector.clear();
        jsonVector.clear();
    }

    void preServerSpecialize(zygisk::ServerSpecializeArgs *args) override {
        api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
    }

private:
    zygisk::Api *api = nullptr;
    JNIEnv *env = nullptr;
    std::vector<char> dexVector, jsonVector;
};

static void companion(int fd) {
    std::ifstream dex(DEX_FILE_PATH, std::ios::binary);
    std::ifstream json(JSON_FILE_PATH, std::ios::binary);

    std::vector<char> dexVector((std::istreambuf_iterator<char>(dex)),
                                std::istreambuf_iterator<char>());

    std::vector<char> jsonVector((std::istreambuf_iterator<char>(json)),
                                 std::istreambuf_iterator<char>());

    int dexSize = static_cast<int>(dexVector.size());
    int jsonSize = static_cast<int>(jsonVector.size());

    write(fd, &dexSize, sizeof(int));
    write(fd, &jsonSize, sizeof(int));

    write(fd, dexVector.data(), dexSize);
    write(fd, jsonVector.data(), jsonSize);
}

REGISTER_ZYGISK_MODULE(PlayIntegrityFix)

REGISTER_ZYGISK_COMPANION(companion)