#include <android/log.h>
#include <sys/system_properties.h>
#include <unistd.h>
#include "zygisk.hpp"
#include "dobby.h"
#include "json.hpp"
#include <filesystem>

#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, "PIF", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "PIF", __VA_ARGS__)

#define DEX_PATH "/data/adb/modules/playintegrityfix/classes.dex"
#define PIF_JSON "/data/adb/pif.json"
#define PIF_JSON_DEFAULT "/data/adb/modules/playintegrityfix/pif.json"
#define TS_PATH "/data/adb/modules/tricky_store"

class FileWrapper {
public:
    FileWrapper(const char* path, const char* mode) : file_(fopen(path, mode)) {}
    ~FileWrapper() { if (file_) fclose(file_); }
    FILE* get() { return file_; }

private:
    FILE* file_;
};

static ssize_t xread(int fd, void *buffer, size_t count) {
    ssize_t total = 0;
    char *buf = (char *) buffer;
    while (count > 0) {
        ssize_t ret = TEMP_FAILURE_RETRY(read(fd, buf, count));
        if (ret < 0) return -1;
        buf += ret;
        total += ret;
        count -= ret;
    }
    return total;
}

static ssize_t xwrite(int fd, const void *buffer, size_t count) {
    ssize_t total = 0;
    char *buf = (char *) buffer;
    while (count > 0) {
        ssize_t ret = TEMP_FAILURE_RETRY(write(fd, buf, count));
        if (ret < 0) return -1;
        buf += ret;
        total += ret;
        count -= ret;
    }
    return total;
}

static bool DEBUG = false;
static std::string DEVICE_INITIAL_SDK_INT, SECURITY_PATCH, BUILD_ID;

typedef void (*T_Callback)(void *, const char *, const char *, uint32_t);

static T_Callback o_callback = nullptr;

static void modify_callback(void *cookie, const char *name, const char *value, uint32_t serial) {
    if (!cookie || !name || !value || !o_callback) return;

    const char *oldValue = value;
    std::string_view prop(name);

    if (prop == "init.svc.adbd") {
        value = "stopped";
    } else if (prop == "sys.usb.state") {
        value = "mtp";
    } else if (prop.ends_with("api_level")) {
        if (!DEVICE_INITIAL_SDK_INT.empty()) {
            value = DEVICE_INITIAL_SDK_INT.c_str();
        }
    } else if (prop.ends_with(".security_patch")) {
        if (!SECURITY_PATCH.empty()) {
            value = SECURITY_PATCH.c_str();
        }
    } else if (prop.ends_with(".build.id")) {
        if (!BUILD_ID.empty()) {
            value = BUILD_ID.c_str();
        }
    }

    if (strcmp(oldValue, value) != 0) {
       LOGD("[%s]: %s -> %s", name, oldValue, value);
    }

    return o_callback(cookie, name, value, serial);
}

static void (*o_system_property_read_callback)(prop_info *, T_Callback, void *) = nullptr;

static void my_system_property_read_callback(prop_info *pi, T_Callback callback, void *cookie) {
    if (pi && callback && cookie) o_callback = callback;
    return o_system_property_read_callback(pi, modify_callback, cookie);
}

static bool doHook() {
    void *ptr = DobbySymbolResolver(nullptr, "__system_property_read_callback");
    if (ptr && DobbyHook(ptr, (void *) my_system_property_read_callback,
                         (void **) &o_system_property_read_callback) == 0) {
        return true;
    }
    return false;
}

class PlayIntegrityFix : public zygisk::ModuleBase {
public:
    void onLoad(zygisk::Api *api, JNIEnv *env) override {
        this->api = api;
        this->env = env;
    }

    void preAppSpecialize(zygisk::AppSpecializeArgs *args) override {
        if (!args || !args->app_data_dir) {
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

        if (!args->nice_name) {
            api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
            return;
        }

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
        std::string jsonStr;
        xread(fd, &dexSize, sizeof(dexSize));
        xread(fd, &jsonSize, sizeof(jsonSize));

        if (dexSize > 0) {
            dexVector.resize(dexSize);
            xread(fd, dexVector.data(), dexSize * sizeof(uint8_t));
        }

        if (jsonSize > 0) {
            jsonStr.resize(jsonSize);
            xread(fd, jsonStr.data(), jsonSize * sizeof(uint8_t));
            json = nlohmann::json::parse(jsonStr, nullptr, false, true);
        }

        bool trickyStore = false;
        xread(fd, &trickyStore, sizeof(trickyStore));

        bool testSignedRom = false;
        xread(fd, &testSignedRom, sizeof(testSignedRom));
        close(fd);

        parseJSON();

        if (trickyStore) {
            spoofProvider = false;
            spoofProps = false;
        }

        if (testSignedRom) {
            spoofSignature = true;
        }
    }

    void postAppSpecialize(const zygisk::AppSpecializeArgs *args) override {
        if (dexVector.empty()) return;

        UpdateBuildFields();

        if (spoofProvider || spoofSignature) {
            injectDex();
        } else {
            LOGD("Dex file won't be injected due spoofProvider and spoofSignature are false");
        }

        if (spoofProps) {
            if (!doHook()) {
                dlclose();
            }
        } else {
            dlclose();
        }

        json.clear();
        dexVector.clear();
        dexVector.shrink_to_fit();
    }

    void preServerSpecialize(zygisk::ServerSpecializeArgs *args) override {
        api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
    }

private:
    zygisk::Api *api = nullptr;
    JNIEnv *env = nullptr;
    std::vector<uint8_t> dexVector;
    nlohmann::json json;
    bool spoofProps = true;
    bool spoofProvider = true;
    bool spoofSignature = false;

    void dlclose() {
        api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
    }

    void parseJSON() {
        if (json.empty()) return;

        if (auto it = json.find("DEVICE_INITIAL_SDK_INT"); it != json.end()) {
            if (it->is_string()) {
                DEVICE_INITIAL_SDK_INT = it->get<std::string>();
            } else if (it->is_number_integer()) {
                DEVICE_INITIAL_SDK_INT = std::to_string(it->get<int>());
            }
            json.erase(it);
        }

        if (auto it = json.find("spoofProvider"); it != json.end() && it->is_boolean()) {
            spoofProvider = it->get<bool>();
            json.erase(it);
        }

        if (auto it = json.find("spoofProps"); it != json.end() && it->is_boolean()) {
            spoofProps = it->get<bool>();
            json.erase(it);
        }

        if (auto it = json.find("spoofSignature"); it != json.end() && it->is_boolean()) {
            spoofSignature = it->get<bool>();
            json.erase(it);
        }
        
        if (auto it = json.find("DEBUG"); it != json.end() && it->is_boolean()) {
            DEBUG = it->get<bool>();
            json.erase(it);
        }

        if (auto it = json.find("FINGERPRINT"); it != json.end() && it->is_string()) {
            std::string fingerprint = it->get<std::string>();
            std::vector<std::string> vector;
            size_t start = 0;
            size_t end = fingerprint.find('/');
            while (end != std::string::npos) {
                size_t subStart = 0;
                size_t subEnd = fingerprint.find(':', start);
                while (subEnd != std::string::npos && subEnd < end) {
                    vector.emplace_back(fingerprint.substr(subStart, subEnd - subStart));
                    subStart = subEnd + 1;
                    subEnd = fingerprint.find(':', subStart);
                }
                vector.emplace_back(fingerprint.substr(subStart, end - subStart));
                start = end + 1;
                end = fingerprint.find('/', start);
            }
             size_t subStart = 0;
            size_t subEnd = fingerprint.find(':', start);
            while (subEnd != std::string::npos) {
                vector.emplace_back(fingerprint.substr(subStart, subEnd - subStart));
                subStart = subEnd + 1;
                subEnd = fingerprint.find(':', subStart);
            }
            vector.emplace_back(fingerprint.substr(subStart));

            if (vector.size() == 8) {
                json["BRAND"] = vector[0];
                json["PRODUCT"] = vector[1];
                json["DEVICE"] = vector[2];
                json["RELEASE"] = vector[3];
                json["ID"] = vector[4];
                json["INCREMENTAL"] = vector[5];
                json["TYPE"] = vector[6];
                json["TAGS"] = vector[7];
            }
        }

        if (auto it = json.find("SECURITY_PATCH"); it != json.end() && it->is_string()) {
            SECURITY_PATCH = it->get<std::string>();
        }

        if (auto it = json.find("ID"); it != json.end() && it->is_string()) {
            BUILD_ID = it->get<std::string>();
        }
    }

    void injectDex() {
        auto clClass = env->FindClass("java/lang/ClassLoader");
        auto getSystemClassLoader = env->GetStaticMethodID(clClass, "getSystemClassLoader", "()Ljava/lang/ClassLoader;");
        auto systemClassLoader = env->CallStaticObjectMethod(clClass, getSystemClassLoader);
        if (env->ExceptionCheck()) {
            env->ExceptionDescribe();
            env->ExceptionClear();
            return;
        }

        auto dexClClass = env->FindClass("dalvik/system/InMemoryDexClassLoader");
        auto dexClInit = env->GetMethodID(dexClClass, "<init>", "(Ljava/nio/ByteBuffer;Ljava/lang/ClassLoader;)V");
        auto buffer = env->NewDirectByteBuffer(dexVector.data(), static_cast<jlong>(dexVector.size()));
        auto dexCl = env->NewObject(dexClClass, dexClInit, buffer, systemClassLoader);
        if (env->ExceptionCheck()) {
            env->ExceptionDescribe();
            env->ExceptionClear();
            return;
        }

        auto loadClass = env->GetMethodID(clClass, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
        auto entryClassName = env->NewStringUTF("es.chiteroman.playintegrityfix.EntryPoint");
        auto entryClassObj = env->CallObjectMethod(dexCl, loadClass, entryClassName);
        auto entryPointClass = (jclass) entryClassObj;
        if (env->ExceptionCheck()) {
            env->ExceptionDescribe();
            env->ExceptionClear();
            return;
        }

        auto entryInit = env->GetStaticMethodID(entryPointClass, "init", "(Ljava/lang/String;ZZ)V");
        auto jsonStr = env->NewStringUTF(json.dump().c_str());
        env->CallStaticVoidMethod(entryPointClass, entryInit, jsonStr, spoofProvider, spoofSignature);
        if (env->ExceptionCheck()) {
            env->ExceptionDescribe();
            env->ExceptionClear();
        }

        env->DeleteLocalRef(entryClassName);
        env->DeleteLocalRef(entryClassObj);
        env->DeleteLocalRef(jsonStr);
        env->DeleteLocalRef(dexCl);
        env->DeleteLocalRef(buffer);
        env->DeleteLocalRef(dexClClass);
        env->DeleteLocalRef(clClass);
    }

    void UpdateBuildFields() {
        jclass buildClass = env->FindClass("android/os/Build");
        jclass versionClass = env->FindClass("android/os/Build$VERSION");

        for (auto &[key, val]: json.items()) {
            if (!val.is_string()) continue;

            const char *fieldName = key.c_str();
            jfieldID fieldID = env->GetStaticFieldID(buildClass, fieldName, "Ljava/lang/String;");
            if (env->ExceptionCheck()) {
                env->ExceptionClear();
                fieldID = env->GetStaticFieldID(versionClass, fieldName, "Ljava/lang/String;");
                if (env->ExceptionCheck()) {
                    env->ExceptionClear();
                    continue;
                }
            }

            if (fieldID != nullptr) {
                std::string str = val.get<std::string>();
                const char *value = str.c_str();
                jstring jValue = env->NewStringUTF(value);
                if (env->ExceptionCheck()) {
                    env->ExceptionDescribe();
                    env->ExceptionClear();
                    continue;
                }

                env->SetStaticObjectField(buildClass, fieldID, jValue);
                if (env->ExceptionCheck()) {
                    env->ExceptionDescribe();
                    env->ExceptionClear();
                    continue;
                }

                env->DeleteLocalRef(jValue);
            }
        }
        env->DeleteLocalRef(buildClass);
        env->DeleteLocalRef(versionClass);
    }
};

static std::vector<uint8_t> readFile(const char *path) {
    FileWrapper file(path, "rb");
    if (!file.get()) return {};
    int size = static_cast<int>(std::filesystem::file_size(path));
    std::vector<uint8_t> vector(size);
    fread(vector.data(), 1, size, file.get());
    return vector;
}

static bool checkOtaZip() {
    std::array<char, 256> buffer{};
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen("unzip -l /system/etc/security/otacerts.zip", "r"), pclose);
    if (!pipe) return false;
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
        if (result.find("test") != std::string::npos) {
            return true;
        }
    }
    return false;
}

static void companion(int fd) {

    std::vector<uint8_t> dex, json;

    dex = readFile(DEX_PATH);

    json = readFile(PIF_JSON);
    if (json.empty()) json = readFile(PIF_JSON_DEFAULT);

    int dexSize = static_cast<int>(dex.size());
    int jsonSize = static_cast<int>(json.size());

    xwrite(fd, &dexSize, sizeof(dexSize));
    xwrite(fd, &jsonSize, sizeof(jsonSize));

    if (dexSize > 0) {
        xwrite(fd, dex.data(), dexSize * sizeof(uint8_t));
    }

    if (jsonSize > 0) {
        xwrite(fd, json.data(), jsonSize * sizeof(uint8_t));
    }

    bool trickyStore = std::filesystem::exists(TS_PATH);
    xwrite(fd, &trickyStore, sizeof(trickyStore));

    bool testSignedRom = checkOtaZip();
    xwrite(fd, &testSignedRom, sizeof(testSignedRom));
}

REGISTER_ZYGISK_MODULE(PlayIntegrityFix)

REGISTER_ZYGISK_COMPANION(companion)
