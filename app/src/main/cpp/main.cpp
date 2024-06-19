#include <android/log.h>
#include <sys/system_properties.h>
#include <unistd.h>
#include "dobby.h"
#include "json.hpp"
#include "zygisk.hpp"
#include "dex.h"

#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, "PIF", __VA_ARGS__)

#define PIF_JSON "/data/adb/pif.json"

#define PIF_JSON_DEFAULT "/data/adb/modules/playintegrityfix/pif.json"

static std::string DEVICE_INITIAL_SDK_INT, SECURITY_PATCH, ID;

static inline ssize_t xread(int fd, void *buffer, size_t count) {
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

static inline ssize_t xwrite(int fd, void *buffer, size_t count) {
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

typedef void (*T_Callback)(void *, const char *, const char *, uint32_t);

static T_Callback o_callback = nullptr;

static void modify_callback(void *cookie, const char *name, const char *value, uint32_t serial) {

    if (cookie == nullptr || name == nullptr || value == nullptr || o_callback == nullptr) return;

    std::string_view prop(name);

    if (prop.ends_with("api_level") && !DEVICE_INITIAL_SDK_INT.empty()) {
        value = DEVICE_INITIAL_SDK_INT.c_str();
    } else if (prop.ends_with(".security_patch") && !SECURITY_PATCH.empty()) {
        value = SECURITY_PATCH.c_str();
    } else if (prop.ends_with(".id") && !ID.empty()) {
        value = ID.c_str();
    } else if (prop == "sys.usb.state") {
        value = "none";
    }

    if (!prop.starts_with("persist") && !prop.starts_with("cache") && !prop.starts_with("debug")) {
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

        api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);

        if (!args) {
            api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
            return;
        }

        const char *dir = env->GetStringUTFChars(args->app_data_dir, nullptr);

        if (!dir) {
            api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
            return;
        }

        if (!std::string_view(dir).ends_with("/com.google.android.gms")) {
            env->ReleaseStringUTFChars(args->app_data_dir, dir);
            api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
            return;
        }

        env->ReleaseStringUTFChars(args->app_data_dir, dir);

        api->setOption(zygisk::FORCE_DENYLIST_UNMOUNT);

        const char *name = env->GetStringUTFChars(args->nice_name, nullptr);

        if (!name) {
            api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
            return;
        }

        if (strncmp(name, "com.google.android.gms.unstable", 31) != 0) {
            env->ReleaseStringUTFChars(args->nice_name, name);
            api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
            return;
        }

        env->ReleaseStringUTFChars(args->nice_name, name);

        long size = 0;
        std::vector<char> vector;

        int fd = api->connectCompanion();

        xread(fd, &size, sizeof(long));

        if (size > 0) {
            vector.resize(size);
            xread(fd, vector.data(), size);
            json = nlohmann::json::parse(vector, nullptr, false, true);
        } else {
            api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
        }

        close(fd);
    }

    void postAppSpecialize(const zygisk::AppSpecializeArgs *args) override {
        if (json.empty()) return;

        parseJson();

        injectDex();

//        doHook();
    }

    void preServerSpecialize(zygisk::ServerSpecializeArgs *args) override {
        api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
    }

private:
    zygisk::Api *api = nullptr;
    JNIEnv *env = nullptr;
    nlohmann::json json;

    void parseJson() {
        if (json.contains("DEVICE_INITIAL_SDK_INT")) {
            DEVICE_INITIAL_SDK_INT = json["DEVICE_INITIAL_SDK_INT"].get<std::string>();
            json.erase("DEVICE_INITIAL_SDK_INT"); // You can't modify field value
        }
        if (json.contains("SECURITY_PATCH")) {
            SECURITY_PATCH = json["SECURITY_PATCH"].get<std::string>();
        }
        if (json.contains("ID")) {
            ID = json["ID"].get<std::string>();
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
        auto buffer = env->NewDirectByteBuffer(classes_dex, classes_dex_len);
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
};

static std::vector<char> readFile(const char *path) {

    std::vector<char> vector;

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
    long size = 0;
    std::vector<char> vector;

    vector = readFile(PIF_JSON);

    if (vector.empty()) vector = readFile(PIF_JSON_DEFAULT);

    size = vector.size();

    xwrite(fd, &size, sizeof(long));
    xwrite(fd, vector.data(), size);
}

REGISTER_ZYGISK_MODULE(PlayIntegrityFix)

REGISTER_ZYGISK_COMPANION(companion)
