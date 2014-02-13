package com.reicast.emulator.emu;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.util.Log;
import android.view.View;

public class VmuLcd extends View {
	
	public final static int w = 48;
	public final static int h = 32;
	
	private int[] image = new int[w*h];
	private Bitmap current = Bitmap.createBitmap(w, h, Bitmap.Config.ARGB_8888);
	private float scale;
	
	public VmuLcd(Context context) {
		super(context);
		
		scale = (float)OnScreenMenu.getPixelsFromDp(60, getContext()) / w;
		Log.d("VmuLcd", "scale: "+scale);
	}
	
	public void updateBytes(byte[] data){
		for(int i=0; i<h; i++){
			for(int j=0; j<w; j++){
				image[i*w+j] = data[(h-i-1)*w+j]==0x00?Color.BLACK:Color.WHITE;
			}
		}
		postInvalidate();
	}

	@Override
	public void onDraw(Canvas c){
		current.setPixels(image, 0, w, 0, 0, w, h);
		c.scale(scale, scale);
		c.drawBitmap(current, 0, 0, null);
	}

}
