package com.android.util;

import android.content.Context;
import android.content.SharedPreferences;
import android.graphics.Bitmap;
import android.media.MediaScannerConnection;
import android.media.MediaScannerConnection.OnScanCompletedListener;
import android.net.Uri;
import android.os.Environment;
import android.preference.PreferenceManager;

import com.reicast.emulator.config.Config;

import java.io.File;
import java.io.FileOutputStream;
import java.nio.IntBuffer;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;

import javax.microedition.khronos.opengles.GL10;

public class FileUtils {

	public static void saveScreenshot(final Context c, int w, int h, GL10 gl){
		try {
			SharedPreferences mPrefs = PreferenceManager.getDefaultSharedPreferences(c);
			File dir = new File(mPrefs.getString(Config.pref_home,
					Environment.getExternalStorageDirectory().getAbsolutePath()));
			SimpleDateFormat s = new SimpleDateFormat("yyyyMMddHHmmss", Locale.getDefault());
			String timestamp = s.format(new Date());
			File f = new File(dir.getAbsolutePath(), timestamp+".jpeg");
			FileOutputStream out = new FileOutputStream(f);
			savePixels(0, 0, w, h, gl).compress(Bitmap.CompressFormat.JPEG, 100, out);
			out.close();
			//attempt to put into gallery app
			MediaScannerConnection.scanFile(c.getApplicationContext(), new String[]{f.toString()}, null, new OnScanCompletedListener() {
				public void onScanCompleted(String path, Uri uri) {
					//Log.d("onScanCompleted", path);
					//c.sendBroadcast(new Intent(Intent.ACTION_MEDIA_SCANNER_SCAN_FILE, uri));
				}
			});
		} catch(Exception e) {
			e.printStackTrace();
		}
	}
	
	//thank you stackoverflow
	private static Bitmap savePixels(int x, int y, int w, int h, GL10 gl)
	{
		int b[]=new int[w*(y+h)];
		int bt[]=new int[w*h];
		IntBuffer ib=IntBuffer.wrap(b);
		ib.position(0);
		gl.glReadPixels(x, 0, w, y+h, GL10.GL_RGBA, GL10.GL_UNSIGNED_BYTE, ib);

		for(int i=0, k=0; i<h; i++, k++)
		{//remember, that OpenGL bitmap is incompatible with Android bitmap
			//and so, some correction need.        
			for(int j=0; j<w; j++)
			{
				int pix=b[i*w+j];
				int pb=(pix>>16)&0xff;
				int pr=(pix<<16)&0x00ff0000;
				int pix1=(pix&0xff00ff00) | pr | pb;
				bt[(h-k-1)*w+j]=pix1;
			}
		}
		return Bitmap.createBitmap(bt, w, h, Bitmap.Config.ARGB_8888);
	}
}