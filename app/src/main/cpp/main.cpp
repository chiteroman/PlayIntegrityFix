#include <android/log.h>
#include <sys/system_properties.h>
#include <unistd.h>
#include <fstream>

#include "zygisk.hpp"
#include "shadowhook.h"
#include "json.hpp"

#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, "PIF/Native", __VA_ARGS__)

#define DEX_FILE_PATH "/data/adb/modules/playintegrityfix/classes.dex"

#define JSON_FILE_PATH "/data/adb/modules/playintegrityfix/pif.json"

#define MAX_FIRST_API_LEVEL 32

static std::string FIRST_API_LEVEL, SECURITY_PATCH;

typedef void (*T_Callback)(void *, const char *, const char *, uint32_t);

static std::map<void *, T_Callback> callbacks;

static void modify_callback(void *cookie, const char *name, const char *value, uint32_t serial) {

    if (cookie == nullptr || name == nullptr || value == nullptr ||
        !callbacks.contains(cookie))
        return;

    std::string_view prop(name);
    std::string default_value = value;
    if (prop.ends_with("api_level")) {
        if (FIRST_API_LEVEL == "nullptr" || FIRST_API_LEVEL == "") {
            //value = nullptr;
            LOGD("First Api level is defined as null");
            if (std::stoi(value) > MAX_FIRST_API_LEVEL) {
                LOGD("Default %s is over %d. Defaulting to %d", name,MAX_FIRST_API_LEVEL, MAX_FIRST_API_LEVEL);
                value = "32";
            }
        } else {
            value = FIRST_API_LEVEL.c_str();
        }
        if (default_value != value) {
            LOGD("[%s] %s -> %s", name, default_value.c_str(), value);
        }
    } else if (prop.ends_with("security_patch")) {
        if (SECURITY_PATCH == "nullptr" || SECURITY_PATCH == "") {
            //value = nullptr;
            LOGD("Security patch is defined as null");
        } else {
            value = SECURITY_PATCH.c_str();
        }
        if (default_value != value) {
            LOGD("[%s] %s -> %s", name, default_value.c_str(), value);
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
    shadowhook_init(SHADOWHOOK_MODE_UNIQUE, false);
    void *handle = shadowhook_hook_sym_name(
            "libc.so",
            "__system_property_read_callback",
            reinterpret_cast<void *>(my_system_property_read_callback),
            reinterpret_cast<void **>(&o_system_property_read_callback)
    );
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
        auto rawProcess = env->GetStringUTFChars(args->nice_name, nullptr);

        std::string_view process(rawProcess);

        bool isGms = process.starts_with("com.google.android.gms");
        bool isGmsUnstable = process.compare("com.google.android.gms.unstable") == 0;

        env->ReleaseStringUTFChars(args->nice_name, rawProcess);

        if (!isGms) {
            api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
            return;
        }

        api->setOption(zygisk::FORCE_DENYLIST_UNMOUNT);

        if (!isGmsUnstable) {
            api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
            return;
        }

        int dexSize = 0;
        int jsonSize = 0;

        int fd = api->connectCompanion();

        read(fd, &dexSize, sizeof(int));
        read(fd, &jsonSize, sizeof(int));

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

        dexVector.resize(dexSize);
        jsonVector.resize(jsonSize);

        read(fd, dexVector.data(), dexSize);
        read(fd, jsonVector.data(), jsonSize);

        close(fd);

        LOGD("Read from file descriptor file 'classes.dex' -> %d bytes", dexSize);
        LOGD("Read from file descriptor file 'pif.json' -> %d bytes", jsonSize);
    }

    void postAppSpecialize(const zygisk::AppSpecializeArgs *args) override {
        if (dexVector.empty() || jsonVector.empty()) return;

        readJson();

        doHook();

        inject();

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

    void readJson() {
        std::string data(jsonVector.cbegin(), jsonVector.cend());
        nlohmann::json json = nlohmann::json::parse(data, nullptr, false, true);

        if (json.contains("SECURITY_PATCH")) {
            if (json["SECURITY_PATCH"].is_null()) {
                SECURITY_PATCH = "nullptr";
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
                FIRST_API_LEVEL = "nullptr";
            } else if (json["FIRST_API_LEVEL"].is_string()) {
                FIRST_API_LEVEL = json["FIRST_API_LEVEL"].get<std::string>();
            } else {
                LOGD("Error parsing FIRST_API_LEVEL!");
            }
        } else {
            LOGD("Key FIRST_API_LEVEL doesn't exist in JSON file!");
        }

        json.clear();
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

        LOGD("read json");
        auto readProps = env->GetStaticMethodID(entryClass, "readJson",
                                                "(Ljava/lang/String;)V");
        std::string data(jsonVector.cbegin(), jsonVector.cend());
        auto javaStr = env->NewStringUTF(data.c_str());
        env->CallStaticVoidMethod(entryClass, readProps, javaStr);

        LOGD("call init");
        auto entryInit = env->GetStaticMethodID(entryClass, "init", "()V");
        env->CallStaticVoidMethod(entryClass, entryInit);
    }
};

static void companion(int fd) {
    std::ifstream dex(DEX_FILE_PATH, std::ios::binary);
    std::ifstream json(JSON_FILE_PATH);

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