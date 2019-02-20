package com.reicast.emulator.emu;

import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioTrack;
import android.util.Log;

import java.io.UnsupportedEncodingException;

class EmuThread extends Thread
{
    private IEmulatorView view;
    AudioTrack Player;
    long pos;	//write position
    long size;	//size in frames
    private boolean sound;

    EmuThread(IEmulatorView view) {
        this.view = view;
        this.sound = view.hasSound();
    }

    @Override public void run()
    {
        if (sound) {
            int min=AudioTrack.getMinBufferSize(44100,AudioFormat.CHANNEL_OUT_STEREO,AudioFormat.ENCODING_PCM_16BIT);

            if (2048>min)
                min=2048;

            Player = new AudioTrack(
                    AudioManager.STREAM_MUSIC,
                    44100,
                    AudioFormat.CHANNEL_OUT_STEREO,
                    AudioFormat.ENCODING_PCM_16BIT,
                    min,
                    AudioTrack.MODE_STREAM
            );

            size=min/4;
            pos=0;

            Log.i("audcfg", "Audio streaming: buffer size " + min + " samples / " + min/44100.0 + " ms");
            Player.play();
        }

        JNIdc.run(this);
    }

    // Called by native code
    int WriteBuffer(short[] samples, int wait)
    {
        if (sound) {
            int newdata=samples.length/2;

            if (wait==0)
            {
                //user bytes = write-read
                //available = size - (write - play)
                long used=pos-Player.getPlaybackHeadPosition();
                long avail=size-used;

                //Log.i("audcfg", "u: " + used + " a: " + avail);
                if (avail<newdata)
                    return 0;
            }

            pos+=newdata;

            Player.write(samples, 0, samples.length);
        }

        return 1;
    }

    private void showMessage(final String msg) {
        view.postMessage(msg);
    }

    // Called by native code
    int coreMessage(byte[] msg) {
        try {
            showMessage(new String(msg, "UTF-8"));
        }
        catch (UnsupportedEncodingException e) {
            showMessage("coreMessage: Failed to display error");
        }
        return 1;
    }

    // Called by native code
    void Die() {
        showMessage("Something went wrong and reicast crashed.\nPlease report this on the reicast forums.");
        view.finish();
    }

    // Called by native code
    void reiosInfo(String reiosId, String reiosSoftware) {
        view.reiosInfo(reiosId, reiosSoftware);
    }
}
