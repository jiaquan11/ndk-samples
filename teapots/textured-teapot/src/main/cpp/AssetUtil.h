// Copyright (C) 2018 The Android Open Source Project
// SPDX-License-Identifier: Apache-2.0
#ifndef __ASSET__UTIL_H__
#define __ASSET__UTIL_H__

#include <android/asset_manager.h>

#include <string>
#include <vector>

bool AssetEnumerateFileType(AAssetManager* assetManager, const char* type,
                            std::vector<std::string>& files);
bool AssetReadFile(AAssetManager* assetManager, std::string& name,
                   std::vector<uint8_t>& buf);

#endif  // __ASSET__UTIL_H__
