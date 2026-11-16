// Copyright (C) 2007 The Android Open Source Project
// SPDX-License-Identifier: Apache-2.0

package com.android.accelerometergraph;

import android.app.Activity;
import android.opengl.GLSurfaceView;
import android.os.Bundle;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;


public class AccelerometerGraphActivity extends Activity {

    GLSurfaceView mView;

    @Override protected void onCreate(Bundle icicle) {
        super.onCreate(icicle);
        mView = new GLSurfaceView(getApplication());
        mView.setEGLContextClientVersion(2);
        mView.setRenderer(new GLSurfaceView.Renderer() {
            @Override
            public void onSurfaceCreated(GL10 gl, EGLConfig config) {
                AccelerometerGraphJNI.surfaceCreated();
            }

            @Override
            public void onSurfaceChanged(GL10 gl, int width, int height) {
                AccelerometerGraphJNI.surfaceChanged(width, height);
            }

            @Override
            public void onDrawFrame(GL10 gl) {
                AccelerometerGraphJNI.drawFrame();
            }
        });
        mView.queueEvent(new Runnable() {
            @Override
            public void run() {
                AccelerometerGraphJNI.init(getAssets());
            }
        });
	    setContentView(mView);
    }

    @Override protected void onPause() {
        super.onPause();
        mView.onPause();
        mView.queueEvent(new Runnable() {
            @Override
            public void run() {
                AccelerometerGraphJNI.pause();
            }
        });
    }

    @Override protected void onResume() {
        super.onResume();
        mView.onResume();
        mView.queueEvent(new Runnable() {
            @Override
            public void run() {
                AccelerometerGraphJNI.resume();
            }
        });
    }
}
