#include <android/log.h>
#include <sys/system_properties.h>
#include <unistd.h>
#include <map>
#include "dobby.h"
#include "json.hpp"
#include "zygisk.hpp"

#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, "PIF", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "PIF", __VA_ARGS__)

#define DEX_PATH "/data/adb/modules/playintegrityfix/classes.dex"

#define PIF_JSON "/data/adb/pif.json"

#define PIF_JSON_DEFAULT "/data/adb/modules/playintegrityfix/pif.json"

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

static int verboseLogs = 0;

static std::map<std::string, std::string> props;

typedef void (*T_Callback)(void *, const char *, const char *, uint32_t);

static std::map<void *, T_Callback> callbacks;

static void modify_callback(void *cookie, const char *name, const char *value, uint32_t serial) {

    if (cookie == nullptr || name == nullptr || value == nullptr ||
        !callbacks.contains(cookie))
        return;

    std::string_view prop(name);

    if (prop == "init.svc.adbd") {
        value = "stopped";
    } else if (prop == "sys.usb.state") {
        value = "mtp";
    }

    if (props.contains(name)) {
        value = props[name].c_str();
        if (verboseLogs > 49) LOGD("[%s]: %s", name, value);
    } else {
        for (const auto &item: props) {
            if (item.first.starts_with("*") && prop.ends_with(item.first.substr(1))) {
                value = item.second.c_str();
                if (verboseLogs > 49) LOGD("[%s]: %s", name, value);
                break;
            }
        }
    }

    if (verboseLogs > 99) LOGD("[%s]: %s", name, value);

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
    auto handle = DobbySymbolResolver(nullptr, "__system_property_read_callback");
    if (!handle) {
        LOGE("Couldn't find __system_property_read_callback symbol!");
        return;
    }
    if (DobbyHook(handle, (dobby_dummy_func_t) my_system_property_read_callback,
                  (dobby_dummy_func_t *) &o_system_property_read_callback)) {
        LOGE("Couldn't hook __system_property_read_callback!");
        return;
    }
    LOGD("Hooked __system_property_read_callback at %p", handle);
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
            json = nlohmann::json::parse(jsonVector, nullptr, false, true);
        }

        close(fd);

        LOGD("Dex file size: %d", dexSize);
        LOGD("Json file size: %d", jsonSize);

        parseJSON();
    }

    void postAppSpecialize(const zygisk::AppSpecializeArgs *args) override {
        if (dexVector.empty()) return;

        doHook();

        injectDex();
    }

    void preServerSpecialize(zygisk::ServerSpecializeArgs *args) override {
        api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
    }

private:
    zygisk::Api *api = nullptr;
    JNIEnv *env = nullptr;
    std::vector<uint8_t> dexVector;
    nlohmann::json json;

    void parseJSON() {
        if (json.empty()) return;

        if (json.contains("verboseLogs") && !json["verboseLogs"].empty()) {
            if (json["verboseLogs"].is_string()) {
                verboseLogs = std::stoi(json["verboseLogs"].get<std::string>());
            } else if (json["verboseLogs"].is_number_integer()) {
                verboseLogs = json["verboseLogs"].get<int>();
            }
            json.erase("verboseLogs");
        }

        if (verboseLogs > 0) LOGD("Verbose logging (level %d) enabled", verboseLogs);

        std::vector<std::string> removeKeys;

        // Java fields are always uppercase, system props are always lowercase
        for (const auto &item: json.items()) {
            if (std::all_of(item.key().cbegin(), item.key().cend(), [](unsigned char c) {
                return c == '_' || (std::isalpha(c) && std::isupper(c));
            })) {
                // Java field
                if (verboseLogs > 99) LOGD("Skip Java field: %s", item.key().c_str());
                continue;
            }
            // System prop
            if (item.value().is_string()) {
                props.insert({item.key(), item.value().get<std::string>()});
                if (verboseLogs > 99) LOGD("Got system prop: %s", item.key().c_str());
            }
            removeKeys.push_back(item.key());
        }

        LOGD("Loaded %d props to spoof", props.size());

        for (const auto &item: removeKeys) json.erase(item);
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
        auto entryInit = env->GetStaticMethodID(entryPointClass, "init", "(Ljava/lang/String;)V");
        auto jsonStr = env->NewStringUTF(json.dump().c_str());
        env->CallStaticVoidMethod(entryPointClass, entryInit, jsonStr);
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

    auto dex = readFile(DEX_PATH);

    auto json = readFile(PIF_JSON);
    if (json.empty()) json = readFile(PIF_JSON_DEFAULT);

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
}

REGISTER_ZYGISK_MODULE(PlayIntegrityFix)

REGISTER_ZYGISK_COMPANION(companion)
