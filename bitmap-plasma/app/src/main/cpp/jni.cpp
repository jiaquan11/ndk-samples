// Copyright (C) 2025 The Android Open Source Project
// SPDX-License-Identifier: Apache-2.0

#include <base/macros.h>
#include <jni.h>

#include "plasma.h"

static jint RegisterNativeMethods(JNIEnv* env) {
  jclass c = env->FindClass("com/example/plasma/PlasmaView");
  if (c == nullptr) return JNI_ERR;

  static const JNINativeMethod methods[] = {
      {"renderPlasma", "(Landroid/graphics/Bitmap;J)V",
       reinterpret_cast<void*>(RenderPlasma)},
  };
  return env->RegisterNatives(c, methods, arraysize(methods));
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
