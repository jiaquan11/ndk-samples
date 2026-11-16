// Copyright (C) 2018 The Android Open Source Project
// SPDX-License-Identifier: Apache-2.0
#include "AssetUtil.h"

#include <algorithm>
#include <cmath>

#define LOG_TAG "TeapotTexture.Asset"
#include <base/logging.h>

#define IS_LOW_CHAR(c) ((c) >= 'a' && (c) <= 'z')
#define TO_UPPER_CHAR(c) (c + 'A' - 'a')
void StringToUpper(std::string& str) {
  for (auto& ch : str) {
    if (IS_LOW_CHAR(ch)) ch = TO_UPPER_CHAR(ch);
  }
}
bool AssetEnumerateFileType(AAssetManager* assetManager, const char* type,
                            std::vector<std::string>& files) {
  if (!assetManager || !type || !*type) return false;

  std::string fileType(type);
  if (fileType[0] != '.') {
    fileType.insert(0, 1, '.');
  }
  StringToUpper(fileType);

  AAssetDir* dir = AAssetManager_openDir(assetManager, "");

  const char* name;
  while ((name = AAssetDir_getNextFileName(dir))) {
    std::string assetName(name);
    if (assetName.length() <= fileType.length()) continue;
    std::string assetType = assetName.substr(
        assetName.length() - fileType.length(), fileType.length());
    StringToUpper(assetType);

    if (assetType == fileType) {
      files.push_back(assetName);
    }
  }
  LOGI("Found %d PNG Files", static_cast<uint32_t>(files.size()));

  AAssetDir_close(dir);
  return true;
}

bool AssetReadFile(AAssetManager* assetManager, std::string& assetName,
                   std::vector<uint8_t>& buf) {
  if (!assetName.length()) return false;
  AAsset* assetDescriptor =
      AAssetManager_open(assetManager, assetName.c_str(), AASSET_MODE_BUFFER);
  CHECKF(assetDescriptor, "%s does not exist in %s", assetName.c_str(),
         __FUNCTION__);
  size_t remaining = AAsset_getLength(assetDescriptor);

  buf.resize(remaining);
  size_t idx = 0;

  while (remaining > 0) {
    int readSize = AAsset_read(assetDescriptor, &buf[idx], remaining);
    if (readSize < 0) {
      LOGE("AAsset_read of %s failed", assetName.c_str());
      return false;
    }
    idx += readSize;
    remaining -= readSize;
  }

  AAsset_close(assetDescriptor);
  return true;
}
