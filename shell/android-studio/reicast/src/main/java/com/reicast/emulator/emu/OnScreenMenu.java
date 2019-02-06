package com.reicast.emulator.emu;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.view.Gravity;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup.LayoutParams;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.PopupWindow;
import android.widget.ScrollView;
import android.widget.TextView;

import com.reicast.emulator.Emulator;
import com.reicast.emulator.GL2JNIActivity;
import com.reicast.emulator.GL2JNINative;
import com.reicast.emulator.MainActivity;
import com.reicast.emulator.R;
import com.reicast.emulator.config.Config;
import com.reicast.emulator.periph.VmuLcd;

import java.util.Vector;

public class OnScreenMenu {

	private Activity mContext;

	public OnScreenMenu(Activity context, SharedPreferences prefs) {
		if (context instanceof GL2JNINative) {
			this.mContext = context;
		}
		if (context instanceof GL2JNIActivity) {
			this.mContext = context;
		}
	}


	public class FpsPopup extends PopupWindow {

		private TextView fpsText;

		public FpsPopup(Context c) {
			super(c);
			setBackgroundDrawable(null);
			fpsText = new TextView(mContext);
			fpsText.setTextAppearance(mContext, R.style.fpsOverlayText);
			fpsText.setGravity(Gravity.CENTER);
			fpsText.setText("XX");
			setContentView(fpsText);
			setFocusable(false);
		}

		public void setText(int frames) {
			fpsText.setText(String.valueOf(frames));
			fpsText.invalidate();
		}
	}

	public static int getPixelsFromDp(float dps, Context context) {
		return (int) (dps * context.getResources().getDisplayMetrics().density + 0.5f);
	}
}
