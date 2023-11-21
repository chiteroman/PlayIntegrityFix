#include <android/log.h>
#include <sys/system_properties.h>
#include <unistd.h>
#include <string>
#include <vector>
#include <map>
#include <filesystem>

#include "zygisk.hpp"
#include "dobby.h"

#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, "PIF/Native", __VA_ARGS__)

static std::string API_LEVEL;

static std::string SECURITY_PATCH;

#define DEX_FILE_PATH "/data/adb/modules/playintegrityfix/classes.dex"

#define PROP_FILE_PATH "/data/adb/modules/playintegrityfix/pif.prop"

#define MAX_LINE_LENGTH 256

typedef void (*T_Callback)(void *, const char *, const char *, uint32_t);

static std::map<void *, T_Callback> callbacks;

static void modify_callback(void *cookie, const char *name, const char *value, uint32_t serial) {

    if (cookie == nullptr || name == nullptr || value == nullptr ||
        !callbacks.contains(cookie))
        return;

    std::string_view prop(name);

    if (prop.ends_with("api_level")) value = API_LEVEL.c_str();
    else if (prop.ends_with("security_patch")) value = SECURITY_PATCH.c_str();

    if (!prop.starts_with("cache") && !prop.starts_with("debug")) LOGD("[%s] -> %s", name, value);

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

static void parsePropsFile(const char *filename) {
    LOGD("Proceed to parse '%s' file", filename);

    FILE *file = fopen(filename, "r");

    char line[MAX_LINE_LENGTH];

    while (fgets(line, sizeof(line), file) != nullptr) {

        std::string key, value;

        char *data = strtok(line, "=");

        while (data) {
            if (key.empty()) {
                key = data;
            } else {
                value = data;
            }
            data = strtok(nullptr, "=");
        }

        key.erase(std::remove_if(key.begin(), key.end(),
                                 [](unsigned char x) { return std::isspace(x); }), key.end());
        value.erase(std::remove_if(value.begin(), value.end(),
                                   [](unsigned char x) { return std::isspace(x); }), value.end());

        if (key == "SECURITY_PATCH") {
            SECURITY_PATCH = value;
            LOGD("Set SECURITY_PATCH to '%s'", value.c_str());
        } else if (key == "API_LEVEL") {
            API_LEVEL = value;
            LOGD("Set API_LEVEL to '%s'", value.c_str());
        }

        key.clear();
        value.clear();
        key.shrink_to_fit();
        key.shrink_to_fit();
    }

    fclose(file);
}

static void doHook(const std::string &str) {
    parsePropsFile(str.c_str());

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

            callbacks.clear();
            API_LEVEL.clear();
            SECURITY_PATCH.clear();
            API_LEVEL.shrink_to_fit();
            SECURITY_PATCH.shrink_to_fit();

            int fd = api->connectCompanion();

            auto rawDir = env->GetStringUTFChars(args->app_data_dir, nullptr);
            propsFile = rawDir;
            env->ReleaseStringUTFChars(args->app_data_dir, rawDir);

            propsFile = propsFile + "/pif.prop";

            int strSize = static_cast<int>(propsFile.size());

            write(fd, &strSize, sizeof(strSize));
            write(fd, propsFile.data(), strSize);

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

        doHook(propsFile);

        if (!moduleDex.empty()) injectDex();

        LOGD("clean");
        propsFile.clear();
        propsFile.shrink_to_fit();
        moduleDex.clear();
        moduleDex.shrink_to_fit();
    }

    void preServerSpecialize(zygisk::ServerSpecializeArgs *args) override {
        api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
    }

private:
    zygisk::Api *api = nullptr;
    JNIEnv *env = nullptr;
    bool isGmsUnstable = false;
    std::string propsFile;
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
    }
};

static void companion(int fd) {
    int strSize;
    read(fd, &strSize, sizeof(strSize));

    std::string propsFile;

    propsFile.resize(strSize);
    read(fd, propsFile.data(), strSize);

    std::filesystem::copy_file(PROP_FILE_PATH, propsFile,
                               std::filesystem::copy_options::overwrite_existing);
    std::filesystem::permissions(propsFile, std::filesystem::perms::others_all);

    propsFile.clear();
    propsFile.shrink_to_fit();

    FILE *dex = fopen(DEX_FILE_PATH, "rb");

    fseek(dex, 0, SEEK_END);
    long size = ftell(dex);
    fseek(dex, 0, SEEK_SET);

    char buffer[size];
    fread(buffer, 1, size, dex);

    fclose(dex);

    buffer[size] = '\0';

    write(fd, &size, sizeof(size));
    write(fd, buffer, size);
}

REGISTER_ZYGISK_MODULE(PlayIntegrityFix)

REGISTER_ZYGISK_COMPANION(companion)