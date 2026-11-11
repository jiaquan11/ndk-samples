#pragma once

// Include this helper before any public header that may include logging.h.
// A consumer-defined tag takes precedence over the Camera sample default.
#ifndef LOG_TAG
#define LOG_TAG "CAMERA-SAMPLE"
#endif
#include <base/logging.h>
