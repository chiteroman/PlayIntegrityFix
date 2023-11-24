#include <android/log.h>
#include <sys/system_properties.h>
#include <unistd.h>
#include <string>
#include <vector>
#include <map>
#include <filesystem>

#include "zygisk.hpp"
#include "shadowhook.h"

#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, "PIF/Native", __VA_ARGS__)

#define DEX_FILE_PATH "/data/adb/modules/playintegrityfix/classes.dex"

#define PROP_FILE_PATH "/data/adb/modules/playintegrityfix/pif.prop"

#define DEFAULT_SECURITY_PATCH "2017-08-05"

#define DEFAULT_FIRST_API_LEVEL "23"

static std::string SECURITY_PATCH, FIRST_API_LEVEL;

typedef void (*T_Callback)(void *, const char *, const char *, uint32_t);

static T_Callback o_callback = nullptr;

static void modify_callback(void *cookie, const char *name, const char *value, uint32_t serial) {

    if (cookie == nullptr || name == nullptr || value == nullptr || o_callback == nullptr) return;

    std::string prop(name);

    if (prop.ends_with("api_level")) {
        if (FIRST_API_LEVEL.empty()) {
            value = nullptr;
        } else {
            value = FIRST_API_LEVEL.c_str();
        }
    } else if (prop.ends_with("security_patch")) {
        if (SECURITY_PATCH.empty()) {
            value = nullptr;
        } else {
            value = SECURITY_PATCH.c_str();
        }
    }

    if (!prop.starts_with("cache") && !prop.starts_with("debug")) LOGD("[%s] -> %s", name, value);

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
    shadowhook_init(SHADOWHOOK_MODE_UNIQUE, false);
    void *handle = shadowhook_hook_sym_name(
            "libc.so",
            "__system_property_read_callback",
            (void *) my_system_property_read_callback,
            (void **) &o_system_property_read_callback
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
        std::string process(rawProcess);
        env->ReleaseStringUTFChars(args->nice_name, rawProcess);

        if (process.starts_with("com.google.android.gms")) {

            api->setOption(zygisk::FORCE_DENYLIST_UNMOUNT);

            if (process == "com.google.android.gms.unstable") {

                isGmsUnstable = true;

                auto rawDir = env->GetStringUTFChars(args->app_data_dir, nullptr);
                dir = std::string(rawDir);
                env->ReleaseStringUTFChars(args->app_data_dir, rawDir);

                auto fd = api->connectCompanion();

                write(fd, dir.data(), dir.size());

                int temp;
                read(fd, &temp, sizeof(temp));

                close(fd);

                return;
            }
        }

        api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
    }

    void postAppSpecialize(const zygisk::AppSpecializeArgs *args) override {
        if (!isGmsUnstable) return;

        readDexFile();

        readPropsFile();

        doHook();

        inject();

        clean();
    }

    void preServerSpecialize(zygisk::ServerSpecializeArgs *args) override {
        api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
    }

private:
    zygisk::Api *api = nullptr;
    JNIEnv *env = nullptr;
    bool isGmsUnstable = false;
    std::string dir;
    std::vector<char> moduleDex;
    std::map<std::string, std::string> props;

    void clean() {
        dir.clear();
        moduleDex.clear();
        props.clear();
    }

    void readPropsFile() {
        std::string f = dir + "/pif.prop";

        FILE *file = fopen(f.c_str(), "r");

        if (file == nullptr) {
            SECURITY_PATCH = DEFAULT_SECURITY_PATCH;
            FIRST_API_LEVEL = DEFAULT_FIRST_API_LEVEL;
            LOGD("File pif.prop doesn't exist, using default values");
            return;
        }

        char buffer[256];
        while (fgets(buffer, sizeof(buffer), file)) {

            char *rawKey = strtok(buffer, "=");
            char *rawValue = strtok(nullptr, "=");

            if (rawKey == nullptr) continue;

            std::string key(rawKey);
            std::string value(rawValue);

            key.erase(std::remove_if(key.begin(), key.end(), [](unsigned char x) {
                return std::isspace(x);
            }), key.end());

            value.erase(std::remove(value.begin(), value.end(), '\n'), value.end());

            props[key] = value;
        }

        fclose(file);

        std::filesystem::remove(f);

        LOGD("Read %d props from file!", static_cast<int>(props.size()));

        SECURITY_PATCH = props["SECURITY_PATCH"];
        FIRST_API_LEVEL = props["FIRST_API_LEVEL"];
    }

    void readDexFile() {
        std::string f = dir + "/classes.dex";

        FILE *file = fopen(f.c_str(), "rb");

        if (file == nullptr) {
            LOGD("File classes.dex doesn't exist. This is weird... Report to @chiteroman");
            return;
        }

        fseek(file, 0, SEEK_END);
        long size = ftell(file);
        fseek(file, 0, SEEK_SET);

        char buffer[size];
        fread(buffer, 1, size, file);

        fclose(file);

        std::filesystem::remove(f);

        moduleDex.insert(moduleDex.end(), buffer, buffer + size);
    }

    void inject() {
        if (moduleDex.empty()) {
            LOGD("ERROR! Dex in memory is empty!");
            return;
        }

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

        auto entryClass = (jclass) entryClassObj;

        if (!props.empty()) {
            LOGD("call add prop");
            auto addProp = env->GetStaticMethodID(entryClass, "addProp",
                                                  "(Ljava/lang/String;Ljava/lang/String;)V");
            for (const auto &item: props) {
                auto key = env->NewStringUTF(item.first.c_str());
                auto value = env->NewStringUTF(item.second.c_str());
                env->CallStaticVoidMethod(entryClass, addProp, key, value);
            }
        }

        LOGD("call init");
        auto entryInit = env->GetStaticMethodID(entryClass, "init", "()V");
        env->CallStaticVoidMethod(entryClass, entryInit);
    }
};

static void companion(int fd) {
    char buffer[256];
    auto bytes = read(fd, buffer, sizeof(buffer));

    std::string file(buffer, bytes);
    LOGD("[ROOT] Read file: %s", file.c_str());

    std::string dex = file + "/classes.dex";
    std::string prop = file + "/pif.prop";

    if (std::filesystem::copy_file(DEX_FILE_PATH, dex,
                                   std::filesystem::copy_options::overwrite_existing)) {
        std::filesystem::permissions(dex, std::filesystem::perms::all);
    }

    if (std::filesystem::copy_file(PROP_FILE_PATH, prop,
                                   std::filesystem::copy_options::overwrite_existing)) {
        std::filesystem::permissions(prop, std::filesystem::perms::all);
    }

    int temp = 1;
    write(fd, &temp, sizeof(temp));
}

REGISTER_ZYGISK_MODULE(PlayIntegrityFix)

REGISTER_ZYGISK_COMPANION(companion)