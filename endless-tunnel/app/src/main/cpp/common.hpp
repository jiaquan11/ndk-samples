// Copyright (C) Google Inc.
// SPDX-License-Identifier: Apache-2.0
#ifndef endlesstunnel_common_hpp
#define endlesstunnel_common_hpp

extern "C" {
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <android/sensor.h>
#include <android_native_app_glue.h>
#include <errno.h>
#include <jni.h>
#include <stdlib.h>
#include <unistd.h>
}

#define LOG_TAG "EndlessTunnel:Native"
#include <base/logging.h>

#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"

#define ABORT_GAME CHECKF(false, "*** GAME ABORTING.")
#define MY_ASSERT(cond) CHECKF(cond, "ASSERTION FAILED")

#include "our_key_codes.hpp"

#endif
