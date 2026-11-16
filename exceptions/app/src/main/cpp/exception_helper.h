#pragma once

// Copyright (C) 2022 The Android Open Source Project
// SPDX-License-Identifier: Apache-2.0

/*
 * Helper functions for throwing Java exceptions from C++.
 *
 * Copied, with a few changes, from JNIHelp.{h,c} in the Android platform's
 * libnativehelper:
 *
 * https://android.googlesource.com/platform/libnativehelper/+/refs/heads/master/include/nativehelper/JNIHelp.h
 * https://android.googlesource.com/platform/libnativehelper/+/refs/heads/master/JNIHelp.c
 */

#include <jni.h>

/*
 * Throw an exception with the specified class and an optional message.
 *
 * The "className" argument will be passed directly to FindClass, which
 * takes strings with slashes (e.g. "java/lang/Object").
 *
 * If an exception is currently pending, we log a warning message and
 * clear it.
 *
 * Returns 0 on success, nonzero if something failed (e.g. the exception
 * class couldn't be found, so *an* exception will still be pending).
 *
 * Currently aborts the VM if it can't throw the exception.
 */
int jniThrowException(JNIEnv* env, const char* className, const char* msg);

static inline int jniThrowRuntimeException(JNIEnv* env, const char* msg) {
  return jniThrowException(env, "java/lang/RuntimeException", msg);
}