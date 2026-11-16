// Copyright 2013 The Android Open Source Project
// SPDX-License-Identifier: Apache-2.0

#ifndef _NDKSUPPORT_H
#define _NDKSUPPORT_H

#define NDK_HELPER_VERSION "0.90"

/******************************************************************
 * NDK support helpers
 * Utility module to provide misc functionalities that is used widely in native
 * applications,
 * such as gesture detection, jni bridge, openGL context etc.
 *
 * The purpose of this module is,
 * - Provide best practices using NDK
 * - Provide handy utility functions for NDK development
 * - Make NDK samples more simpler and readable
 */
#include "GLContext.h"        // EGL & OpenGL manager
#include "JNIHelper.h"        // JNI support
#include "gestureDetector.h"  // Tap/Doubletap/Pinch detector
#include "gl3stub.h"          // GLES3 stubs
#include "interpolator.h"     // Interpolator
#include "perfMonitor.h"      // FPS counter
#include "sensorManager.h"    // SensorManager
#include "shader.h"           // Shader compiler support
#include "tapCamera.h"        // Tap/Pinch camera control
#include "vecmath.h"  // Vector math support, C++ implementation n current version
#endif
