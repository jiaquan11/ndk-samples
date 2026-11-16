// Copyright (C) Google Inc.
// SPDX-License-Identifier: Apache-2.0
#ifndef endlesstunnel_joystick_support_hpp
#define endlesstunnel_joystick_support_hpp

// A few declarations we need for NDK joystick support as of NDK release 9
typedef float (*_getAxisValue_sig)(const AInputEvent*, int32_t, size_t);
static const int AXIS_HAT_X = 15, AXIS_HAT_Y = 16;
static const int SOURCE_CLASS_JOYSTICK = 16;
static const int KEYCODE_BUTTON_X = 99;
static const int KEYCODE_BUTTON_Y = 100;
static const int KEYCODE_BUTTON_A = 96;
static const int KEYCODE_BUTTON_B = 97;

// source type for "touch navigation" devices (introduced in API 18)
static const int SOURCE_TOUCH_NAVIGATION = 0x00200000;

#endif
