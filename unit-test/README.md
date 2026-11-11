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
payload-boundary and fatal-prefix cases, and verifies that the public
printf-style logging macros restore `errno`.

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
