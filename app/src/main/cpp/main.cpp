#include <android/log.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dlfcn.h>
#include "zygisk.hpp"

#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, "PIF", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "PIF", __VA_ARGS__)

#define DEX_PATH "/data/adb/modules/playintegrityfix/classes.dex"

#define LIB_64 "/data/adb/modules/playintegrityfix/inject/arm64-v8a.so"
#define LIB_32 "/data/adb/modules/playintegrityfix/inject/armeabi-v7a.so"

#define DEFAULT_JSON "/data/adb/modules/playintegrityfix/pif.json"
#define CUSTOM_JSON_FORK "/data/adb/modules/playintegrityfix/custom.pif.json"
#define CUSTOM_JSON "/data/adb/pif.json"

#define TS_PATH "/data/adb/modules/tricky_store"

static ssize_t xread(int fd, void *buffer, size_t count) {
    ssize_t total = 0;
    char *buf = static_cast<char *>(buffer);
    while (count > 0) {
        ssize_t ret = read(fd, buf, count);
        if (ret < 0) return -1;
        buf += ret;
        total += ret;
        count -= ret;
    }
    return total;
}

static ssize_t xwrite(int fd, const void *buffer, size_t count) {
    ssize_t total = 0;
    const char *buf = static_cast<const char *>(buffer);
    while (count > 0) {
        ssize_t ret = write(fd, buf, count);
        if (ret < 0) return -1;
        buf += ret;
        total += ret;
        count -= ret;
    }
    return total;
}

static bool copyFile(const char *origin, const char *dest, mode_t perms) {
    int fd_in, fd_out;
    ssize_t bytes_read, bytes_written;
    char buffer[4096];

    fd_in = open(origin, O_RDONLY);
    if (fd_in < 0) {
        return false;
    }

    fd_out = open(dest, O_WRONLY | O_CREAT | O_TRUNC, perms);
    if (fd_out < 0) {
        close(fd_in);
        return false;
    }

    while ((bytes_read = read(fd_in, buffer, sizeof(buffer))) > 0) {
        ssize_t total_written = 0;
        while (total_written < bytes_read) {
            bytes_written = write(fd_out, buffer + total_written, bytes_read - total_written);
            if (bytes_written < 0) {
                close(fd_in);
                close(fd_out);
                return false;
            }
            total_written += bytes_written;
        }
    }

    if (bytes_read < 0) {
        close(fd_in);
        close(fd_out);
        return false;
    }

    fchmod(fd_out, perms);

    close(fd_in);
    close(fd_out);

    return true;
}

static char *concatStr(const char *str1, const char *str2) {
    size_t len = strlen(str1) + strlen(str2) + 1;

    char *buffer = static_cast<char *>(calloc(len, sizeof(char)));

    strcpy(buffer, str1);

    strcat(buffer, str2);

    return buffer;
}

static void companion(int fd) {
    size_t len = 0;
    xread(fd, &len, sizeof(size_t));

    char *dir = static_cast<char *>(calloc(len, sizeof(char)));
    ssize_t size = xread(fd, dir, len);
    dir[size] = '\0';

    LOGD("[COMPANION] GMS dir: %s", dir);

    char *libFile = concatStr(dir, "/libinject.so");
#if defined(__aarch64__)
    copyFile(LIB_64, libFile, 0777);
#elif defined(__arm__)
    copyFile(LIB_32, libFile, 0777);
#endif
    free(libFile);

    LOGD("[COMPANION] copied lib");

    char *dexFile = concatStr(dir, "/classes.dex");
    copyFile(DEX_PATH, dexFile, 0644);
    free(dexFile);

    LOGD("[COMPANION] copied dex");

    char *jsonFile = concatStr(dir, "/pif.json");
    if (!copyFile(CUSTOM_JSON, jsonFile, 0777)) {
        if (!copyFile(CUSTOM_JSON_FORK, jsonFile, 0777)) {
            copyFile(DEFAULT_JSON, jsonFile, 0777);
        }
    }
    free(jsonFile);

    LOGD("[COMPANION] copied json");

    free(dir);

    bool ok = true;
    xwrite(fd, &ok, sizeof(bool));

    LOGD("[COMPANION] end");
}

class PlayIntegrityFix : public zygisk::ModuleBase {
public:
    void onLoad(zygisk::Api *api_, JNIEnv *env_) override {
        this->api = api_;
        this->env = env_;
    }

    void preAppSpecialize(zygisk::AppSpecializeArgs *args) override {
        api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);

        bool isGms = false, isGmsUnstable = false;

        const char *name = env->GetStringUTFChars(args->nice_name, nullptr);

        if (name) {
            isGmsUnstable = strcmp(name, "com.google.android.gms.unstable") == 0;
            env->ReleaseStringUTFChars(args->nice_name, name);
        }

        const char *dir = env->GetStringUTFChars(args->app_data_dir, nullptr);

        if (dir) {
            isGms = strstr(dir, "/com.google.android.gms") != nullptr;
            isGmsUnstable &= isGms;
            if (isGmsUnstable) {
                targetDir = strdup(dir);
            }
            env->ReleaseStringUTFChars(args->app_data_dir, dir);
        }

        if (!isGms)
            return;

        api->setOption(zygisk::FORCE_DENYLIST_UNMOUNT);

        if (!isGmsUnstable)
            return;

        int fd = api->connectCompanion();

        size_t len = strlen(targetDir) + 1;
        xwrite(fd, &len, sizeof(size_t));
        xwrite(fd, targetDir, len);

        bool ok = false;
        xread(fd, &ok, sizeof(bool));

        close(fd);
    }

    void postAppSpecialize(const zygisk::AppSpecializeArgs *args) override {
        if (!targetDir) return;

        char *lib = concatStr(targetDir, "/libinject.so");
        void *handle = dlopen(lib, RTLD_NOW);
        free(lib);

        if (!handle) {
            LOGE("Error loading lib: %s", dlerror());
            free(targetDir);
            return;
        }

        dlerror();

        void (*init)(char *, JavaVM *);
        *(void **) (&init) = dlsym(handle, "init");

        const char *error = dlerror();
        if (error) {
            LOGE("Error loading symbol: %s", error);
            dlclose(handle);
            free(targetDir);
            return;
        }

        JavaVM *jvm = nullptr;
        env->GetJavaVM(&jvm);

        init(targetDir, jvm);

        free(targetDir);
    }

    void preServerSpecialize(zygisk::ServerSpecializeArgs *args) override {
        api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
    }

private:
    zygisk::Api *api = nullptr;
    JNIEnv *env = nullptr;
    char *targetDir = nullptr;
};

REGISTER_ZYGISK_MODULE(PlayIntegrityFix)

REGISTER_ZYGISK_COMPANION(companion)
