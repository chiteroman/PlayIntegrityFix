#include "log.h"
#include "zygisk.hpp"
#include <dlfcn.h>
#include <filesystem>
#include <string>

#if __aarch64__
#define INJECT_LIB                                                             \
  "/data/adb/modules/playintegrityfix/inject/arm64-v8a.so"
#elif __arm__
#define INJECT_LIB                                                             \
  "/data/adb/modules/playintegrityfix/inject/armeabi-v7a.so"
#endif

#define DEX_FILE "/data/adb/modules/playintegrityfix/classes.dex"

#define PIF_FILE "/data/adb/modules/playintegrityfix/pif.json"

#define CUSTOM_PIF_FILE "/data/adb/pif.json"

#define CUSTOM_PIF_FILE_1 "/data/adb/modules/playintegrityfix/custom.pif.json"

static inline ssize_t xread(int fd, void *buffer, size_t count) {
    auto *buf = static_cast<char *>(buffer);
    ssize_t total = 0;

    while (count > 0) {
        ssize_t ret = ::read(fd, buf, count);

        if (ret < 0) {
            // Retry if interrupted
            if (errno == EINTR) {
                continue;
            }

            return -1;
        }

        // If 0, we've hit EOF (read no more data)
        if (ret == 0) {
            break;
        }

        buf += ret;
        total += ret;
        count -= ret;
    }

    return total;
}

static inline ssize_t xwrite(int fd, const void *buffer, size_t count) {
    auto *buf = static_cast<const char *>(buffer);
    ssize_t total = 0;

    while (count > 0) {
        ssize_t ret = ::write(fd, buf, count);

        if (ret < 0) {
            // Retry if interrupted
            if (errno == EINTR) {
                continue;
            }

            return -1;
        }

        // Technically, write returning 0 is unusual (e.g., disk full); handle it if needed
        if (ret == 0) {
            break;
        }

        buf += ret;
        total += ret;
        count -= ret;
    }

    return total;
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
        if (result.find("test") != std::string::npos) {
            found = true;
            break;
        }
    }

    return found;
}

static std::string gmsDir;

static bool
copyFile(const std::string &origin, const std::string &path,
         std::filesystem::perms perms = std::filesystem::perms::all) {
    std::string dest = gmsDir + "/" + path;

    bool ok = std::filesystem::copy_file(
            origin, dest, std::filesystem::copy_options::overwrite_existing);

    std::error_code ec;

    std::filesystem::permissions(dest, perms, ec);

    ok &= ec.value() == 0;

    return ok;
}

static void companion(int fd) {
    bool ok = false;

    size_t size = 0;
    xread(fd, &size, sizeof(size));

    gmsDir.resize(size);

    size = xread(fd, gmsDir.data(), size);

    gmsDir.resize(size);
    gmsDir[size] = '\0';
    gmsDir.shrink_to_fit();

    LOGD("[ROOT] GMS dir: %s", gmsDir.c_str());

    ok = copyFile(INJECT_LIB, "libinject.so");

    ok &= copyFile(DEX_FILE, "classes.dex",
                   std::filesystem::perms::owner_read |
                   std::filesystem::perms::group_read |
                   std::filesystem::perms::others_read);

    if (std::filesystem::exists(CUSTOM_PIF_FILE)) {
        ok &= copyFile(CUSTOM_PIF_FILE, "pif.json");
    } else if (std::filesystem::exists(CUSTOM_PIF_FILE_1)) {
        ok &= copyFile(CUSTOM_PIF_FILE_1, "pif.json");
    } else if (std::filesystem::exists(PIF_FILE)) {
        ok &= copyFile(PIF_FILE, "pif.json");
    } else {
        ok = false;
    }

    std::string ts("/data/adb/modules/tricky_store");
    bool trickyStore = std::filesystem::exists(ts) &&
                       !std::filesystem::exists(ts + "/disable") &&
                       !std::filesystem::exists(ts + "/remove");
    if (trickyStore) {
        FILE *file = fopen((gmsDir + "/trickystore").c_str(), "w");
        if (file)
            fclose(file);
    } else {
        if (std::filesystem::exists(gmsDir + "/trickystore")) {
            std::filesystem::remove(gmsDir + "/trickystore");
        }
    }

    bool testSignedRom = checkOtaZip();
    if (testSignedRom) {
        FILE *file = fopen((gmsDir + "/testsign").c_str(), "w");
        if (file)
            fclose(file);
    } else {
        if (std::filesystem::exists(gmsDir + "/testsign")) {
            std::filesystem::remove(gmsDir + "/testsign");
        }
    }

    gmsDir.clear();
    gmsDir.shrink_to_fit();

    LOGD("[ROOT] OK? %d", ok);

    write(fd, &ok, sizeof(ok));
}

using namespace zygisk;

class PlayIntegrityFix : public ModuleBase {
public:
    void onLoad(Api *_api, JNIEnv *_env) override {
        this->api = _api;
        this->env = _env;
    }

    void preAppSpecialize(AppSpecializeArgs *args) override {
        api->setOption(DLCLOSE_MODULE_LIBRARY);

        if (!args)
            return;

        bool isGms = false, isGmsUnstable = false;

        auto name = env->GetStringUTFChars(args->nice_name, nullptr);
        auto dir = env->GetStringUTFChars(args->app_data_dir, nullptr);

        if (name) {
            isGmsUnstable =
                    std::string_view(name) == "com.google.android.gms.unstable";
            env->ReleaseStringUTFChars(args->nice_name, name);
        }

        if (dir) {
            isGms = std::string_view(dir).ends_with("/com.google.android.gms");
            isGmsUnstable &= isGms;
            if (isGmsUnstable) {
                gmsDir = dir;
            }
            env->ReleaseStringUTFChars(args->app_data_dir, dir);
        }

        if (!isGms)
            return;

        api->setOption(FORCE_DENYLIST_UNMOUNT);

        if (!isGmsUnstable)
            return;

        LOGD("We are in GMS unstable process!");

        if (gmsDir.empty()) {
            LOGE("dir is empty, wtf?");
            return;
        }

        LOGD("GMS dir: %s", gmsDir.c_str());

        int fd = api->connectCompanion();

        size_t size = gmsDir.size();
        xwrite(fd, &size, sizeof(size));

        xwrite(fd, gmsDir.data(), size);

        bool done = false;
        xread(fd, &done, sizeof(done));

        close(fd);

        if (!done)
            gmsDir.clear();
    }

    void postAppSpecialize(const AppSpecializeArgs *args) override {
        if (!args || gmsDir.empty())
            return;

        void *handle = dlopen((gmsDir + "/libinject.so").c_str(), RTLD_NOW);
        if (!handle) {
            LOGE("Error loading lib: %s", dlerror());
            return;
        }

        dlerror();

        typedef void (*init_t)(const char *, JavaVM *);

        auto init_func = reinterpret_cast<init_t>(dlsym(handle, "init"));
        const char *error = dlerror();
        if (error) {
            LOGE("Error loading lib: %s", error);
            dlclose(handle);
            return;
        }

        JavaVM *jvm = nullptr;
        auto result = env->GetJavaVM(&jvm);
        if (result != JNI_OK) {
            LOGE("couldn't get jvm");
            dlclose(handle);
            return;
        }

        init_func(gmsDir.c_str(), jvm);
    }

    void preServerSpecialize(ServerSpecializeArgs *args) override {
        api->setOption(DLCLOSE_MODULE_LIBRARY);
    }

private:
    JNIEnv *env = nullptr;
    Api *api = nullptr;
    std::string gmsDir;
};

REGISTER_ZYGISK_MODULE(PlayIntegrityFix)

REGISTER_ZYGISK_COMPANION(companion)