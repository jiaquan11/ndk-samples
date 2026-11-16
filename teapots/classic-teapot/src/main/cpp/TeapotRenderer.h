// Copyright 2013 The Android Open Source Project
// SPDX-License-Identifier: Apache-2.0

//--------------------------------------------------------------------------------
// Teapot Renderer.h
// Renderer for teapots
//--------------------------------------------------------------------------------
#ifndef _TEAPOTRENDERER_H
#define _TEAPOTRENDERER_H

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

#include <vector>

#define CLASS_NAME "android/app/NativeActivity"
#define APPLICATION_CLASS_NAME "com/sample/teapot/TeapotApplication"

#include "NDKHelper.h"

struct TEAPOT_VERTEX {
  float pos[3];
  float normal[3];
};

enum SHADER_ATTRIBUTES {
  ATTRIB_VERTEX,
  ATTRIB_NORMAL,
  ATTRIB_UV,
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
  float diffuse_color[3];
  float specular_color[4];
  float ambient_color[3];
};

class TeapotRenderer {
  int32_t num_indices_;
  int32_t num_vertices_;
  GLuint ibo_;
  GLuint vbo_;

  SHADER_PARAMS shader_param_;
  bool LoadShaders(SHADER_PARAMS* params, const char* strVsh,
                   const char* strFsh);

  ndk_helper::Mat4 mat_projection_;
  ndk_helper::Mat4 mat_view_;
  ndk_helper::Mat4 mat_model_;

  ndk_helper::TapCamera* camera_;

 public:
  TeapotRenderer();
  virtual ~TeapotRenderer();
  void Init();
  void Render();
  void Update(float dTime);
  bool Bind(ndk_helper::TapCamera* camera);
  void Unload();
  void UpdateViewport();
};

#endif
