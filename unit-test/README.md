# Unit Test

This Android sample shows how to write unit tests for native code with
[googletest](https://github.com/google/googletest), and run them on-device. Its
test library also hosts the internal `base` logging tests so those tests reuse
the same device runner without publishing private implementation headers.

## Writing unit tests

This app has a very simple library containing a function to add two numbers.

[adder.h](app/src/main/cpp/adder.h):

```C++
#pragma once

int add(int a, int b);
```

[adder.cpp](app/src/main/cpp/adder.cpp):

```C++
#include "adder.h"

int add(int a, int b) { return a + b; }
```

[adder_test.cpp](app/src/main/cpp/adder_test.cpp) contains a unit test for this
function:

```C++
#include "adder.h"

#include "gtest/gtest.h"

TEST(adder, adder) { EXPECT_EQ(3, add(1, 2)); }
```

[`base_logging_test.cpp`](../base/src/test/cpp/base_logging_test.cpp) covers the
logging splitter's empty message, leading and repeated empty lines, multiline,
payload-boundary and source-prefix cases. It also covers lossless oversized-line
splitting, UTF-8 boundaries, oversized Tag and source fallbacks, minimum-severity
short-circuiting, temporary explicit Tags, long formatting and `errno`
restoration.

## Building and running the tests

We need to add a library for the test to
[CMakeLists.txt](app/src/main/cpp/CMakeLists.txt), which depends on googletest
and junit-gtest:

```cmake
find_package(base REQUIRED CONFIG)
find_package(googletest REQUIRED CONFIG)
find_package(junit-gtest REQUIRED CONFIG)
```

```cmake
get_filename_component(NDK_SAMPLES_ROOT
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../../.." ABSOLUTE)
set(BASE_CPP_DIR "${NDK_SAMPLES_ROOT}/base/src/main/cpp")
set(BASE_TEST_DIR "${NDK_SAMPLES_ROOT}/base/src/test/cpp")

add_app_library(app_tests NO_VERSION_SCRIPT SHARED
    adder_test.cpp
    "${BASE_TEST_DIR}/base_logging_test.cpp"
)
target_include_directories(app_tests PRIVATE "${BASE_CPP_DIR}")
target_link_libraries(app_tests
    PRIVATE
    base::base
    $<TARGET_OBJECTS:adder>
    googletest::gtest
    junit-gtest::junit-gtest
)
```

We need to add `base`, googletest and junit-gtest as dependencies in
[app/build.gradle](app/build.gradle). All three expose native CMake packages, so
we enable Prefab consumption.

```groovy
android {
    buildFeatures {
        prefab true
    }

    packagingOptions {
        jniLibs {
            // Keep the native test runner out of the application APK.
            testOnly += ["**/libapp_tests.so"]
        }
    }
}
```

```groovy
dependencies {
    implementation project(":base")
    implementation libs.androidx.junit.gtest
    implementation libs.googletest
}
```

Dependency versions are managed in the root
[`libs.versions.toml`](../gradle/libs.versions.toml).

The `testOnly` rule packages `libapp_tests.so` only in the test APK. The
application's `libunittest.so` remains independent of `base`; only the test
target links `base::base`.

## Base logging benchmark

同一个 CMake 构建还提供独立的 `base_logging_benchmark` 设备端可执行程序。
它复用 `base::base` 的 Prefab 消费链，但不打入 APK，也不与 GTest 正确性
测试混合。完整方法、指标边界和结果入口见
[`base/src/benchmark/README.md`](../base/src/benchmark/README.md)。

连接一台 arm64 设备后，可从仓库根目录运行：

```powershell
.\base\src\benchmark\run_benchmark.ps1
```

真实 `liblog` 突发写入是可选层，需要显式增加 `-IncludeLogd`；默认只测量
过滤、格式化和分片等库内路径。

Finally, we need a wrapper in androidTest,
[NativeTests.kt](app/src/androidTest/java/com/example/unittest/NativeTests.kt):

```kotlin
package com.example.unittest

import androidx.test.ext.junitgtest.GtestRunner
import androidx.test.ext.junitgtest.TargetLibrary
import org.junit.runner.RunWith

@RunWith(GtestRunner::class)
@TargetLibrary(libraryName = "app_tests")
class NativeTests
```

You can run the test on a phone or emulator as described in the
[Android developer documentation](https://developer.android.com/studio/test/test-in-android-studio).

Try deliberately breaking the test, like this:

```C++
EXPECT_EQ(4, add(1, 2));
```

You should see a failure message like this:

```
java.lang.AssertionError:
/path/to/ndk-samples/unit-test/app/src/main/cpp/adder_test.cpp:6
Expected equality of these values:
4
add(1,2)
Which is: 3
```

## Screenshot

![screenshot](screenshot.png)
