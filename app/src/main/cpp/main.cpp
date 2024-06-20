#include <android/log.h>
#include <sys/system_properties.h>
#include <unistd.h>
#include <fstream>
#include "dobby.h"
#include "json.hpp"
#include "zygisk.hpp"

#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, "PIF", __VA_ARGS__)

#define DEX_PATH "/data/adb/modules/playintegrityfix/classes.dex"

#define PIF_JSON "/data/adb/pif.json"

#define PIF_JSON_DEFAULT "/data/adb/modules/playintegrityfix/pif.json"

#define KEYBOX_JSON "/data/adb/keybox.xml"

#define KEYBOX_JSON_DEFAULT "/data/adb/modules/playintegrityfix/keybox.xml"

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

typedef void (*T_Callback)(void *, const char *, const char *, uint32_t);

static T_Callback o_callback = nullptr;

static void modify_callback(void *cookie, const char *name, const char *value, uint32_t serial) {

    if (cookie == nullptr || name == nullptr || value == nullptr || o_callback == nullptr) return;

    std::string_view prop(name);

    if (prop.ends_with("first_api_level") && !DEVICE_INITIAL_SDK_INT.empty()) {
        value = DEVICE_INITIAL_SDK_INT.c_str();
        LOGD("[%s]: %s", name, value);
    }

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

        int dexSize = 0, jsonSize = 0, keyboxSize = 0;
        std::vector<char> jsonVector, keyboxVector;

        xread(fd, &dexSize, sizeof(int));
        xread(fd, &jsonSize, sizeof(int));
        xread(fd, &keyboxSize, sizeof(int));

        dexVector.resize(dexSize);
        xread(fd, dexVector.data(), dexSize);

        jsonVector.resize(jsonSize);
        xread(fd, jsonVector.data(), jsonSize);

        keyboxVector.resize(keyboxSize);
        xread(fd, keyboxVector.data(), keyboxSize);

        close(fd);

        json = nlohmann::json::parse(jsonVector, nullptr, false, true);

        keyboxString = std::string(keyboxVector.cbegin(), keyboxVector.cend());
    }

    void postAppSpecialize(const zygisk::AppSpecializeArgs *args) override {
        if (dexVector.empty() || json.empty()) return;

        if (json.contains("DEVICE_INITIAL_SDK_INT")) {
            DEVICE_INITIAL_SDK_INT = json["DEVICE_INITIAL_SDK_INT"].get<std::string>();
            json.erase("DEVICE_INITIAL_SDK_INT"); // You can't modify field value
        }

        doHook();

        injectDex();
    }

    void preServerSpecialize(zygisk::ServerSpecializeArgs *args) override {
        api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
    }

private:
    zygisk::Api *api = nullptr;
    JNIEnv *env = nullptr;
    std::vector<char> dexVector;
    nlohmann::json json;
    std::string keyboxString;

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
        auto entryInit = env->GetStaticMethodID(entryPointClass, "init",
                                                "(Ljava/lang/String;Ljava/lang/String;)V");
        auto jsonStr = env->NewStringUTF(json.dump().c_str());
        auto keyboxStr = env->NewStringUTF(keyboxString.c_str());
        env->CallStaticVoidMethod(entryPointClass, entryInit, jsonStr, keyboxStr);
    }
};

static std::vector<char> readFile(const std::string &path) {

    std::ifstream ifs(path);

    if (!ifs || ifs.bad()) {
        return std::vector<char>();
    }

    return std::vector<char>((std::istreambuf_iterator<char>(ifs)),
                             std::istreambuf_iterator<char>());
}

static void companion(int fd) {

    auto dex = readFile(DEX_PATH);

    auto json = readFile(PIF_JSON);
    if (json.empty()) json = readFile(PIF_JSON_DEFAULT);

    auto keybox = readFile(KEYBOX_JSON);
    if (keybox.empty()) keybox = readFile(KEYBOX_JSON_DEFAULT);

    int dexSize = dex.size();
    int jsonSize = json.size();
    int keyboxSize = keybox.size();

    xwrite(fd, &dexSize, sizeof(int));
    xwrite(fd, &jsonSize, sizeof(int));
    xwrite(fd, &keyboxSize, sizeof(int));

    xwrite(fd, dex.data(), dexSize * sizeof(char));
    xwrite(fd, json.data(), jsonSize * sizeof(char));
    xwrite(fd, keybox.data(), keyboxSize * sizeof(char));
}

REGISTER_ZYGISK_MODULE(PlayIntegrityFix)

REGISTER_ZYGISK_COMPANION(companion)
