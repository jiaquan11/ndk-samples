cmake_minimum_required(VERSION 3.4.1)

#增加一个新的动态库so
add_library(hello-jni SHARED
            hello-jni.c)

#链接所有的库
# Include libraries needed for hello-jni lib
target_link_libraries(hello-jni
                      android
                      log)
