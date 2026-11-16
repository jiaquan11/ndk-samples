// Copyright (C) Google Inc.
// SPDX-License-Identifier: Apache-2.0
#ifndef endlesstunnel_texture_hpp
#define endlesstunnel_texture_hpp

#include "common.hpp"

/* Represents an OpenGL texture */
class Texture {
 private:
  GLuint mTextureH;

 public:
  inline Texture() { mTextureH = 0; }

  // Initialize from raw RGB data. If hasAlpha is true, then it's 4 bytes per
  // pixel (RGBA), otherwise it's interpreted as 3 bytes per pixel (RGB).
  void InitFromRawRGB(int width, int height, bool hasAlpha,
                      const unsigned char *data);
  void Bind(int unit);
  void Unbind();
};

#endif
