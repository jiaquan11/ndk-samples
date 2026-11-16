// Copyright 2020 The Android Open Source Project
// SPDX-License-Identifier: Apache-2.0

#ifndef TEAPOTS_IMAGEDECODERRENDER_H
#define TEAPOTS_IMAGEDECODERRENDER_H
#include "TeapotRenderer.h"
#include "Texture.h"
/**
 *  class TextureTeapotRender
 *    adding texture into teapot
 *     - create texture buffer and load image in assets/Textures
 *     - enable texture units
 *     - enable texturing inside shaders
 */
class ImageDecoderRender : public TeapotRenderer {
  GLuint texVbo_ = GL_INVALID_VALUE;
  Texture* texObj_ = nullptr;

 public:
  ImageDecoderRender();
  virtual ~ImageDecoderRender();
  // This is to decide which teapot type to render:
  //   plain teapot
  //   2D textured teapot
  //   Cubemap textured teapot
  // the rest of the code looks this function to decide
  // what to render.
  virtual GLint GetTextureType(void);
  virtual void Init(AAssetManager* amgr);
  virtual void Render();
  virtual void Unload();
};

#endif  // TEAPOTS_IMAGEDECODERRENDER_H
