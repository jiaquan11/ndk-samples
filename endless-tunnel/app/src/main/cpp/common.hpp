/*
 * Copyright (C) Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
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
