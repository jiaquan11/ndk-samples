// Copyright (C) 2016 The Android Open Source Project
// SPDX-License-Identifier: Apache-2.0
package com.example.hellojni

import android.os.Bundle
import androidx.appcompat.app.AppCompatActivity
import com.example.hellojni.databinding.ActivityHelloJniBinding

class HelloJni : AppCompatActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        /*
         * Retrieve our TextView and set its content.
         * the text is retrieved by calling a native
         * function.
         */
        val binding = ActivityHelloJniBinding.inflate(layoutInflater)
        setContentView(binding.root)
        binding.helloTextview.text = stringFromJNI()
    }

    /*
     * A native method that is implemented by the
     * 'hello-jni' native library, which is packaged
     * with this application.
     */
    external fun stringFromJNI(): String?

    /*
     * This is another native method declaration that is *not*
     * implemented by 'hello-jni'. This is simply to show that
     * you can declare as many native methods in your Java code
     * as you want, their implementation is searched in the
     * currently loaded native libraries only the first time
     * you call them.
     *
     * Trying to call this function will result in a
     * java.lang.UnsatisfiedLinkError exception !
     */
    external fun unimplementedStringFromJNI(): String?

    companion object {
    /*
     * this is used to load the 'hello-jni' library on application
     * startup. The library has already been unpacked into
     * /data/data/com.example.hellojni/lib/libhello-jni.so
     * at the installation time by the package manager.
     */
        init {
            System.loadLibrary("hello-jni")
        }
    }
}

