// Copyright (C) Google Inc.
// SPDX-License-Identifier: Apache-2.0
// clang-format off
#include "common.hpp"
// clang-format on
#include "jni_util.hpp"

#include "native_engine.hpp"

static struct JniSetup _jni_setup = {};

struct JniSetup* GetJNISetup() {
  if (!_jni_setup.env) {
    _jni_setup.env = NativeEngine::GetInstance()->GetJniEnv();
    _jni_setup.thiz =
        NativeEngine::GetInstance()->GetAndroidApp()->activity->clazz;
    _jni_setup.clazz = _jni_setup.env->GetObjectClass(_jni_setup.thiz);
  }
  return &_jni_setup;
}
