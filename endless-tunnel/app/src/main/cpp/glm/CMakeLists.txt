# Copyright (C) The Android Open Source Project
# SPDX-License-Identifier: Apache-2.0

add_library(glm STATIC
    detail/glm.cpp)

target_include_directories(glm PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..)
