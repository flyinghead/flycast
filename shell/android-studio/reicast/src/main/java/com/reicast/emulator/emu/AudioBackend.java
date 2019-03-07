package com.reicast.emulator.emu;

import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioTrack;
import android.os.Build;
import android.util.Log;

import com.reicast.emulator.Emulator;

import static android.media.AudioTrack.STATE_INITIALIZED;

public final class AudioBackend {
    static { System.loadLibrary("dc"); }

    private AudioTrack audioTrack;
    private long writePosition;
    private long size;	//size in frames
    private static AudioBackend instance;

    public AudioBackend() {
        setInstance(this);
        instance = this;
        enableSound(!Emulator.nosound);
    }

    public static AudioBackend getInstance() {
        return instance;
    }
    public void release() {
        setInstance(null);
        instance = null;
    }

    public void enableSound(boolean enable) {
        if (enable && audioTrack == null) {
            int min = AudioTrack.getMinBufferSize(44100, AudioFormat.CHANNEL_OUT_STEREO, AudioFormat.ENCODING_PCM_16BIT);

            if (2048 > min)
                min = 2048;

            audioTrack = new AudioTrack(
                    AudioManager.STREAM_MUSIC,
                    44100,
                    AudioFormat.CHANNEL_OUT_STEREO,
                    AudioFormat.ENCODING_PCM_16BIT,
                    min,
                    AudioTrack.MODE_STREAM
            );
            if (audioTrack.getState() != STATE_INITIALIZED) {
                audioTrack = null;
                release();
                Log.e("reicast", "Error initializing AudioTrack. Disabling sound");
            }
            else {
                size = min / 4;
                writePosition = 0;

                Log.i("audcfg", "Audio streaming: buffer size " + min + " samples / " + min / 44100.0 + " ms");
                audioTrack.play();
            }
        }
        else if (!enable && audioTrack != null) {
            audioTrack.pause();
            audioTrack.flush();
            audioTrack.release();
            audioTrack = null;
        }
    }

    // Called by native code
    private int writeBuffer(short[] samples, boolean wait)
    {
        if (audioTrack != null) {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
                audioTrack.write(samples, 0, samples.length, wait ? AudioTrack.WRITE_BLOCKING : AudioTrack.WRITE_NON_BLOCKING);
            } else {
                if (!wait)
                {
                    int newdata = samples.length / 2;

                    //user bytes = write-read
                    //available = size - (write - play)
                    long used = writePosition - audioTrack.getPlaybackHeadPosition();
                    long avail = size - used;

                    //Log.i("audcfg", "u: " + used + " a: " + avail);
                    if (avail < newdata)
                        return 0;

                    writePosition += newdata;
                }

                audioTrack.write(samples, 0, samples.length);
            }
        }

        return 1;
    }

    private static native void setInstance(AudioBackend backend);
}
