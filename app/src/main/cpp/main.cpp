#include <android/log.h>
#include <sys/system_properties.h>
#include <unistd.h>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>

#include "zygisk.hpp"
#include "dobby.h"
#include "json.hpp"

#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, "PIF/Native", __VA_ARGS__)

static std::string API_LEVEL;

static std::string SECURITY_PATCH;

#define DEX_FILE_PATH "/data/adb/modules/playintegrityfix/classes.dex"

#define JSON_FILE_PATH "/data/adb/modules/playintegrityfix/pif.json"

typedef void (*T_Callback)(void *, const char *, const char *, uint32_t);

static volatile T_Callback propCallback = nullptr;

static void modify_callback(void *cookie, const char *name, const char *value, uint32_t serial) {

    if (cookie == nullptr || name == nullptr || value == nullptr || propCallback == nullptr) return;

    std::string_view prop(name);

    if (prop.ends_with("api_level")) value = API_LEVEL.c_str();
    else if (prop.ends_with("security_patch")) value = SECURITY_PATCH.c_str();

    if (!prop.starts_with("cache") && !prop.starts_with("debug")) LOGD("[%s] -> %s", name, value);

    return propCallback(cookie, name, value, serial);
}

static void (*o_system_property_read_callback)(const prop_info *, T_Callback, void *);

static void
my_system_property_read_callback(const prop_info *pi, T_Callback callback, void *cookie) {

    if (pi == nullptr || callback == nullptr || cookie == nullptr) {
        return o_system_property_read_callback(pi, callback, cookie);
    }
    propCallback = callback;
    return o_system_property_read_callback(pi, modify_callback, cookie);
}

static void doHook() {
    std::ifstream ifs("/data/data/com.google.android.gms/pif.json");

    auto json = nlohmann::json::parse(ifs, nullptr, false);

    ifs.close();

    LOGD("Loaded %d keys from pif.json", static_cast<int>(json.size()));

    API_LEVEL = json["API_LEVEL"].get<std::string>();
    SECURITY_PATCH = json["SECURITY_PATCH"].get<std::string>();

    json.clear();

    void *handle = DobbySymbolResolver("libc.so", "__system_property_read_callback");
    if (handle == nullptr) {
        LOGD("Couldn't find '__system_property_read_callback' handle. Report to @chiteroman");
        return;
    }
    LOGD("Found '__system_property_read_callback' handle at %p", handle);
    DobbyHook(handle, (void *) my_system_property_read_callback,
              (void **) &o_system_property_read_callback);
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
        isGmsUnstable = process.compare("com.google.android.gms.unstable") == 0;

        env->ReleaseStringUTFChars(args->nice_name, rawProcess);

        if (isGms) api->setOption(zygisk::FORCE_DENYLIST_UNMOUNT);

        if (isGmsUnstable) {

            int fd = api->connectCompanion();

            auto rawDir = env->GetStringUTFChars(args->app_data_dir, nullptr);
            std::string dir(rawDir);
            env->ReleaseStringUTFChars(args->app_data_dir, rawDir);

            int strSize = static_cast<int>(dir.size());

            write(fd, &strSize, sizeof(strSize));
            write(fd, dir.data(), strSize);

            dir.clear();
            dir.shrink_to_fit();

            long size;
            read(fd, &size, sizeof(size));

            char buffer[size];
            read(fd, buffer, size);

            close(fd);

            moduleDex.insert(moduleDex.end(), buffer, buffer + size);

            return;
        }

        api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
    }

    void postAppSpecialize(const zygisk::AppSpecializeArgs *args) override {
        if (!isGmsUnstable) return;

        doHook();

        if (!moduleDex.empty()) injectDex();
    }

    void preServerSpecialize(zygisk::ServerSpecializeArgs *args) override {
        api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
    }

private:
    zygisk::Api *api = nullptr;
    JNIEnv *env = nullptr;
    bool isGmsUnstable = false;
    std::vector<char> moduleDex;

    void injectDex() {
        LOGD("Preparing to inject %d bytes to the process", static_cast<int>(moduleDex.size()));

        LOGD("get system classloader");
        auto clClass = env->FindClass("java/lang/ClassLoader");
        auto getSystemClassLoader = env->GetStaticMethodID(clClass, "getSystemClassLoader",
                                                           "()Ljava/lang/ClassLoader;");
        auto systemClassLoader = env->CallStaticObjectMethod(clClass, getSystemClassLoader);

        LOGD("create buffer");
        auto buf = env->NewDirectByteBuffer(moduleDex.data(), static_cast<jlong>(moduleDex.size()));
        LOGD("create class loader");
        auto dexClClass = env->FindClass("dalvik/system/InMemoryDexClassLoader");
        auto dexClInit = env->GetMethodID(dexClClass, "<init>",
                                          "(Ljava/nio/ByteBuffer;Ljava/lang/ClassLoader;)V");
        auto dexCl = env->NewObject(dexClClass, dexClInit, buf, systemClassLoader);

        LOGD("load class");
        auto loadClass = env->GetMethodID(clClass, "loadClass",
                                          "(Ljava/lang/String;)Ljava/lang/Class;");
        auto entryClassName = env->NewStringUTF("es.chiteroman.playintegrityfix.EntryPoint");
        auto entryClassObj = env->CallObjectMethod(dexCl, loadClass, entryClassName);

        LOGD("call init");
        auto entryClass = (jclass) entryClassObj;
        auto entryInit = env->GetStaticMethodID(entryClass, "init", "()V");
        env->CallStaticVoidMethod(entryClass, entryInit);

        LOGD("clean");
        moduleDex.clear();
        moduleDex.shrink_to_fit();
    }
};

static void companion(int fd) {
    int strSize;
    read(fd, &strSize, sizeof(strSize));

    std::string file;

    file.resize(strSize);
    read(fd, file.data(), strSize);

    file = file + "/pif.json";

    LOGD("Json path: %s", file.c_str());

    std::filesystem::copy_file(JSON_FILE_PATH, file,
                               std::filesystem::copy_options::overwrite_existing);
    std::filesystem::permissions(file, std::filesystem::perms::all);

    std::ifstream dex(DEX_FILE_PATH, std::ifstream::binary | std::ifstream::ate);

    long size = dex.tellg();
    dex.seekg(std::ifstream::beg);

    char buffer[size];
    dex.read(buffer, size);

    dex.close();

    write(fd, &size, sizeof(size));
    write(fd, buffer, size);
}

REGISTER_ZYGISK_MODULE(PlayIntegrityFix)

REGISTER_ZYGISK_COMPANION(companion)