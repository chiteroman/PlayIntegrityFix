#include "zygisk.hpp"
#include <android/log.h>
#include <string>
#include <dlfcn.h>
#include <unistd.h>
#include <cerrno>
#include <filesystem>
#include <sys/stat.h>

#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, "PIF", __VA_ARGS__)

#define TS_PATH "/data/adb/modules/tricky_store"

#define DEX_PATH "/data/adb/modules/playintegrityfix/classes.dex"

#define LIB_64 "/data/adb/modules/playintegrityfix/inject/arm64-v8a.so"
#define LIB_32 "/data/adb/modules/playintegrityfix/inject/armeabi-v7a.so"

#define DEFAULT_JSON "/data/adb/modules/playintegrityfix/pif.json"
#define CUSTOM_JSON_FORK "/data/adb/modules/playintegrityfix/custom.pif.json"
#define CUSTOM_JSON "/data/adb/pif.json"

static ssize_t xread(int fd, void *buffer, size_t count) {
    ssize_t total = 0;
    auto buf = static_cast<char *>(buffer);
    while (count > 0) {
        ssize_t ret = TEMP_FAILURE_RETRY(read(fd, buf, count));
        if (ret < 0)
            return -1;
        buf += ret;
        total += ret;
        count -= ret;
    }
    return total;
}

static ssize_t xwrite(int fd, const void *buffer, size_t count) {
    ssize_t total = 0;
    auto buf = (char *) buffer;
    while (count > 0) {
        ssize_t ret = TEMP_FAILURE_RETRY(write(fd, buf, count));
        if (ret < 0)
            return -1;
        buf += ret;
        total += ret;
        count -= ret;
    }
    return total;
}

static bool copyFile(const std::string &from, const std::string &to, mode_t perms = 0777) {
    return std::filesystem::exists(from) &&
           std::filesystem::copy_file(
                   from,
                   to,
                   std::filesystem::copy_options::overwrite_existing
           ) &&
           !chmod(
                   to.c_str(),
                   perms
           );
}

static bool checkOtaZip() {
    std::array<char, 256> buffer{};
    std::string result;
    bool found = false;

    std::unique_ptr<FILE, decltype(&pclose)> pipe(
            popen("unzip -l /system/etc/security/otacerts.zip", "r"), pclose);
    if (!pipe) return false;

    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
        if (result.find("testkey") != std::string::npos) {
            found = true;
            break;
        }
    }

    return found;
}

static void companion(int fd) {
    bool ok = true;

    int size = 0;
    xread(fd, &size, sizeof(int));

    std::string dir;
    dir.resize(size);
    auto bytes = xread(fd, dir.data(), size);
    dir.resize(bytes);
    dir.shrink_to_fit();

    LOGD("[COMPANION] GMS dir: %s", dir.c_str());

    auto libFile = dir + "/libinject.so";
#if defined(__aarch64__)
    ok &= copyFile(LIB_64, libFile);
#elif defined(__arm__)
    ok &= copyFile(LIB_32, libFile);
#endif

    LOGD("[COMPANION] copied inject lib");

    auto dexFile = dir + "/classes.dex";
    ok &= copyFile(DEX_PATH, dexFile, 0644);

    LOGD("[COMPANION] copied dex");

    auto jsonFile = dir + "/pif.json";
    if (!copyFile(CUSTOM_JSON, jsonFile)) {
        if (!copyFile(CUSTOM_JSON_FORK, jsonFile)) {
            if (!copyFile(DEFAULT_JSON, jsonFile)) {
                ok = false;
            }
        }
    }

    LOGD("[COMPANION] copied json");

    std::string ts(TS_PATH);
    bool trickyStore = std::filesystem::exists(ts) &&
                       !std::filesystem::exists(ts + "/disable") &&
                       !std::filesystem::exists(ts + "/remove");
    xwrite(fd, &trickyStore, sizeof(bool));

    bool testSignedRom = checkOtaZip();
    xwrite(fd, &testSignedRom, sizeof(bool));

    xwrite(fd, &ok, sizeof(bool));
}

using namespace zygisk;

class PlayIntegrityFix : public ModuleBase {
public:
    void onLoad(Api *api_, JNIEnv *env_) override {
        this->api = api_;
        this->env = env_;
    }

    void preAppSpecialize(AppSpecializeArgs *args) override {
        api->setOption(DLCLOSE_MODULE_LIBRARY);

        if (!args)
            return;

        auto rawDir = env->GetStringUTFChars(args->app_data_dir, nullptr);
        auto rawName = env->GetStringUTFChars(args->nice_name, nullptr);

        std::string dir, name;

        if (rawDir) {
            dir = rawDir;
            env->ReleaseStringUTFChars(args->app_data_dir, rawDir);
        }

        if (rawName) {
            name = rawName;
            env->ReleaseStringUTFChars(args->nice_name, rawName);
        }

        bool isGms = dir.ends_with("/com.google.android.gms");
        bool isGmsUnstable = isGms && name == "com.google.android.gms.unstable";

        if (!isGms)
            return;

        api->setOption(FORCE_DENYLIST_UNMOUNT);

        if (!isGmsUnstable)
            return;

        auto fd = api->connectCompanion();

        int size = static_cast<int>(dir.size());
        xwrite(fd, &size, sizeof(int));

        xwrite(fd, dir.data(), size);

        xread(fd, &trickyStore, sizeof(bool));

        xread(fd, &testSignedRom, sizeof(bool));

        bool ok = false;
        xread(fd, &ok, sizeof(bool));

        close(fd);

        if (ok)
            gmsDir = dir;
    }

    void postAppSpecialize(const AppSpecializeArgs *args) override {
        if (gmsDir.empty())
            return;

        typedef bool (*InitFuncPtr)(JavaVM *, const std::string &, bool, bool);

        void *handle = dlopen((gmsDir + "/libinject.so").c_str(), RTLD_NOW);

        if (!handle)
            return;

        auto init_func = reinterpret_cast<InitFuncPtr>(dlsym(handle, "init"));

        JavaVM *vm = nullptr;
        env->GetJavaVM(&vm);

        if (init_func(vm, gmsDir, trickyStore, testSignedRom)) {
            LOGD("dlclose injected lib");
            dlclose(handle);
        }
    }

    void preServerSpecialize(ServerSpecializeArgs *args) override {
        api->setOption(DLCLOSE_MODULE_LIBRARY);
    }

private:
    Api *api = nullptr;
    JNIEnv *env = nullptr;
    std::string gmsDir;
    bool trickyStore = false;
    bool testSignedRom = false;
};

REGISTER_ZYGISK_MODULE(PlayIntegrityFix)

REGISTER_ZYGISK_COMPANION(companion)