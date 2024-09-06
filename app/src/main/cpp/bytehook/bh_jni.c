// Copyright (c) 2020-2022 ByteDance, Inc.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

// Created by Kelun Cai (caikelun@bytedance.com) on 2020-06-02.

#include <jni.h>
#include <stdlib.h>

#include "bytehook.h"

#define BH_JNI_VERSION    JNI_VERSION_1_6
#define BH_JNI_CLASS_NAME "com/bytedance/android/bytehook/ByteHook"

static jstring bh_jni_get_version(JNIEnv *env, jobject thiz) {
  (void)thiz;
  return (*env)->NewStringUTF(env, bytehook_get_version());
}

static jint bh_jni_init(JNIEnv *env, jobject thiz, jint mode, jboolean debug) {
  (void)env;
  (void)thiz;

  return bytehook_init((int)mode, (bool)debug);
}

static jint bh_jni_add_ignore(JNIEnv *env, jobject thiz, jstring caller_path_name) {
  (void)env;
  (void)thiz;

  int r = BYTEHOOK_STATUS_CODE_IGNORE;
  if (!caller_path_name) return r;

  const char *c_caller_path_name;
  if (NULL == (c_caller_path_name = (*env)->GetStringUTFChars(env, caller_path_name, 0))) goto clean;
  r = bytehook_add_ignore(c_caller_path_name);

clean:
  if (caller_path_name && c_caller_path_name)
    (*env)->ReleaseStringUTFChars(env, caller_path_name, c_caller_path_name);
  return r;
}

static jint bh_jni_get_mode(JNIEnv *env, jobject thiz) {
  (void)env, (void)thiz;

  return BYTEHOOK_MODE_AUTOMATIC == bytehook_get_mode() ? 0 : 1;
}

static jboolean bh_jni_get_debug(JNIEnv *env, jobject thiz) {
  (void)env, (void)thiz;

  return bytehook_get_debug();
}

static void bh_jni_set_debug(JNIEnv *env, jobject thiz, jboolean debug) {
  (void)env;
  (void)thiz;

  bytehook_set_debug((bool)debug);
}

static jboolean bh_jni_get_recordable(JNIEnv *env, jobject thiz) {
  (void)env, (void)thiz;

  return bytehook_get_recordable();
}

static void bh_jni_set_recordable(JNIEnv *env, jobject thiz, jboolean recordable) {
  (void)env, (void)thiz;

  bytehook_set_recordable((bool)recordable);
}

static jstring bh_jni_get_records(JNIEnv *env, jobject thiz, jint item_flags) {
  (void)thiz;

  char *str = bytehook_get_records((uint32_t)item_flags);
  if (NULL == str) return NULL;

  jstring jstr = (*env)->NewStringUTF(env, str);
  free(str);
  return jstr;
}

static jstring bh_jni_get_arch(JNIEnv *env, jobject thiz) {
  (void)thiz;

#if defined(__arm__)
  char *arch = "arm";
#elif defined(__aarch64__)
  char *arch = "arm64";
#elif defined(__i386__)
  char *arch = "x86";
#elif defined(__x86_64__)
  char *arch = "x86_64";
#else
  char *arch = "unsupported";
#endif

  return (*env)->NewStringUTF(env, arch);
}

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved) {
  (void)reserved;

  if (__predict_false(NULL == vm)) return JNI_ERR;

  JNIEnv *env;
  if (__predict_false(JNI_OK != (*vm)->GetEnv(vm, (void **)&env, BH_JNI_VERSION))) return JNI_ERR;
  if (__predict_false(NULL == env || NULL == *env)) return JNI_ERR;

  jclass cls;
  if (__predict_false(NULL == (cls = (*env)->FindClass(env, BH_JNI_CLASS_NAME)))) return JNI_ERR;

  JNINativeMethod m[] = {{"nativeGetVersion", "()Ljava/lang/String;", (void *)bh_jni_get_version},
                         {"nativeInit", "(IZ)I", (void *)bh_jni_init},
                         {"nativeAddIgnore", "(Ljava/lang/String;)I", (void *)bh_jni_add_ignore},
                         {"nativeGetMode", "()I", (void *)bh_jni_get_mode},
                         {"nativeGetDebug", "()Z", (void *)bh_jni_get_debug},
                         {"nativeSetDebug", "(Z)V", (void *)bh_jni_set_debug},
                         {"nativeGetRecordable", "()Z", (void *)bh_jni_get_recordable},
                         {"nativeSetRecordable", "(Z)V", (void *)bh_jni_set_recordable},
                         {"nativeGetRecords", "(I)Ljava/lang/String;", (void *)bh_jni_get_records},
                         {"nativeGetArch", "()Ljava/lang/String;", (void *)bh_jni_get_arch}};
  if (__predict_false(0 != (*env)->RegisterNatives(env, cls, m, sizeof(m) / sizeof(m[0])))) return JNI_ERR;

  return BH_JNI_VERSION;
}
