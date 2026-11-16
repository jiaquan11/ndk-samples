# Copyright (C)  2017 The Android Open Source Project
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.22.1)
project(CameraBasic LANGUAGES C CXX)

include(AppLibrary)
include(AndroidNdkModules)
find_package(base REQUIRED CONFIG)
find_package(camera-utils REQUIRED CONFIG)

android_ndk_import_module_native_app_glue()

add_app_library(ndk_camera SHARED
    ${CMAKE_CURRENT_SOURCE_DIR}/android_main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/camera_engine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/camera_manager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/camera_listeners.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/image_reader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/camera_ui.cpp
)

target_link_libraries(ndk_camera
    PRIVATE
    base::base
    camera-utils::camera-utils
    android
    m
    $<LINK_LIBRARY:WHOLE_ARCHIVE,native_app_glue>
    camera2ndk
    mediandk
)
