package com.reicast.emulator.periph;

import android.media.AudioFormat;
import android.media.AudioRecord;
import android.media.MediaRecorder;
import android.util.Log;

import java.util.concurrent.ConcurrentLinkedQueue;

public class SipEmulator {

	private static final String TAG = "SipEmulator";

	//this needs to get set to the amount the mic normally sends per data request
	//...cant be bigger than a maple packet
	// 240 16 (or 14) bit samples
	private static final int ONE_BLIP_SIZE = 480; //ALSO DEFINED IN maple_devs.h

	private AudioRecord record;
	private RecordThread thread;
	private ConcurrentLinkedQueue<byte[]> bytesReadBuffer;

	private boolean continueRecording;
	private boolean firstGet;
	
	/*
	16-bit PCM @ 11025 hz
	== 176.4 kbit/s
	== 22050 bytes/s
	*/

	private void init(int samplingFreq){
		Log.d(TAG, "SipEmulator init called");

		record = new AudioRecord(
				MediaRecorder.AudioSource.VOICE_RECOGNITION,
				samplingFreq,
				AudioFormat.CHANNEL_IN_MONO,
				AudioFormat.ENCODING_PCM_16BIT,
				samplingFreq * 2);

		bytesReadBuffer = new ConcurrentLinkedQueue<>();

		continueRecording = false;
		firstGet = true;
	}

	public void startRecording(int samplingFreq) {
		Log.d(TAG, "SipEmulator startRecording called. freq " + samplingFreq);
		init(samplingFreq);
		record.startRecording();
		continueRecording = true;
		thread = new RecordThread();
		thread.start();
	}

	public void stopRecording(){
		Log.d(TAG, "SipEmulator stopRecording called");
		continueRecording = false;
		record.stop();
		record.release();
		record = null;
	}

	public byte[] getData(int samples){
		if(firstGet || bytesReadBuffer.size()>50){//50 blips is about 2 seconds!
			firstGet = false;
			return catchUp();
		}
		return bytesReadBuffer.poll();
	}

	private byte[] catchUp(){
		Log.d(TAG, "SipEmulator catchUp");
		byte[] last = bytesReadBuffer.poll();
		bytesReadBuffer.clear();
		return last;
	}

	class RecordThread extends Thread {
		public void run() {
			Log.d(TAG, "RecordThread starting");

			while (continueRecording) {
				byte[] freshData = new byte[ONE_BLIP_SIZE];
				// read blocks
				record.read(freshData, 0, ONE_BLIP_SIZE);
				if (!firstGet) {
					bytesReadBuffer.add(freshData);
				}
			}
		}
	}
}
