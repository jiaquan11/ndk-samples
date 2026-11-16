// Copyright 2018 The Android Open Source Project
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <EGL/egl.h>
#include <GLES/gl.h>
#include <android/asset_manager.h>

#include <string>
#include <vector>

/**
 *  class Texture
 *    adding texture into teapot
 *     - oad image in assets/Textures
 *     - enable texture units
 *     - report samplers needed inside shader
 *  Functionality wise:
 *     - one texture
 *     - one sampler
 *     - texture unit 0, sampler unit 0
 */
class Texture {
 public:
  /**
   * Create a texture object
   *
   * @param asset_paths holds image file names under APK/assets.
   *     cube map needs 6 (direction of +x, -x, +y, -y, +z, -z)
   * @param asset_manager Java side asset_manager object
   * @return newly created texture object, or nullptr in case of errors
   */
  Texture(std::vector<std::string>& asset_paths, AAssetManager* asset_manager);
  Texture(const Texture&) = delete;
  ~Texture();

  Texture& operator=(const Texture&) = delete;

  void GetActiveSamplerInfo(std::vector<std::string>& names,
                            std::vector<GLint>& units);
  bool Activate(void);

 private:
  GLuint texId_;
};
