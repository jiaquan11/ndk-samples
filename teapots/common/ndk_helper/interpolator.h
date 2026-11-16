// Copyright 2013 The Android Open Source Project
// SPDX-License-Identifier: Apache-2.0

#ifndef INTERPOLATOR_H_
#define INTERPOLATOR_H_

#include <errno.h>
#include <jni.h>
#include <time.h>

#include <list>

#include "JNIHelper.h"
#include "perfMonitor.h"

namespace ndk_helper {

enum INTERPOLATOR_TYPE {
  INTERPOLATOR_TYPE_LINEAR,
  INTERPOLATOR_TYPE_EASEINQUAD,
  INTERPOLATOR_TYPE_EASEOUTQUAD,
  INTERPOLATOR_TYPE_EASEINOUTQUAD,
  INTERPOLATOR_TYPE_EASEINCUBIC,
  INTERPOLATOR_TYPE_EASEOUTCUBIC,
  INTERPOLATOR_TYPE_EASEINOUTCUBIC,
  INTERPOLATOR_TYPE_EASEINQUART,
  INTERPOLATOR_TYPE_EASEINEXPO,
  INTERPOLATOR_TYPE_EASEOUTEXPO,
};

struct InterpolatorParams {
  float dest_value_;
  INTERPOLATOR_TYPE type_;
  double duration_;
};

/******************************************************************
 * Interpolates values with several interpolation methods
 */
class Interpolator {
 private:
  double start_time_;
  double dest_time_;
  INTERPOLATOR_TYPE type_;

  float start_value_;
  float dest_value_;
  std::list<InterpolatorParams> list_params_;

  float GetFormula(const INTERPOLATOR_TYPE type, const float t, const float b,
                   const float d, const float c);

 public:
  Interpolator();
  ~Interpolator();

  Interpolator& Set(const float start, const float dest,
                    const INTERPOLATOR_TYPE type, double duration);

  Interpolator& Add(const float dest, const INTERPOLATOR_TYPE type,
                    const double duration);

  bool Update(const double currentTime, float& p);

  void Clear();
};

}  // namespace ndk_helper
#endif /* INTERPOLATOR_H_ */
