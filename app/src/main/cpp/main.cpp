#include <android/log.h>
#include <unistd.h>
#include <sys/stat.h>
#include "zygisk.hpp"
#include "dobby.h"
#include "json.hpp"

#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, "PIF/Native", __VA_ARGS__)

#define CLASSES_DEX "/data/adb/modules/playintegrityfix/classes.dex"
#define PIF_JSON "/data/adb/pif.json"

enum class PIF_Request {
    GET_GMS_UID,
    GET_CLASSES_DEX,
    END
};

std::string FIRST_API_LEVEL, SECURITY_PATCH, BUILD_ID;

using T_Callback = void (*)(void *, const char *, const char *, uint32_t);
T_Callback o_callback = nullptr;

void modify_callback(void *cookie, const char *name, const char *value, uint32_t serial) {
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

    o_callback(cookie, name, value, serial);
}

void (*o_system_property_read_callback)(const void *, T_Callback, void *);

void my_system_property_read_callback(const void *pi, T_Callback callback, void *cookie) {
    if (pi == nullptr || callback == nullptr || cookie == nullptr) {
        o_system_property_read_callback(pi, callback, cookie);
        return;
    }
    o_callback = callback;
    o_system_property_read_callback(pi, modify_callback, cookie);
}

void doHook() {
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
        int gms_uid = -1, fd = AskCompanion(PIF_Request::GET_GMS_UID);
        read(fd, &gms_uid, sizeof(gms_uid));
        close(fd);

        if ((args->uid % 100000) == gms_uid) {
            auto name = env->GetStringUTFChars(args->nice_name, nullptr);
            bool should_load_dex = memcmp(name, "com.google.android.gms.unstable", 31) == 0;
            env->ReleaseStringUTFChars(args->nice_name, name);

            api->setOption(zygisk::FORCE_DENYLIST_UNMOUNT);

            if (!should_load_dex) goto unload;

            long dexSize = 0, jsonSize = 0;

            fd = AskCompanion(PIF_Request::GET_CLASSES_DEX);

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

        unload:
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

    int AskCompanion(PIF_Request req) {
        int fd = api->connectCompanion();
        if (fd < 0) return -1;
        auto reqInt = static_cast<int>(req);
        write(fd, &reqInt, sizeof(reqInt));
        return fd;
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

void request_dex(int fd) {
    long dexSize = 0, jsonSize = 0;
    std::vector<char> dexVector, jsonVector;

    FILE *dexFile = fopen(CLASSES_DEX, "rb");

    if (dexFile) {
        fseek(dexFile, 0, SEEK_END);
        dexSize = ftell(dexFile);
        fseek(dexFile, 0, SEEK_SET);

        dexVector.resize(dexSize);
        fread(dexVector.data(), 1, dexSize, dexFile);

        fclose(dexFile);
    }

    FILE *jsonFile = fopen(PIF_JSON, "r");

    if (jsonFile) {
        fseek(jsonFile, 0, SEEK_END);
        jsonSize = ftell(jsonFile);
        fseek(jsonFile, 0, SEEK_SET);

        jsonVector.resize(jsonSize);
        fread(jsonVector.data(), 1, jsonSize, jsonFile);

        fclose(jsonFile);
    }

    write(fd, &dexSize, sizeof(long));
    write(fd, &jsonSize, sizeof(long));

    write(fd, dexVector.data(), dexSize);
    write(fd, jsonVector.data(), jsonSize);
}

void get_gms_uid(int fd) {
    struct stat st{};
    int uid = -1;
    if (!stat("/data/data/com.google.android.gms", &st))
        uid = st.st_uid;
    write(fd, &uid, sizeof(uid));
}

void companion(int fd) {
    int req = -1;
    read(fd, &req, sizeof(req));
    switch (static_cast<PIF_Request>(req)) {
        case PIF_Request::GET_GMS_UID:
            return get_gms_uid(fd);
        case PIF_Request::GET_CLASSES_DEX:
            return request_dex(fd);
        default:
            break;
    }
}

REGISTER_ZYGISK_MODULE(PlayIntegrityFix)
REGISTER_ZYGISK_COMPANION(companion)
