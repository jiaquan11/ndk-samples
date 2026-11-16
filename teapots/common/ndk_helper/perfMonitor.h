// Copyright 2013 The Android Open Source Project
// SPDX-License-Identifier: Apache-2.0

#ifndef PERFMONITOR_H_
#define PERFMONITOR_H_

#include <errno.h>
#include <jni.h>
#include <time.h>

#include "JNIHelper.h"

namespace ndk_helper {

const int32_t kNumSamples = 100;

/******************************************************************
 * Helper class for a performance monitoring and get current tick time
 */
class PerfMonitor {
 private:
  float current_FPS_;
  time_t tv_last_sec_;

  double last_tick_;
  int32_t tickindex_;
  double ticksum_;
  double ticklist_[kNumSamples];

  double UpdateTick(double current_tick);

 public:
  PerfMonitor();
  virtual ~PerfMonitor();

  bool Update(float& fFPS);

  static double GetCurrentTime() {
    struct timeval time;
    gettimeofday(&time, NULL);
    double ret = time.tv_sec + time.tv_usec * 1.0 / 1000000.0;
    return ret;
  }
};

}  // namespace ndk_helper
#endif /* PERFMONITOR_H_ */
