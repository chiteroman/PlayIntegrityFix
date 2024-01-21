#include <android/log.h>
#include <unistd.h>
#include "zygisk.hpp"
#include "dobby.h"
#include "json.hpp"

#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, "PIF/Native", __VA_ARGS__)

#define CLASSES_DEX "/data/adb/modules/playintegrityfix/classes.dex"

#define PIF_JSON "/data/adb/pif.json"

static std::string FIRST_API_LEVEL, SECURITY_PATCH, BUILD_ID;

typedef void (*T_Callback)(void *, const char *, const char *, uint32_t);

static T_Callback o_callback = nullptr;

static void modify_callback(void *cookie, const char *name, const char *value, uint32_t serial) {

    if (cookie == nullptr || name == nullptr || value == nullptr || o_callback == nullptr) return;

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

    if (!prop.starts_with("debug") && !prop.starts_with("cache") && !prop.starts_with("persist")) {

        LOGD("[%s] -> %s", name, value);
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
    void *handle = DobbySymbolResolver("libc.so", "__system_property_read_callback");
    if (handle == nullptr) {
        LOGD("Couldn't find '__system_property_read_callback' handle. Report to @chiteroman");
        return;
    }
    LOGD("Found '__system_property_read_callback' handle at %p", handle);
    DobbyHook(
            handle,
            (dobby_dummy_func_t) my_system_property_read_callback,
            (dobby_dummy_func_t *) &o_system_property_read_callback
    );
}

class PlayIntegrityFix : public zygisk::ModuleBase {
public:
    void onLoad(zygisk::Api *api, JNIEnv *env) override {
        this->api = api;
        this->env = env;
    }

    void preAppSpecialize(zygisk::AppSpecializeArgs *args) override {

        auto name = env->GetStringUTFChars(args->nice_name, nullptr);

        bool isGms = memcmp(name, "com.google.android.gms", 22) == 0;
        bool isGmsUnstable = memcmp(name, "com.google.android.gms.unstable", 31) == 0;

        env->ReleaseStringUTFChars(args->nice_name, name);

        if (isGms) api->setOption(zygisk::FORCE_DENYLIST_UNMOUNT);

        if (isGmsUnstable) {
            long dexSize = 0, jsonSize = 0;

            int fd = api->connectCompanion();

            read(fd, &dexSize, sizeof(long));
            read(fd, &jsonSize, sizeof(long));

            LOGD("Dex file size: %ld", dexSize);
            LOGD("Json file size: %ld", jsonSize);

            vector.resize(dexSize);
            read(fd, vector.data(), dexSize);

            std::vector<char> jsonVector(jsonSize);
            read(fd, jsonVector.data(), jsonSize);

            close(fd);

            std::string_view jsonStr(jsonVector.cbegin(), jsonVector.cend());
            json = nlohmann::json::parse(jsonStr, nullptr, false, true);

            parseJson();

            return; // We can't dlclose lib because of the hook
        }

        api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
    }

    void postAppSpecialize(const zygisk::AppSpecializeArgs *args) override {
        if (vector.empty() || json.empty()) return;

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
    std::vector<char> vector;
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
        auto buffer = env->NewDirectByteBuffer(vector.data(), vector.size());
        auto dexCl = env->NewObject(dexClClass, dexClInit, buffer, systemClassLoader);

        LOGD("load class");
        auto loadClass = env->GetMethodID(clClass, "loadClass",
                                          "(Ljava/lang/String;)Ljava/lang/Class;");
        auto entryClassName = env->NewStringUTF("es.chiteroman.playintegrityfix.EntryPoint");
        auto entryClassObj = env->CallObjectMethod(dexCl, loadClass, entryClassName);

        auto entryClass = (jclass) entryClassObj;

        LOGD("call init");
        auto entryInit = env->GetStaticMethodID(entryClass, "init", "(Ljava/lang/String;)V");
        auto str = env->NewStringUTF(json.dump().c_str());
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

        } else {

            LOGD("JSON file doesn't contain ID/BUILD_ID keys :(");
        }
    }
};

static std::vector<char> readFile(const char* filename, long& size) {
    std::vector<char> vector;
    FILE *file = fopen(filename, "rb");

    if (file) {
        fseek(file, 0, SEEK_END);
        size = ftell(file);
        fseek(file, 0, SEEK_SET);

        vector.resize(size);
        size_t readSize = fread(vector.data(), 1, size, file);
        if (readSize != size) {
            LOGD("Error reading file: %s", filename);
            vector.clear();
        }

        fclose(file);
    } else {
        LOGD("Error opening file: %s", filename);
    }

    return vector;
}

static void companion(int fd) {
    long dexSize = 0, jsonSize = 0;
    std::vector<char> dexVector = readFile(CLASSES_DEX, dexSize);
    std::vector<char> jsonVector = readFile(PIF_JSON, jsonSize);

    write(fd, &dexSize, sizeof(long));
    write(fd, &jsonSize, sizeof(long));

    if (!dexVector.empty()) {
        write(fd, dexVector.data(), dexSize);
    }

    if (!jsonVector.empty()) {
        write(fd, jsonVector.data(), jsonSize);
    }
}


REGISTER_ZYGISK_MODULE(PlayIntegrityFix)

REGISTER_ZYGISK_COMPANION(companion)
