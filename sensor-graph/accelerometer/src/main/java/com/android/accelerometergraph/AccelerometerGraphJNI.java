// Copyright (C) 2007 The Android Open Source Project
// SPDX-License-Identifier: Apache-2.0

package com.android.accelerometergraph;

// Wrapper for native library

import android.content.res.AssetManager;

public class AccelerometerGraphJNI {

     static {
         System.loadLibrary("accelerometergraph");
     }

     public static native void init(AssetManager assetManager);
     public static native void surfaceCreated();
     public static native void surfaceChanged(int width, int height);
     public static native void drawFrame();
     public static native void pause();
     public static native void resume();
}
