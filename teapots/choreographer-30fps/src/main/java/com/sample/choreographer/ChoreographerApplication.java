// Copyright 2013 The Android Open Source Project
// SPDX-License-Identifier: Apache-2.0

package com.sample.choreographer;

import javax.microedition.khronos.opengles.GL10;

import android.app.Application;
import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Matrix;
import android.opengl.GLUtils;
import android.util.Log;
import android.widget.Toast;

public class ChoreographerApplication extends Application {
    public void onCreate(){
        super.onCreate();	
        Log.w("native-activity", "onCreate");

        final PackageManager pm = getApplicationContext().getPackageManager();
        ApplicationInfo ai;
        try {
            ai = pm.getApplicationInfo( this.getPackageName(), 0);
        } catch (final NameNotFoundException e) {
            ai = null;
        }
        final String applicationName = (String) (ai != null ? pm.getApplicationLabel(ai) : "(unknown)");
        Toast.makeText(this, applicationName, Toast.LENGTH_SHORT).show();
    }
}
