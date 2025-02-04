#include <android/log.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
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
#define TS_PATH_DISABLE "/data/adb/modules/tricky_store/disable"
#define TS_PATH_REMOVE "/data/adb/modules/tricky_store/remove"

#define TS_TARGET "/data/adb/tricky_store/target.txt"

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
    char *buf = (char *) buffer;
    while (count > 0) {
        ssize_t ret = write(fd, buf, count);
        if (ret < 0) return -1;
        buf += ret;
        total += ret;
        count -= ret;
    }
    return total;
}

static bool createFile(const char *path, mode_t perms) {
    FILE *file = fopen(path, "w");

    if (file == nullptr) {
        LOGE("[COMPANION] Failed to create file: %s", path);
        return false;
    }

    if (chmod(path, perms) == -1) {
        LOGE("[COMPANION] Failed to set permissions on destination file: %s", path);
        fclose(file);
        return false;
    }

    fclose(file);

    return true;
}

static bool copyFile(const char *origin, const char *dest, mode_t perms) {
    int input, output;
    struct stat stat_buf{};
    off_t offset = 0;

    if ((input = open(origin, O_RDONLY)) == -1) {
        LOGE("[COMPANION] Failed to open source file: %s", origin);
        return false;
    }

    if (fstat(input, &stat_buf) == -1) {
        LOGE("[COMPANION] Failed to stat source file: %s", origin);
        close(input);
        return false;
    }

    if ((output = open(dest, O_WRONLY | O_CREAT | O_TRUNC, perms)) == -1) {
        LOGE("[COMPANION] Failed to open destination file: %s", dest);
        close(input);
        return false;
    }

    ssize_t bytes_copied = sendfile(output, input, &offset, stat_buf.st_size);
    if (bytes_copied == -1) {
        LOGE("[COMPANION] Failed to copy file: %s", origin);
        close(input);
        close(output);
        return false;
    }

    close(input);
    close(output);

    if (chmod(dest, perms) == -1) {
        LOGE("[COMPANION] Failed to set permissions on destination file: %s", dest);
        return false;
    }

    return true;
}

static char *concatStr(const char *str1, const char *str2) {
    size_t len = strlen(str1) + strlen(str2) + 1;

    char *buffer = static_cast<char *>(calloc(len, sizeof(char)));

    strcpy(buffer, str1);

    strcat(buffer, str2);

    return buffer;
}

static bool trickyStoreExists() {
    struct stat st{};

    if (stat(TS_PATH, &st) == 0 && S_ISDIR(st.st_mode)) {
        if (stat(TS_PATH_DISABLE, &st) != 0) {
            if (stat(TS_PATH_REMOVE, &st) != 0) {
                return true;
            }
        }
    }

    return false;
}

static bool checkOtaZip() {
    char buffer[256] = {0};
    char result[1024 * 10] = {0};
    bool found = false;

    FILE *pipe = popen("unzip -l /system/etc/security/otacerts.zip", "r");
    if (!pipe) return false;

    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        strcat(result, buffer);
        if (strstr(result, "test") != nullptr) {
            found = true;
            break;
        }
    }

    pclose(pipe);
    return found;
}

static void playIntegrityApiHandleNewChecks() {
    FILE *file = fopen(TS_TARGET, "r+");
    if (file == nullptr) {
        return;
    }

    char line[256];
    bool android_found = false, vending_found = false;

    while (fgets(line, sizeof(line), file) != nullptr) {
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
        }

        if (strcmp(line, "android") == 0 || strcmp(line, "android!") == 0 || strcmp(line, "android?") == 0) {
            android_found = true;
        }

        if (strcmp(line, "com.android.vending") == 0 || strcmp(line, "com.android.vending!") == 0 || strcmp(line, "com.android.vending?") == 0) {
            vending_found = true;
        }
    }

    fseek(file, 0, SEEK_END);

    if (!android_found) {
        fprintf(file, "android\n");
        LOGE("[COMPANION] add 'android' to target.txt");
    }

    if (!vending_found) {
        fprintf(file, "com.android.vending\n");
        LOGE("[COMPANION] add 'com.android.vending' to target.txt");
    }

    fclose(file);
}

static void companion(int fd) {
    bool ok = true;
    size_t len = 0;
    xread(fd, &len, sizeof(size_t));

    char *dir = static_cast<char *>(calloc(len + 1, sizeof(char)));
    ssize_t size = xread(fd, dir, len);
    dir[size] = '\0';

    LOGD("[COMPANION] GMS dir: %s", dir);

    char *libFile = concatStr(dir, "/libinject.so");
#if defined(__aarch64__)
    ok &= copyFile(LIB_64, libFile, 0777);
#elif defined(__arm__)
    ok &= copyFile(LIB_32, libFile, 0777);
#endif
    free(libFile);

    LOGD("[COMPANION] copied lib");

    char *dexFile = concatStr(dir, "/classes.dex");
    ok &= copyFile(DEX_PATH, dexFile, 0644);
    free(dexFile);

    LOGD("[COMPANION] copied dex");

    char *jsonFile = concatStr(dir, "/pif.json");
    if (!copyFile(CUSTOM_JSON, jsonFile, 0777)) {
        if (!copyFile(CUSTOM_JSON_FORK, jsonFile, 0777)) {
            if (!copyFile(DEFAULT_JSON, jsonFile, 0777)) {
                ok = false;
            }
        }
    }
    free(jsonFile);

    LOGD("[COMPANION] copied json");

    char *ts = concatStr(dir, "/trickystore");
    if (trickyStoreExists()) {
        ok &= createFile(ts, 0777);
        playIntegrityApiHandleNewChecks();
        LOGD("[COMPANION] trickystore detected!");
    } else {
        remove(ts);
    }
    free(ts);

    char *unsign = concatStr(dir, "/unsign");
    if (checkOtaZip()) {
        ok &= createFile(unsign, 0777);
        LOGD("[COMPANION] test-keys signed rom detected!");
    } else {
        remove(unsign);
    }
    free(unsign);

    free(dir);

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

        if (!ok) {
            LOGE("ERROR");
            free(targetDir);
            targetDir = nullptr;
        }

        close(fd);
    }

    void postAppSpecialize(const zygisk::AppSpecializeArgs *args) override {
        if (!targetDir) return;

        char *lib = concatStr(targetDir, "/libinject.so");
        void *handle = dlopen(lib, RTLD_LAZY);
        free(lib);

        if (!handle) {
            LOGE("Error loading lib: %s", dlerror());
            free(targetDir);
            return;
        }

        dlerror();

        bool (*init)(char *, JavaVM *);
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

        bool close = true;

        if (jvm)
            close = init(targetDir, jvm);
        else
            LOGE("jvm is null!");

        free(targetDir);

        if (close) {
            dlclose(handle);
            LOGD("dlclose injected lib!");
        }

        LOGD("DONE");
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
