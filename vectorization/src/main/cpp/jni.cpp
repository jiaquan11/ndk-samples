// Copyright (C) 2024 The Android Open Source Project
// SPDX-License-Identifier: Apache-2.0

#include <iterator>
#include <jni.h>

#include <expected>
#include <optional>

#include "benchmark.h"

using samples::vectorization::Backend;
using samples::vectorization::BenchmarkMatrixMultiplication;

static jlong BenchmarkMatrixMultiplyJni(JNIEnv* _Nonnull /* env */,
                                        jobject _Nonnull /* this */,
                                        jint backend) {
  auto result = BenchmarkMatrixMultiplication(static_cast<Backend>(backend));
  if (result.has_value()) {
    return result->count();
  }
  return static_cast<jlong>(result.error());
}

static jint RegisterNativeMethods(JNIEnv* env) {
  jclass c = env->FindClass("com/android/ndk/samples/vectorization/AppJni");
  if (c == nullptr) return JNI_ERR;

  static const JNINativeMethod methods[] = {
      {"benchmarkMatrixMultiply", "(I)J",
       reinterpret_cast<void*>(BenchmarkMatrixMultiplyJni)},
  };
  return env->RegisterNatives(c, methods,
                              static_cast<jint>(std::size(methods)));
}

JNIEXPORT jint JNI_OnLoad(JavaVM* _Nonnull vm,
                          void* _Nullable reserved __unused) {
  JNIEnv* env;
  if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
    return JNI_ERR;
  }

  const jint registration_result = RegisterNativeMethods(env);
  if (registration_result != JNI_OK) return registration_result;

  return JNI_VERSION_1_6;
}
