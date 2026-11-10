/*
 * Copyright (C) 2010 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.example.nativeaudio;

import android.Manifest;
import android.annotation.TargetApi;
import android.app.Activity;
import android.content.Context;
import android.content.pm.PackageManager;
import android.content.res.AssetManager;
import android.media.AudioManager;
import android.os.Build;
import android.os.Bundle;
import androidx.annotation.NonNull;
import androidx.core.app.ActivityCompat;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.SeekBar;
import android.widget.SeekBar.OnSeekBarChangeListener;
import android.widget.Spinner;
import android.widget.Toast;

import com.example.nativeaudio.databinding.MainBinding;

public class NativeAudio extends Activity
        implements ActivityCompat.OnRequestPermissionsResultCallback {


    //static final String TAG = "NativeAudio";
    private static final int AUDIO_ECHO_REQUEST = 0;

    static final int CLIP_NONE = 0;
    static final int CLIP_HELLO = 1;
    static final int CLIP_ANDROID = 2;
    static final int CLIP_SAWTOOTH = 3;
    static final int CLIP_PLAYBACK = 4;

    static String URI;
    static AssetManager assetManager;

    static boolean isPlayingAsset = false;
    static boolean isPlayingUri = false;

    static int numChannelsUri = 0;

    /** Called when the activity is first created. */
    @Override
    @TargetApi(17)
    protected void onCreate(Bundle icicle) {
        super.onCreate(icicle);
        MainBinding binding = MainBinding.inflate(getLayoutInflater());
        setContentView(binding.getRoot());

        assetManager = getAssets();

        // initialize native audio system
        createEngine();

        int sampleRate = 0;
        int bufSize = 0;
        /*
         * retrieve fast audio path sample rate and buf size; if we have it, we pass to native
         * side to create a player with fast audio enabled [ fast audio == low latency audio ];
         * IF we do not have a fast audio path, we pass 0 for sampleRate, which will force native
         * side to pick up the 8Khz sample rate.
         */
        if(Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR1) {
            AudioManager myAudioMgr = (AudioManager) getSystemService(Context.AUDIO_SERVICE);
            String nativeParam = myAudioMgr.getProperty(AudioManager.PROPERTY_OUTPUT_SAMPLE_RATE);
            sampleRate = Integer.parseInt(nativeParam);
            nativeParam = myAudioMgr.getProperty(AudioManager.PROPERTY_OUTPUT_FRAMES_PER_BUFFER);
            bufSize = Integer.parseInt(nativeParam);
        }
        createBufferQueueAudioPlayer(sampleRate, bufSize);

        // initialize URI spinner
        Spinner uriSpinner = binding.uriSpinner;
        ArrayAdapter<CharSequence> uriAdapter = ArrayAdapter.createFromResource(
                this, R.array.uri_spinner_array, android.R.layout.simple_spinner_item);
        uriAdapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        uriSpinner.setAdapter(uriAdapter);
        uriSpinner.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {

            public void onItemSelected(AdapterView<?> parent, View view, int pos, long id) {
                URI = parent.getItemAtPosition(pos).toString();
            }

            public void onNothingSelected(AdapterView parent) {
                URI = null;
            }

        });

        // initialize button click handlers

        binding.hello.setOnClickListener(new OnClickListener() {
            public void onClick(View view) {
                // ignore the return value
                selectClip(CLIP_HELLO, 5);
            }
        });

        binding.android.setOnClickListener(new OnClickListener() {
            public void onClick(View view) {
                // ignore the return value
                selectClip(CLIP_ANDROID, 7);
            }
        });

        binding.sawtooth.setOnClickListener(new OnClickListener() {
            public void onClick(View view) {
                // ignore the return value
                selectClip(CLIP_SAWTOOTH, 1);
            }
        });

        binding.reverb.setOnClickListener(new OnClickListener() {
            boolean enabled = false;
            public void onClick(View view) {
                enabled = !enabled;
                if (!enableReverb(enabled)) {
                    enabled = !enabled;
                }
            }
        });

        binding.embeddedSoundtrack.setOnClickListener(new OnClickListener() {
            boolean created = false;
            public void onClick(View view) {
                if (!created) {
                    created = createAssetAudioPlayer(assetManager, "background.mp3");
                }
                if (created) {
                    isPlayingAsset = !isPlayingAsset;
                    setPlayingAssetAudioPlayer(isPlayingAsset);
                }
            }
        });

        // native uriPlayer is broken in android 21 and over, internal bug id: b/29321867
        // will re-open after it is fixed in later OSes
        binding.uriSoundtrack.setOnClickListener(new OnClickListener() {
            boolean created = false;
            public void onClick(View view) {
                if (!created && URI != null) {
                    created = createUriAudioPlayer(URI);
                }
            }
        });

        binding.pauseUri.setOnClickListener(new OnClickListener() {
            public void onClick(View view) {
                setPlayingUriAudioPlayer(false);
            }
        });

        binding.playUri.setOnClickListener(new OnClickListener() {
            public void onClick(View view) {
                setPlayingUriAudioPlayer(true);
            }
        });

        binding.loopUri.setOnClickListener(new OnClickListener() {
            boolean isLooping = false;
            public void onClick(View view) {
                isLooping = !isLooping;
                setLoopingUriAudioPlayer(isLooping);
            }
        });

        binding.muteLeftUri.setOnClickListener(new OnClickListener() {
            boolean muted = false;
            public void onClick(View view) {
                muted = !muted;
                setChannelMuteUriAudioPlayer(0, muted);
            }
        });

        binding.muteRightUri.setOnClickListener(new OnClickListener() {
            boolean muted = false;
            public void onClick(View view) {
                muted = !muted;
                setChannelMuteUriAudioPlayer(1, muted);
            }
        });

        binding.soloLeftUri.setOnClickListener(new OnClickListener() {
            boolean soloed = false;
            public void onClick(View view) {
                soloed = !soloed;
                setChannelSoloUriAudioPlayer(0, soloed);
            }
        });

        binding.soloRightUri.setOnClickListener(new OnClickListener() {
            boolean soloed = false;
            public void onClick(View view) {
                soloed = !soloed;
                setChannelSoloUriAudioPlayer(1, soloed);
            }
        });

        binding.muteUri.setOnClickListener(new OnClickListener() {
            boolean muted = false;
            public void onClick(View view) {
                muted = !muted;
                setMuteUriAudioPlayer(muted);
            }
        });

        binding.enableStereoPositionUri.setOnClickListener(
                new OnClickListener() {
                    boolean enabled = false;
                    public void onClick(View view) {
                        enabled = !enabled;
                        enableStereoPositionUriAudioPlayer(enabled);
                    }
                });

        binding.channelsUri.setOnClickListener(new OnClickListener() {
            public void onClick(View view) {
                if (numChannelsUri == 0) {
                    numChannelsUri = getNumChannelsUriAudioPlayer();
                }
                Toast.makeText(NativeAudio.this, "Channels: " + numChannelsUri,
                        Toast.LENGTH_SHORT).show();
            }
        });

        binding.volumeUri.setOnSeekBarChangeListener(
                new OnSeekBarChangeListener() {
                    int lastProgress = 100;
                    public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                        if (BuildConfig.DEBUG && !(progress >= 0 && progress <= 100)) {
                            throw new AssertionError();
                        }
                        lastProgress = progress;
                    }
                    public void onStartTrackingTouch(SeekBar seekBar) {
                    }
                    public void onStopTrackingTouch(SeekBar seekBar) {
                        int attenuation = 100 - lastProgress;
                        int millibel = attenuation * -50;
                        setVolumeUriAudioPlayer(millibel);
                    }
                });

        binding.panUri.setOnSeekBarChangeListener(
                new OnSeekBarChangeListener() {
                    int lastProgress = 100;
                    public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                        if (BuildConfig.DEBUG && !(progress >= 0 && progress <= 100)) {
                            throw new AssertionError();
                        }
                        lastProgress = progress;
                    }
                    public void onStartTrackingTouch(SeekBar seekBar) {
                    }
                    public void onStopTrackingTouch(SeekBar seekBar) {
                        int permille = (lastProgress - 50) * 20;
                        setStereoPositionUriAudioPlayer(permille);
                    }
                });
        if (Build.VERSION.SDK_INT > 19) {
            View[] uriViews = {
                    binding.uriSoundtrack, binding.pauseUri,
                    binding.playUri, binding.loopUri,
                    binding.muteLeftUri, binding.muteRightUri,
                    binding.soloLeftUri, binding.soloRightUri,
                    binding.muteUri, binding.enableStereoPositionUri,
                    binding.channelsUri, binding.volumeUri,
                    binding.panUri, binding.uriSpinner,
            };
            for (View view : uriViews) {
                view.setEnabled(false);
            }
        }

        binding.record.setOnClickListener(new OnClickListener() {
            public void onClick(View view) {
                int status = ActivityCompat.checkSelfPermission(NativeAudio.this,
                        Manifest.permission.RECORD_AUDIO);
                if (status != PackageManager.PERMISSION_GRANTED) {
                    ActivityCompat.requestPermissions(
                            NativeAudio.this,
                            new String[]{Manifest.permission.RECORD_AUDIO},
                            AUDIO_ECHO_REQUEST);
                    return;
                }
                recordAudio();
            }
        });

        binding.playback.setOnClickListener(new OnClickListener() {
            public void onClick(View view) {
                // ignore the return value
                selectClip(CLIP_PLAYBACK, 3);
            }
        });

    }

    // Single out recording for run-permission needs
    static boolean created = false;
    private void recordAudio() {
        if (!created) {
            created = createAudioRecorder();
        }
        if (created) {
            startRecording();
        }
    }

   /** Called when the activity is about to be destroyed. */
    @Override
    protected void onPause()
    {
        // turn off all audio
        selectClip(CLIP_NONE, 0);
        isPlayingAsset = false;
        setPlayingAssetAudioPlayer(false);
        isPlayingUri = false;
        setPlayingUriAudioPlayer(false);
        super.onPause();
    }

    /** Called when the activity is about to be destroyed. */
    @Override
    protected void onDestroy()
    {
        shutdown();
        super.onDestroy();
    }
    @Override
    public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions,
                                           @NonNull int[] grantResults) {
        /*
         * if any permission failed, the sample could not play
         */
        if (AUDIO_ECHO_REQUEST != requestCode) {
            super.onRequestPermissionsResult(requestCode, permissions, grantResults);
            return;
        }

        if (grantResults.length != 1  ||
                grantResults[0] != PackageManager.PERMISSION_GRANTED) {
            /*
             * When user denied the permission, throw a Toast to prompt that RECORD_AUDIO
             * is necessary; on UI, we display the current status as permission was denied so
             * user know what is going on.
             * This application go back to the original state: it behaves as if the button
             * was not clicked. The assumption is that user will re-click the "start" button
             * (to retry), or shutdown the app in normal way.
             */
            Toast.makeText(getApplicationContext(),
                    getString(R.string.NeedRecordAudioPermission),
                    Toast.LENGTH_SHORT)
                    .show();
            return;
        }

        // The callback runs on app's thread, so we are safe to resume the action
        recordAudio();
    }

    /** Native methods, implemented in jni folder */
    public static native void createEngine();
    public static native void createBufferQueueAudioPlayer(int sampleRate, int samplesPerBuf);
    public static native boolean createAssetAudioPlayer(AssetManager assetManager, String filename);
    // true == PLAYING, false == PAUSED
    public static native void setPlayingAssetAudioPlayer(boolean isPlaying);
    public static native boolean createUriAudioPlayer(String uri);
    public static native void setPlayingUriAudioPlayer(boolean isPlaying);
    public static native void setLoopingUriAudioPlayer(boolean isLooping);
    public static native void setChannelMuteUriAudioPlayer(int chan, boolean mute);
    public static native void setChannelSoloUriAudioPlayer(int chan, boolean solo);
    public static native int getNumChannelsUriAudioPlayer();
    public static native void setVolumeUriAudioPlayer(int millibel);
    public static native void setMuteUriAudioPlayer(boolean mute);
    public static native void enableStereoPositionUriAudioPlayer(boolean enable);
    public static native void setStereoPositionUriAudioPlayer(int permille);
    public static native boolean selectClip(int which, int count);
    public static native boolean enableReverb(boolean enabled);
    public static native boolean createAudioRecorder();
    public static native void startRecording();
    public static native void shutdown();

    /** Load jni .so on initialization */
    static {
         System.loadLibrary("native-audio-jni");
    }

}
