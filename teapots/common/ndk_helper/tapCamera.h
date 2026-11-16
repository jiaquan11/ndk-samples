// Copyright 2013 The Android Open Source Project
// SPDX-License-Identifier: Apache-2.0

#pragma once
#include <GLES2/gl2.h>

#include <string>
#include <vector>

#include "JNIHelper.h"
#include "interpolator.h"
#include "vecmath.h"

namespace ndk_helper {

/******************************************************************
 * Camera control helper class with a tap gesture
 * This class is mainly used for 3D space camera control in samples.
 *
 */
class TapCamera {
 private:
  // Trackball
  Vec2 vec_ball_center_;
  float ball_radius_;
  Quaternion quat_ball_now_;
  Quaternion quat_ball_down_;
  Vec2 vec_ball_now_;
  Vec2 vec_ball_down_;
  Quaternion quat_ball_rot_;

  bool dragging_;
  bool pinching_;

  // Pinch related info
  Vec2 vec_pinch_start_;
  Vec2 vec_pinch_start_center_;
  float pinch_start_distance_SQ_;

  // Camera shift
  Vec3 vec_offset_;
  Vec3 vec_offset_now_;

  // Camera Rotation
  float camera_rotation_;
  float camera_rotation_start_;
  float camera_rotation_now_;

  // Momentum support
  bool momentum_;
  double time_stamp_;
  Vec2 vec_drag_delta_;
  Vec2 vec_last_input_;
  Vec3 vec_offset_last_;
  Vec3 vec_offset_delta_;
  float momemtum_steps_;

  Vec2 vec_flip_;
  float flip_z_;

  Mat4 mat_rotation_;
  Mat4 mat_transform_;

  Vec3 vec_pinch_transform_factor_;

  Vec3 PointOnSphere(Vec2& point);
  void BallUpdate();
  void InitParameters();

 public:
  TapCamera();
  virtual ~TapCamera();
  void BeginDrag(const Vec2& vec);
  void EndDrag();
  void Drag(const Vec2& vec);
  void Update();
  void Update(const double time);

  Mat4& GetRotationMatrix();
  Mat4& GetTransformMatrix();

  void BeginPinch(const Vec2& v1, const Vec2& v2);
  void EndPinch();
  void Pinch(const Vec2& v1, const Vec2& v2);

  void SetFlip(const float x, const float y, const float z) {
    vec_flip_ = Vec2(x, y);
    flip_z_ = z;
  }

  void SetPinchTransformFactor(const float x, const float y, const float z) {
    vec_pinch_transform_factor_ = Vec3(x, y, z);
  }

  void Reset(const bool bAnimate);
};

}  // namespace ndk_helper
