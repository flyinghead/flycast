package com.reicast.emulator.emu;

import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioTrack;
import android.os.Build;
import android.util.Log;

import static android.media.AudioTrack.STATE_INITIALIZED;

public final class AudioBackend {
    static { System.loadLibrary("flycast"); }

    private AudioTrack audioTrack;
    private long writePosition;
    private long size;	//size in frames

    public AudioBackend() {
        setInstance(this);
    }

    public void release() {
        setInstance(null);
    }

    // Called by native code
    private boolean init(int bufferSize)
    {
        if (bufferSize == 0)
            bufferSize = AudioTrack.getMinBufferSize(44100, AudioFormat.CHANNEL_OUT_STEREO, AudioFormat.ENCODING_PCM_16BIT);
        else
            bufferSize *= 4;

        audioTrack = new AudioTrack(
                AudioManager.STREAM_MUSIC,
                44100,
                AudioFormat.CHANNEL_OUT_STEREO,
                AudioFormat.ENCODING_PCM_16BIT,
                bufferSize,
                AudioTrack.MODE_STREAM
        );
        if (audioTrack.getState() != STATE_INITIALIZED) {
            audioTrack = null;
            release();
            Log.e("audio", "Error initializing AudioTrack. Disabling sound");
            return false;
        }
        size = bufferSize / 4;
        writePosition = 0;

        Log.i("audio", "Audio streaming: buffer size " + size + " samples / " + size * 1000.0 / 44100.0 + " ms");
        audioTrack.play();

        return true;
    }

    // Called by native code
    private void term()
    {
        if (audioTrack != null) {
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
