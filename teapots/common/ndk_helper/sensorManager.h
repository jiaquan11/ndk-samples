// Copyright 2013 The Android Open Source Project
// SPDX-License-Identifier: Apache-2.0

//--------------------------------------------------------------------------------
// sensorManager.h
//--------------------------------------------------------------------------------
#ifndef SENSORMANAGER_H_
#define SENSORMANAGER_H_

#include <android/sensor.h>

#include "JNIHelper.h"

namespace ndk_helper {
//--------------------------------------------------------------------------------
// Constants
//--------------------------------------------------------------------------------
enum ORIENTATION {
  ORIENTATION_UNKNOWN = -1,
  ORIENTATION_PORTRAIT = 0,
  ORIENTATION_LANDSCAPE = 1,
  ORIENTATION_REVERSE_PORTRAIT = 2,
  ORIENTATION_REVERSE_LANDSCAPE = 3,
};

/*
 * Helper to handle sensor inputs such as accelerometer.
 * The helper also check for screen rotation
 *
 */
class SensorManager {
  ASensorManager* sensorManager_;
  const ASensor* accelerometerSensor_;
  ASensorEventQueue* sensorEventQueue_;

 protected:
 public:
  SensorManager();
  ~SensorManager();
  void Init(android_app* state);
  void Suspend();
  void Resume();
};

/*
 * AcquireASensorManagerInstance(android_app* app)
 *    Workaround ASensorManager_getInstance() deprecation false alarm
 *    for Android-N and before, when compiling with NDK-r15
 */
ASensorManager* AcquireASensorManagerInstance(android_app* app);
}  // namespace ndk_helper
#endif /* SENSORMANAGER_H_ */
