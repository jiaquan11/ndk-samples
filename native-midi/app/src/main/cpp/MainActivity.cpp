// Copyright (C) 2019 The Android Open Source Project
// SPDX-License-Identifier: Apache-2.0
#include <iterator>
#include <jni.h>

// Data callback stuff
JavaVM* theJvm;
jobject dataCallbackObj;
jmethodID midDataCallback;

/**
 * Initializes JNI interface stuff, specifically the info needed to call back
 * into the Java layer when MIDI data is received.
 */
void InitNative(JNIEnv* env, jobject instance) {
  env->GetJavaVM(&theJvm);

  // Setup the receive data callback (into Java)
  jclass clsMainActivity =
      env->FindClass("com/example/nativemidi/MainActivity");
  dataCallbackObj = env->NewGlobalRef(instance);
  midDataCallback =
      env->GetMethodID(clsMainActivity, "onNativeMessageReceive", "([B)V");
}

static jint RegisterNativeMethods(JNIEnv* env) {
  jclass c = env->FindClass("com/example/nativemidi/MainActivity");
  if (c == nullptr) return JNI_ERR;

  static const JNINativeMethod methods[] = {
      {"initNative", "()V", reinterpret_cast<void*>(InitNative)},
  };
  return env->RegisterNatives(c, methods,
                              static_cast<jint>(std::size(methods)));
}

extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* _Nonnull vm,
                                             void* _Nullable) {
  JNIEnv* env;
  if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
    return JNI_ERR;
  }

  const jint registration_result = RegisterNativeMethods(env);
  if (registration_result != JNI_OK) return registration_result;

  return JNI_VERSION_1_6;
}
