#include <android/log.h>
#include <sys/system_properties.h>
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

    if (cookie == nullptr || name == nullptr || o_callback == nullptr) return;

    std::string_view prop(name);

    if (prop.ends_with("security_patch")) {

        if (!SECURITY_PATCH.empty()) {

            value = SECURITY_PATCH.c_str();
            LOGD("Set '%s' to '%s'", name, value);
        }

    } else if (prop.ends_with("api_level")) {

        if (!FIRST_API_LEVEL.empty()) {

            value = FIRST_API_LEVEL.c_str();
            LOGD("Set '%s' to '%s'", name, value);
        }

    } else if (prop == "ro.build.id") {

        if (!BUILD_ID.empty()) {

            value = BUILD_ID.c_str();
            LOGD("Set '%s' to '%s'", name, value);
        }

    } else if (prop == "sys.usb.state") {

        value = "none";
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

        if (args->is_child_zygote && *args->is_child_zygote) {
            api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
            return;
        }

        auto name = env->GetStringUTFChars(args->nice_name, nullptr);

        if (name && strncmp(name, "com.google.android.gms", 22) == 0) {

            api->setOption(zygisk::FORCE_DENYLIST_UNMOUNT);

            if (strcmp(name, "com.google.android.gms.unstable") == 0) {

                int fd = api->connectCompanion();

                read(fd, &dexSize, sizeof(long));

                if (dexSize > 0) {

                    dexBuffer = new unsigned char[dexSize];
                    read(fd, dexBuffer, dexSize);

                } else {

                    LOGD("Couldn't load classes.dex file in memory!");
                    api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
                    goto end;
                }

                read(fd, &jsonSize, sizeof(long));

                if (jsonSize > 0) {

                    jsonBuffer = new char[jsonSize];
                    read(fd, jsonBuffer, jsonSize);

                } else {

                    LOGD("Couldn't load pif.json file in memory!");
                    delete[] dexBuffer;
                    api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
                    goto end;
                }

                end:
                close(fd);
                goto clear;
            }
        }

        api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);

        clear:
        env->ReleaseStringUTFChars(args->nice_name, name);
    }

    void postAppSpecialize(const zygisk::AppSpecializeArgs *args) override {
        if (dexSize < 1 || jsonSize < 1 || dexBuffer == nullptr || jsonBuffer == nullptr) return;

        std::string_view jsonStr(jsonBuffer, jsonSize);
        nlohmann::json json = nlohmann::json::parse(jsonStr, nullptr, false, true);

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

        } else {

            LOGD("JSON file doesn't contain BUILD_ID key :(");
        }

        doHook();

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

        auto entryClass = (jclass) entryClassObj;

        LOGD("call init");
        auto entryInit = env->GetStaticMethodID(entryClass, "init", "(Ljava/lang/String;)V");
        auto str = env->NewStringUTF(json.dump().c_str());
        env->CallStaticVoidMethod(entryClass, entryInit, str);

        env->DeleteLocalRef(clClass);
        env->DeleteLocalRef(dexClClass);
        env->DeleteLocalRef(buffer);
        env->DeleteLocalRef(dexCl);
        env->DeleteLocalRef(entryClassName);
        env->DeleteLocalRef(entryClassObj);
        env->DeleteLocalRef(str);

        delete[] dexBuffer;
        delete[] jsonBuffer;
    }

    void preServerSpecialize(zygisk::ServerSpecializeArgs *args) override {
        api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
    }

private:
    zygisk::Api *api = nullptr;
    JNIEnv *env = nullptr;
    long dexSize = 0, jsonSize = 0;
    unsigned char *dexBuffer = nullptr;
    char *jsonBuffer = nullptr;
};

static void companion(int fd) {
    long dexSize = 0, jsonSize = 0;
    unsigned char *dexBuffer = nullptr;
    char *jsonBuffer = nullptr;

    FILE *dexFile = fopen(CLASSES_DEX, "rb");

    if (dexFile) {

        fseek(dexFile, 0, SEEK_END);
        dexSize = ftell(dexFile);
        fseek(dexFile, 0, SEEK_SET);

        dexBuffer = new unsigned char[dexSize];
        fread(dexBuffer, 1, dexSize, dexFile);

        fclose(dexFile);
    }

    write(fd, &dexSize, sizeof(long));
    write(fd, dexBuffer, dexSize);

    delete[] dexBuffer;

    FILE *jsonFile = fopen(PIF_JSON, "r");

    if (jsonFile) {

        fseek(jsonFile, 0, SEEK_END);
        jsonSize = ftell(jsonFile);
        fseek(jsonFile, 0, SEEK_SET);

        jsonBuffer = new char[jsonSize];
        fread(jsonBuffer, 1, jsonSize, jsonFile);

        fclose(jsonFile);
    }

    write(fd, &jsonSize, sizeof(long));
    write(fd, jsonBuffer, jsonSize);

    delete[] jsonBuffer;
}

REGISTER_ZYGISK_MODULE(PlayIntegrityFix)

REGISTER_ZYGISK_COMPANION(companion)
