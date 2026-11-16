// Copyright 2018 The Android Open Source Project
// SPDX-License-Identifier: Apache-2.0

#ifndef TEAPOTS_TEXTUREDTEAPOTRENDER_H
#define TEAPOTS_TEXTUREDTEAPOTRENDER_H
#include "TeapotRenderer.h"
#include "Texture.h"
/**
 *  class TextureTeapotRender
 *    adding texture into teapot
 *     - create texture buffer and load image in assets/Textures
 *     - enable texture units
 *     - enable texturing inside shaders
 */
class TexturedTeapotRender : public TeapotRenderer {
  GLuint texVbo_ = GL_INVALID_VALUE;
  Texture* texObj_ = nullptr;

 public:
  TexturedTeapotRender();
  virtual ~TexturedTeapotRender();
  virtual void Init(AAssetManager* amgr);
  virtual void Render();
  virtual void Unload();
};

#endif  // TEAPOTS_TEXTUREDTEAPOTRENDER_H
