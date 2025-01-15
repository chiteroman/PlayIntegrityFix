#include "json.hpp"
#include "log.h"
#include "dobby.h"
#include "jni.h"
#include <sys/system_properties.h>

static JNIEnv *env = nullptr;

static std::string gmsDir;

static bool spoofProps = true;
static bool spoofProvider = true;
static bool spoofSignature = false;

static nlohmann::json json;

static bool DEBUG = false;

static std::string DEVICE_INITIAL_SDK_INT, SECURITY_PATCH, BUILD_ID;

typedef void (*T_Callback)(void *, const char *, const char *, uint32_t);

static T_Callback o_callback = nullptr;

static void modify_callback(void *cookie, const char *name, const char *value,
                            uint32_t serial) {

    if (!cookie || !name || !value || !o_callback)
        return;

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

    if (strcmp(oldValue, value) == 0) {
        if (DEBUG)
            LOGD("[%s]: %s (unchanged)", name, oldValue);
    } else {
        LOGD("[%s]: %s -> %s", name, oldValue, value);
    }

    return o_callback(cookie, name, value, serial);
}

static void (*o_system_property_read_callback)(prop_info *, T_Callback,
                                               void *) = nullptr;

static void my_system_property_read_callback(prop_info *pi, T_Callback callback,
                                             void *cookie) {
    if (pi && callback && cookie)
        o_callback = callback;
    return o_system_property_read_callback(pi, modify_callback, cookie);
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

            env->SetStaticObjectField(buildClass, fieldID, jValue);
            if (env->ExceptionCheck()) {
                env->ExceptionClear();
                continue;
            }

            LOGD("Set '%s' to '%s'", fieldName, value);
        }
    }
}

void parseJSON() {
    if (json.empty()) return;

    if (json.contains("DEVICE_INITIAL_SDK_INT")) {
        if (json["DEVICE_INITIAL_SDK_INT"].is_string()) {
            DEVICE_INITIAL_SDK_INT = json["DEVICE_INITIAL_SDK_INT"].get<std::string>();
        } else if (json["DEVICE_INITIAL_SDK_INT"].is_number_integer()) {
            DEVICE_INITIAL_SDK_INT = std::to_string(json["DEVICE_INITIAL_SDK_INT"].get<int>());
        } else {
            LOGE("Couldn't parse DEVICE_INITIAL_SDK_INT value!");
        }
        json.erase("DEVICE_INITIAL_SDK_INT");
    }

    if (json.contains("spoofProvider") && json["spoofProvider"].is_boolean()) {
        spoofProvider = json["spoofProvider"].get<bool>();
        json.erase("spoofProvider");
    }

    if (json.contains("spoofProps") && json["spoofProps"].is_boolean()) {
        spoofProps = json["spoofProps"].get<bool>();
        json.erase("spoofProps");
    }

    if (json.contains("spoofSignature") && json["spoofSignature"].is_boolean()) {
        spoofSignature = json["spoofSignature"].get<bool>();
        json.erase("spoofSignature");
    }

    if (json.contains("DEBUG") && json["DEBUG"].is_boolean()) {
        DEBUG = json["DEBUG"].get<bool>();
        json.erase("DEBUG");
    }

    if (json.contains("FINGERPRINT") && json["FINGERPRINT"].is_string()) {
        std::string fingerprint = json["FINGERPRINT"].get<std::string>();

        std::vector<std::string> vector;
        auto parts = fingerprint | std::views::split('/');

        for (const auto &part: parts) {
            auto subParts = std::string(part.begin(), part.end()) | std::views::split(':');
            for (const auto &subPart: subParts) {
                vector.emplace_back(subPart.begin(), subPart.end());
            }
        }

        if (vector.size() == 8) {
            json["BRAND"] = vector[0];
            json["PRODUCT"] = vector[1];
            json["DEVICE"] = vector[2];
            json["RELEASE"] = vector[3];
            json["ID"] = vector[4];
            json["INCREMENTAL"] = vector[5];
            json["TYPE"] = vector[6];
            json["TAGS"] = vector[7];
        } else {
            LOGE("Error parsing fingerprint values!");
        }
    }

    if (json.contains("SECURITY_PATCH") && json["SECURITY_PATCH"].is_string()) {
        SECURITY_PATCH = json["SECURITY_PATCH"].get<std::string>();
    }

    if (json.contains("ID") && json["ID"].is_string()) {
        BUILD_ID = json["ID"].get<std::string>();
    }
}

void injectDex() {
    LOGD("get system classloader");
    auto clClass = env->FindClass("java/lang/ClassLoader");
    auto getSystemClassLoader = env->GetStaticMethodID(clClass, "getSystemClassLoader",
                                                       "()Ljava/lang/ClassLoader;");
    auto systemClassLoader = env->CallStaticObjectMethod(clClass, getSystemClassLoader);

    if (env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
        return;
    }

    LOGD("create class loader");
    auto dexClClass = env->FindClass("dalvik/system/PathClassLoader");
    auto dexClInit = env->GetMethodID(dexClClass, "<init>",
                                      "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/ClassLoader;)V");
    auto str1 = env->NewStringUTF((gmsDir + "/classes.dex").c_str());
    auto str2 = env->NewStringUTF(gmsDir.c_str());
    auto dexCl = env->NewObject(dexClClass, dexClInit, str1, str2, systemClassLoader);

    if (env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
        return;
    }

    LOGD("load class");
    auto loadClass = env->GetMethodID(clClass, "loadClass",
                                      "(Ljava/lang/String;)Ljava/lang/Class;");
    auto entryClassName = env->NewStringUTF("es.chiteroman.playintegrityfix.EntryPoint");
    auto entryClassObj = env->CallObjectMethod(dexCl, loadClass, entryClassName);
    auto entryPointClass = (jclass) entryClassObj;

    if (env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
        return;
    }

    LOGD("call init");
    auto entryInit = env->GetStaticMethodID(entryPointClass, "init", "(Ljava/lang/String;ZZ)V");
    auto jsonStr = env->NewStringUTF(json.dump().c_str());
    env->CallStaticVoidMethod(entryPointClass, entryInit, jsonStr, spoofProvider,
                              spoofSignature);

    if (env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
    }

    env->DeleteLocalRef(entryClassName);
    env->DeleteLocalRef(entryClassObj);
    env->DeleteLocalRef(jsonStr);
    env->DeleteLocalRef(dexCl);
    env->DeleteLocalRef(dexClClass);
    env->DeleteLocalRef(clClass);

    LOGD("jni memory free");
}

extern "C" {
[[gnu::visibility("default"), maybe_unused]]
void init(const char *dir, JavaVM *jvm, jint jni_version) {

    if (dir) {
        LOGD("[INJECT] GMS dir: %s", dir);
    } else {
        LOGE("[INJECT] dir is null!");
        return;
    }

    if (jvm) {
        auto result = jvm->GetEnv(reinterpret_cast<void **>(&env), jni_version);
        if (result != JNI_OK) {
            LOGE("[INJECT] GetEnv error: %d", result);
            return;
        }
    } else {
        LOGE("[INJECT] jvm is null!");
        return;
    }

    if (env) {
        LOGD("[INJECT] JNIEnv version: %x", env->GetVersion());
    } else {
        LOGE("[INJECT] env is null!");
        return;
    }

    gmsDir = dir;

    FILE *pif = fopen((gmsDir + "/pif.json").c_str(), "r");
    if (pif) {
        json = nlohmann::json::parse(pif, nullptr, false, true);
        fclose(pif);
    }

    if (json == nullptr || json.empty())
        return;

    bool trickyStore = false;
    FILE *trickyStoreFile = fopen((gmsDir + "/trickystore").c_str(), "r");
    if (trickyStoreFile) {
        trickyStore = true;
        fclose(trickyStoreFile);
    }

    LOGD("[INJECT] TrickyStore? %s", trickyStore ? "yes" : "no");

    bool testSignedRom = false;
    FILE *testSignedFile = fopen((gmsDir + "/testsign").c_str(), "r");
    if (testSignedFile) {
        testSignedRom = true;
        fclose(testSignedFile);
    }

    LOGD("[INJECT] ROM signed with test keys? %s", testSignedRom ? "yes" : "no");

    if (trickyStore) {
        spoofProvider = false;
        spoofProps = false;
    }

    if (testSignedRom) {
        spoofSignature = true;
    }

    parseJSON();

    UpdateBuildFields();

    if (spoofProvider || spoofSignature) {
        injectDex();
    } else {
        LOGD("Dex file won't be injected due spoofProvider and spoofSignature are false");
    }

    if (!spoofProps)
        return;

    auto ptr = DobbySymbolResolver(nullptr, "__system_property_read_callback");

    if (ptr && DobbyHook(ptr, (void *) my_system_property_read_callback,
                         (void **) &o_system_property_read_callback) == 0) {
        LOGD("hook __system_property_read_callback successful at %p", ptr);
        return;
    }

    LOGE("hook __system_property_read_callback failed!");
}
}