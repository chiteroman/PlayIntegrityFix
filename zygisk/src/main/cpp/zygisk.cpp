#include "zygisk.hpp"
#include <android/log.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, "PIF", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "PIF", __VA_ARGS__)

#define DEX_PATH "/data/adb/modules/playintegrityfix/classes.jar"

#define LIB_64 "/data/adb/modules/playintegrityfix/inject/arm64-v8a.so"
#define LIB_32 "/data/adb/modules/playintegrityfix/inject/armeabi-v7a.so"

#define DEFAULT_JSON "/data/adb/modules/playintegrityfix/pif.json"
#define CUSTOM_JSON_FORK "/data/adb/modules/playintegrityfix/custom.pif.json"
#define CUSTOM_JSON "/data/adb/pif.json"

static ssize_t xread(int fd, void *buffer, size_t count) {
    ssize_t total = 0;
    auto buf = static_cast<char *>(buffer);
    while (count > 0) {
        auto ret = TEMP_FAILURE_RETRY(read(fd, buf, count));
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
        auto ret = TEMP_FAILURE_RETRY(write(fd, buf, count));
        if (ret < 0)
            return -1;
        buf += ret;
        total += ret;
        count -= ret;
    }
    return total;
}

static bool endsWith(const char *str, const char *suffix) {
    if (!str || !suffix)
        return false;

    auto suffix_len = strlen(suffix);
    auto str_len = strlen(str);

    if (suffix_len > str_len)
        return false;

    return strncmp(str + (str_len - suffix_len), suffix, suffix_len) == 0;
}

static char *concatStr(const char *str1, const char *str2) {
    if (!str1 || !str2)
        return nullptr;

    auto len1 = strlen(str1);
    auto len2 = strlen(str2);
    auto total_length = len1 + len2;

    auto concatenated_string =
            static_cast<char *>(calloc(total_length + 1, sizeof(char)));
    if (!concatenated_string)
        return nullptr;

    strcpy(concatenated_string, str1);
    strcat(concatenated_string, str2);

    return concatenated_string;
}

static bool copyFile(const char *source_path, const char *dest_path,
                     mode_t permissions) {
    int source_fd;
    int dest_fd;
    char buffer[8192];
    ssize_t bytes_read, bytes_written;

    source_fd = open(source_path, O_RDONLY);
    if (source_fd == -1) {
        LOGE("Error opening source file: %s", strerror(errno));
        return false;
    }

    dest_fd = open(dest_path, O_WRONLY | O_CREAT | O_TRUNC, permissions);
    if (dest_fd == -1) {
        LOGE("Error opening destination file: %s", strerror(errno));
        close(source_fd);
        return false;
    }

    while ((bytes_read = read(source_fd, buffer, 8192)) > 0) {
        bytes_written = write(dest_fd, buffer, bytes_read);
        if (bytes_written == -1) {
            LOGE("Error writing to destination file: %s", strerror(errno));
            close(source_fd);
            close(dest_fd);
            return false;
        }
        if (bytes_written != bytes_read) {
            LOGD("Warning: Incomplete write to destination file.");
        }
    }

    if (bytes_read == -1) {
        LOGE("Error reading from source file: %s", strerror(errno));
        close(source_fd);
        close(dest_fd);
        return false;
    }

    if (chmod(dest_path, permissions) == -1) {
        LOGE("Error setting file permissions: %s", strerror(errno));
        close(source_fd);
        close(dest_fd);
        return false;
    }

    close(source_fd);
    close(dest_fd);

    return true;
}

static void companion(int fd) {
    bool ok = true;

    size_t size = 0;
    xread(fd, &size, sizeof(size_t));

    auto dir = static_cast<char *>(calloc(size + 1, sizeof(char)));
    xread(fd, dir, size);

    LOGD("[COMPANION] GMS dir: %s", dir);

    auto libFile = concatStr(dir, "/libinject.so");
#if defined(__aarch64__)
    ok &= copyFile(LIB_64, libFile, 0777);
#elif defined(__arm__)
    ok &= copyFile(LIB_32, libFile, 0777);
#endif
    free(libFile);

    LOGD("[COMPANION] copied inject lib");

    auto dexFile = concatStr(dir, "/classes.jar");
    ok &= copyFile(DEX_PATH, dexFile, 0644);
    free(dexFile);

    LOGD("[COMPANION] copied dex");

    auto jsonFile = concatStr(dir, "/pif.json");
    if (!copyFile(CUSTOM_JSON, jsonFile, 0777)) {
        if (!copyFile(CUSTOM_JSON_FORK, jsonFile, 0777)) {
            if (!copyFile(DEFAULT_JSON, jsonFile, 0777)) {
                ok = false;
            }
        }
    }
    free(jsonFile);

    LOGD("[COMPANION] copied json");

    free(dir);

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
        const char *name{};
        int fd;
        size_t size;
        bool ok = false;

        api->setOption(DLCLOSE_MODULE_LIBRARY);

        dir = env->GetStringUTFChars(args->app_data_dir, nullptr);

        if (!endsWith(dir, "/com.google.android.gms")) {
            goto clear;
        }

        if (endsWith(dir, "/com.android.vending")) {
            api->setOption(FORCE_DENYLIST_UNMOUNT);
            goto clear;
        }

        api->setOption(FORCE_DENYLIST_UNMOUNT);

        name = env->GetStringUTFChars(args->nice_name, nullptr);

        if (strcmp(name, "com.google.android.gms.unstable") != 0) {
            goto clear;
        }

        env->ReleaseStringUTFChars(args->nice_name, name);

        fd = api->connectCompanion();

        size = strlen(dir);
        xwrite(fd, &size, sizeof(size_t));

        xwrite(fd, dir, size);

        xread(fd, &ok, sizeof(bool));

        close(fd);

        if (!ok) {
            LOGE("ERROR");
            goto clear;
        }

        return;

        clear:
        if (name)
            env->ReleaseStringUTFChars(args->nice_name, name);
        if (dir)
            env->ReleaseStringUTFChars(args->app_data_dir, dir);
        dir = nullptr;
    }

    void postAppSpecialize(const AppSpecializeArgs *args) override {
        if (!dir)
            return;

        typedef bool (*InitFunc)(JavaVM *, const char *);

        char *lib{};
        void *handle{};
        InitFunc initPtr;
        const char *error{};
        bool close = false;
        JavaVM *vm{};

        lib = concatStr(dir, "/libinject.so");
        handle = dlopen(lib, RTLD_NOW);
        free(lib);

        if (!handle) {
            LOGE("Error loading lib: %s", dlerror());
            goto clear;
        }

        dlerror();

        initPtr = (InitFunc) dlsym(handle, "init");

        error = dlerror();
        if (error) {
            LOGE("Error loading symbol: %s", error);
            dlclose(handle);
            goto clear;
        }

        env->GetJavaVM(&vm);

        if (vm)
            close = initPtr(vm, dir);
        else {
            LOGE("jvm is null!");
            dlclose(handle);
            goto clear;
        }

        LOGD("DONE");

        if (close) {
            dlclose(handle);
            LOGD("dlclose injected lib!");
        }

        clear:
        env->ReleaseStringUTFChars(args->app_data_dir, dir);
        dir = nullptr;
        LOGD("clear");
    }

    void preServerSpecialize(ServerSpecializeArgs *args) override {
        api->setOption(DLCLOSE_MODULE_LIBRARY);
    }

private:
    Api *api{};
    JNIEnv *env{};
    const char *dir{};
};

REGISTER_ZYGISK_MODULE(PlayIntegrityFix)

REGISTER_ZYGISK_COMPANION(companion)
