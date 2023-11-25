#include <android/log.h>
#include <sys/system_properties.h>
#include <unistd.h>
#include <fstream>

#include "zygisk.hpp"
#include "dobby.h"
#include "json.hpp"

#define DEX_FILE_PATH "/data/adb/modules/playintegrityfix/classes.dex"

#define PROP_FILE_PATH "/data/adb/modules/playintegrityfix/pif.json"

#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, "PIF/Native", __VA_ARGS__)

static std::string FIRST_API_LEVEL, SECURITY_PATCH;

typedef void (*T_Callback)(void *, const char *, const char *, uint32_t);

static std::map<void *, T_Callback> callbacks;

static void modify_callback(void *cookie, const char *name, const char *value, uint32_t serial) {

    if (cookie == nullptr || name == nullptr || value == nullptr ||
        !callbacks.contains(cookie))
        return;

    std::string_view prop(name);

    if (prop.ends_with("api_level")) {
        if (FIRST_API_LEVEL == "NULL") {
            value = nullptr;
        } else {
            value = FIRST_API_LEVEL.c_str();
        }
        LOGD("[%s] -> %s", name, value);
    } else if (prop.ends_with("security_patch")) {
        if (SECURITY_PATCH == "NULL") {
            value = nullptr;
        } else {
            value = SECURITY_PATCH.c_str();
        }
        LOGD("[%s] -> %s", name, value);
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
    void *handle = DobbySymbolResolver("libc.so", "__system_property_read_callback");
    if (handle == nullptr) {
        LOGD("Couldn't find '__system_property_read_callback' handle. Report to @chiteroman");
        return;
    }
    LOGD("Found '__system_property_read_callback' handle at %p", handle);
    DobbyHook(handle, reinterpret_cast<void *>(my_system_property_read_callback),
              reinterpret_cast<void **>(&o_system_property_read_callback));
}

class PlayIntegrityFix : public zygisk::ModuleBase {
public:
    void onLoad(zygisk::Api *api, JNIEnv *env) override {
        this->api = api;
        this->env = env;
    }

    void preAppSpecialize(zygisk::AppSpecializeArgs *args) override {
        bool isGms = false;
        bool isGmsUnstable = false;

        auto rawProcess = env->GetStringUTFChars(args->nice_name, nullptr);
        if (rawProcess) {
            std::string_view process(rawProcess);
            isGms = process.starts_with("com.google.android.gms");
            isGmsUnstable = process.compare("com.google.android.gms.unstable") == 0;
        }
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

        ssize_t size;
        char buffer[10000];
        int fd = api->connectCompanion();

        size = read(fd, buffer, sizeof(buffer));

        if (size > 0) {
            moduleDex.insert(moduleDex.end(), buffer, buffer + size);
        } else {
            LOGD("Couldn't load classes.dex file in memory");
            close(fd);
            api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
            return;
        }

        lseek(fd, 0, SEEK_SET);

        size = read(fd, buffer, sizeof(buffer));

        if (size > 0) {
            jsonStr.insert(jsonStr.end(), buffer, buffer + size);
        } else {
            LOGD("Couldn't load pif.json file in memory");
        }

        close(fd);

        LOGD("Received 'classes.dex' file from socket: %d bytes",
             static_cast<int>(moduleDex.size()));

        LOGD("Received 'pif.json' file from socket: %d bytes", static_cast<int>(jsonStr.size()));
    }

    void postAppSpecialize(const zygisk::AppSpecializeArgs *args) override {
        if (moduleDex.empty()) return;

        readJson();

        inject();

        doHook();
    }

    void preServerSpecialize(zygisk::ServerSpecializeArgs *args) override {
        api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
    }

private:
    zygisk::Api *api = nullptr;
    JNIEnv *env = nullptr;
    std::vector<char> moduleDex;
    std::string jsonStr;

    void readJson() {
        nlohmann::json json = nlohmann::json::parse(jsonStr, nullptr, false);

        auto getStringFromJson = [&json](const std::string &key) {
            return json.contains(key) && !json[key].is_null() ? json[key].get<std::string>()
                                                              : "NULL";
        };

        SECURITY_PATCH = getStringFromJson("SECURITY_PATCH");
        FIRST_API_LEVEL = getStringFromJson("FIRST_API_LEVEL");

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
        auto buffer = env->NewDirectByteBuffer(moduleDex.data(),
                                               static_cast<jlong>(moduleDex.size()));
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
        auto javaStr = env->NewStringUTF(jsonStr.c_str());
        env->CallStaticVoidMethod(entryClass, readProps, javaStr);

        LOGD("call init");
        auto entryInit = env->GetStaticMethodID(entryClass, "init", "()V");
        env->CallStaticVoidMethod(entryClass, entryInit);

        moduleDex.clear();
        jsonStr.clear();
    }
};

static void companion(int fd) {
    std::ifstream dex(DEX_FILE_PATH, std::ios::binary);
    std::ifstream prop(PROP_FILE_PATH);

    std::vector<char> dexVector((std::istreambuf_iterator<char>(dex)),
                                std::istreambuf_iterator<char>());

    std::vector<char> propVector((std::istreambuf_iterator<char>(prop)),
                                 std::istreambuf_iterator<char>());

    write(fd, dexVector.data(), dexVector.size());
    lseek(fd, 0, SEEK_SET);
    write(fd, propVector.data(), propVector.size());
}

REGISTER_ZYGISK_MODULE(PlayIntegrityFix)

REGISTER_ZYGISK_COMPANION(companion)