# Copyright(C) 2020 The Android Open Source Project
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.22.1)
project(ImageDecoderNativeActivity LANGUAGES C CXX)

include(AppLibrary)
find_package(base CONFIG REQUIRED)

get_filename_component(commonDir ${CMAKE_CURRENT_SOURCE_DIR}/../../../../common ABSOLUTE)

# build the ndk-helper library
get_filename_component(ndkHelperSrc ${commonDir}/ndk_helper ABSOLUTE)
add_subdirectory(${ndkHelperSrc}
    ndkHelperBin/${CMAKE_BUILD_TYPE}/${ANDROID_ABI})

# now build app's shared lib
add_app_library(${PROJECT_NAME}
    SHARED
    TeapotNativeActivity.cpp
    TeapotRenderer.cpp
    ImageDecoderRender.cpp
    Texture.cpp
)
target_include_directories(${PROJECT_NAME} PRIVATE
    ${commonDir}
    ${CMAKE_CURRENT_SOURCE_DIR}/image_wrapper)

target_compile_options(${PROJECT_NAME} PRIVATE -Wno-unused-function)

# Export ANativeActivity_onCreate(),
# Refer to: https://github.com/android-ndk/ndk/issues/381.
set_target_properties(${PROJECT_NAME}
    PROPERTIES LINK_FLAGS "-u ANativeActivity_onCreate")

# add lib dependencies
target_link_libraries(${PROJECT_NAME} PRIVATE NdkHelper jnigraphics)
