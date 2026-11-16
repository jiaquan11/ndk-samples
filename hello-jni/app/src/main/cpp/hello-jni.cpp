// Copyright (C) 2016 The Android Open Source Project
// SPDX-License-Identifier: Apache-2.0

#include <iterator>
#include <jni.h>

#include <string>

jstring StringFromJni(JNIEnv* env, jobject) {
  std::string hello = "Hello from JNI.";
  return env->NewStringUTF(hello.c_str());
}

static jint RegisterNativeMethods(JNIEnv* env) {
  jclass c = env->FindClass("com/example/hellojni/HelloJni");
  if (c == nullptr) return JNI_ERR;

  static const JNINativeMethod methods[] = {
      {"stringFromJNI", "()Ljava/lang/String;",
       reinterpret_cast<void*>(StringFromJni)},
  };
  return env->RegisterNatives(c, methods,
                              static_cast<jint>(std::size(methods)));
}

extern "C" JNIEXPORT jint JNI_OnLoad(JavaVM* _Nonnull vm, void* _Nullable) {
  JNIEnv* env;
  if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
    return JNI_ERR;
  }

  const jint registration_result = RegisterNativeMethods(env);
  if (registration_result != JNI_OK) return registration_result;

  return JNI_VERSION_1_6;
}
