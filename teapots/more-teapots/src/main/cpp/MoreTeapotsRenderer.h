// Copyright 2013 The Android Open Source Project
// SPDX-License-Identifier: Apache-2.0

//--------------------------------------------------------------------------------
// MoreTeapotsRenderer.h
// Renderer for teapots
//--------------------------------------------------------------------------------
#ifndef _MoreTeapotsRenderer_H
#define _MoreTeapotsRenderer_H

//--------------------------------------------------------------------------------
// Include files
//--------------------------------------------------------------------------------
#include <EGL/egl.h>
#include <GLES/gl.h>
#include <android/native_window_jni.h>
#include <android/sensor.h>
#include <android_native_app_glue.h>
#include <errno.h>
#include <jni.h>

#include <random>
#include <vector>

#define CLASS_NAME "android/app/NativeActivity"
#define APPLICATION_CLASS_NAME "com/sample/moreteapots/MoreTeapotsApplication"

#include "NDKHelper.h"

struct TEAPOT_VERTEX {
  float pos[3];
  float normal[3];
};

enum SHADER_ATTRIBUTES {
  ATTRIB_VERTEX,
  ATTRIB_NORMAL,
  ATTRIB_COLOR,
  ATTRIB_UV
};

struct SHADER_PARAMS {
  GLuint program_;
  GLuint light0_;
  GLuint material_diffuse_;
  GLuint material_ambient_;
  GLuint material_specular_;

  GLuint matrix_projection_;
  GLuint matrix_view_;
};

struct TEAPOT_MATERIALS {
  float specular_color[4];
  float ambient_color[3];
};

class MoreTeapotsRenderer {
  int32_t num_indices_;
  int32_t num_vertices_;
  GLuint ibo_;
  GLuint vbo_;
  GLuint ubo_;

  SHADER_PARAMS shader_param_;
  bool LoadShaders(SHADER_PARAMS* params, const char* strVsh,
                   const char* strFsh);
  bool LoadShadersES3(SHADER_PARAMS* params, const char* strVsh,
                      const char* strFsh,
                      std::map<std::string, std::string>& shaderParameters);

  ndk_helper::Mat4 mat_projection_;
  ndk_helper::Mat4 mat_view_;
  std::vector<ndk_helper::Mat4> vec_mat_models_;
  std::vector<ndk_helper::Vec3> vec_colors_;
  std::vector<ndk_helper::Vec2> vec_rotations_;
  std::vector<ndk_helper::Vec2> vec_current_rotations_;

  ndk_helper::TapCamera* camera_;

  int32_t teapot_x_;
  int32_t teapot_y_;
  int32_t teapot_z_;
  int32_t ubo_matrix_stride_;
  int32_t ubo_vector_stride_;
  bool geometry_instancing_support_;
  bool arb_support_;

  std::string ToString(const int32_t i);

 public:
  MoreTeapotsRenderer();
  virtual ~MoreTeapotsRenderer();
  void Init(const int32_t numX, const int32_t numY, const int32_t numZ);
  void Render();
  void Update(float dTime);
  bool Bind(ndk_helper::TapCamera* camera);
  void Unload();
  void UpdateViewport();
};

#endif
