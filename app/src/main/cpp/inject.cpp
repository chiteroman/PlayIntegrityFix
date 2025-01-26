#include <android/log.h>
#include <jni.h>
#include "json.hpp"
#include "dobby.h"

#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, "PIF", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "PIF", __VA_ARGS__)

static std::string dir;
static JNIEnv *env;

extern "C" [[gnu::visibility("default"), maybe_unused]]
void init(char *rawDir, JavaVM *jvm) {
    dir = rawDir;
    LOGD("[INJECT] GMS dir: %s", dir.c_str());
    jvm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6);

    LOGD("[INJECT] Done!");
}