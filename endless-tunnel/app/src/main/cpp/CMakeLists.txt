# Copyright (C) The Android Open Source Project
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.22.1)
project(EndlessTunnel LANGUAGES C CXX)

include(AppLibrary)
include(AndroidNdkModules)
find_package(base REQUIRED CONFIG)

android_ndk_import_module_native_app_glue()

# Set common compiler options
add_definitions("-DGLM_FORCE_SIZE_T_LENGTH -DGLM_FORCE_RADIANS")

# Import the CMakeLists.txt for the glm library
add_subdirectory(glm)

# now build app's shared lib
add_app_library(game SHARED
    android_main.cpp
    anim.cpp
    ascii_to_geom.cpp
    dialog_scene.cpp
    indexbuf.cpp
    input_util.cpp
    jni_util.cpp
    native_engine.cpp
    obstacle.cpp
    obstacle_generator.cpp
    our_shader.cpp
    play_scene.cpp
    scene.cpp
    scene_manager.cpp
    sfxman.cpp
    shader.cpp
    shape_renderer.cpp
    tex_quad.cpp
    text_renderer.cpp
    texture.cpp
    ui_scene.cpp
    util.cpp
    vertexbuf.cpp
    welcome_scene.cpp
)

if(ANDROID_ABI STREQUAL riscv64)
    # This sample uses OpenSLES, which was deprecated in API 26. Our
    # minSdkVersion is 21, but we also build for riscv64, which isn't a
    # supported ABI yet and so that configuration is built for the latest API
    # level supported by the NDK.
    #
    # Longer term, this sample should migrate to Oboe.
    target_compile_options(game PRIVATE -Wno-deprecated-declarations)
endif()

target_include_directories(game PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/data
)

# add lib dependencies
target_link_libraries(game
    android
    $<LINK_LIBRARY:WHOLE_ARCHIVE,native_app_glue>
    atomic
    base::base
    EGL
    GLESv2
    glm
    OpenSLES
)
