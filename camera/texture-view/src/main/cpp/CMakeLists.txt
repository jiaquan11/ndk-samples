# Copyright (C)  2017 The Android Open Source Project
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.22.1)
project(CameraTextureView LANGUAGES CXX)

include(AppLibrary)
find_package(base REQUIRED CONFIG)
find_package(camera-utils REQUIRED CONFIG)

add_app_library(camera_textureview SHARED
    android_main.cpp
    camera_manager.cpp
    camera_engine.cpp
    camera_listeners.cpp
)

target_link_libraries(camera_textureview
    PRIVATE
    base::base
    camera-utils::camera-utils
    dl
    android
    m
    camera2ndk
)
