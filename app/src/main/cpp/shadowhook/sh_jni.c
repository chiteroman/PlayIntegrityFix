// Copyright (c) 2021-2024 ByteDance Inc.
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

// Created by Kelun Cai (caikelun@bytedance.com) on 2021-04-11.

#include <errno.h>
#include <jni.h>
#include <stdlib.h>
#include <unistd.h>

#include "sh_log.h"
#include "shadowhook.h"

#define SH_JNI_VERSION    JNI_VERSION_1_6
#define SH_JNI_CLASS_NAME "com/bytedance/shadowhook/ShadowHook"

static jstring sh_jni_get_version(JNIEnv *env, jobject thiz) {
  (void)thiz;
  return (*env)->NewStringUTF(env, shadowhook_get_version());
}

static jint sh_jni_init(JNIEnv *env, jobject thiz, jint mode, jboolean debuggable) {
  (void)env, (void)thiz;

  return shadowhook_init(0 == mode ? SHADOWHOOK_MODE_SHARED : SHADOWHOOK_MODE_UNIQUE, (bool)debuggable);
}

static jint sh_jni_get_init_errno(JNIEnv *env, jobject thiz) {
  (void)env, (void)thiz;

  return shadowhook_get_init_errno();
}

static jint sh_jni_get_mode(JNIEnv *env, jobject thiz) {
  (void)env, (void)thiz;

  return SHADOWHOOK_MODE_SHARED == shadowhook_get_mode() ? 0 : 1;
}

static jboolean sh_jni_get_debuggable(JNIEnv *env, jobject thiz) {
  (void)env, (void)thiz;

  return shadowhook_get_debuggable();
}

static void sh_jni_set_debuggable(JNIEnv *env, jobject thiz, jboolean debuggable) {
  (void)env, (void)thiz;

  shadowhook_set_debuggable((bool)debuggable);
}

static jboolean sh_jni_get_recordable(JNIEnv *env, jobject thiz) {
  (void)env, (void)thiz;

  return shadowhook_get_recordable();
}

static void sh_jni_set_recordable(JNIEnv *env, jobject thiz, jboolean recordable) {
  (void)env, (void)thiz;

  shadowhook_set_recordable((bool)recordable);
}

static jstring sh_jni_to_errmsg(JNIEnv *env, jobject thiz, jint error_number) {
  (void)thiz;

  return (*env)->NewStringUTF(env, shadowhook_to_errmsg(error_number));
}

static jstring sh_jni_get_records(JNIEnv *env, jobject thiz, jint item_flags) {
  (void)thiz;

  char *str = shadowhook_get_records((uint32_t)item_flags);
  if (NULL == str) return NULL;

  jstring jstr = (*env)->NewStringUTF(env, str);
  free(str);
  return jstr;
}

static jstring sh_jni_get_arch(JNIEnv *env, jobject thiz) {
  (void)thiz;

#if defined(__arm__)
  char *arch = "arm";
#elif defined(__aarch64__)
  char *arch = "arm64";
#else
  char *arch = "unsupported";
#endif

  return (*env)->NewStringUTF(env, arch);
}

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved) {
  (void)reserved;

  if (__predict_false(NULL == vm)) return JNI_ERR;

  JNIEnv *env;
  if (__predict_false(JNI_OK != (*vm)->GetEnv(vm, (void **)&env, SH_JNI_VERSION))) return JNI_ERR;
  if (__predict_false(NULL == env || NULL == *env)) return JNI_ERR;

  jclass cls;
  if (__predict_false(NULL == (cls = (*env)->FindClass(env, SH_JNI_CLASS_NAME)))) return JNI_ERR;

  JNINativeMethod m[] = {{"nativeGetVersion", "()Ljava/lang/String;", (void *)sh_jni_get_version},
                         {"nativeInit", "(IZ)I", (void *)sh_jni_init},
                         {"nativeGetInitErrno", "()I", (void *)sh_jni_get_init_errno},
                         {"nativeGetMode", "()I", (void *)sh_jni_get_mode},
                         {"nativeGetDebuggable", "()Z", (void *)sh_jni_get_debuggable},
                         {"nativeSetDebuggable", "(Z)V", (void *)sh_jni_set_debuggable},
                         {"nativeGetRecordable", "()Z", (void *)sh_jni_get_recordable},
                         {"nativeSetRecordable", "(Z)V", (void *)sh_jni_set_recordable},
                         {"nativeToErrmsg", "(I)Ljava/lang/String;", (void *)sh_jni_to_errmsg},
                         {"nativeGetRecords", "(I)Ljava/lang/String;", (void *)sh_jni_get_records},
                         {"nativeGetArch", "()Ljava/lang/String;", (void *)sh_jni_get_arch}};
  if (__predict_false(0 != (*env)->RegisterNatives(env, cls, m, sizeof(m) / sizeof(m[0])))) return JNI_ERR;

  return SH_JNI_VERSION;
}
