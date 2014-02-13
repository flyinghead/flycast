package com.reicast.emulator.emu;

import java.io.File;
import java.util.Vector;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Environment;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup.LayoutParams;
import android.widget.ImageButton;
import android.widget.ImageView.ScaleType;
import android.widget.LinearLayout;
import android.widget.PopupWindow;

import com.reicast.emulator.MainActivity;
import com.reicast.emulator.R;
import com.reicast.emulator.config.ConfigureFragment;

public class OnScreenMenu {

	private GL2JNIActivity mContext;
	private SharedPreferences prefs;
	LayoutParams params;
	private int frameskip;
	private boolean widescreen;
	private boolean limitframes;
	private boolean audiodisabled;
	private VmuLcd vmuLcd;

	private Vector<PopupWindow> popups;

	private File sdcard = Environment.getExternalStorageDirectory();
	private String home_directory = sdcard + "/dc";

	public OnScreenMenu(Context mContext, SharedPreferences prefs) {
		if (mContext instanceof GL2JNIActivity) {
			this.mContext = (GL2JNIActivity) mContext;
		}
		popups = new Vector<PopupWindow>();
		if (prefs != null) {
			this.prefs = prefs;
			home_directory = prefs.getString("home_directory", home_directory);
			widescreen = ConfigureFragment.widescreen;
			frameskip = ConfigureFragment.frameskip;
		}
	}

	public PopupWindow createPopup() {
		final PopupWindow popUp = new PopupWindow(mContext);

		int p = getPixelsFromDp(60, mContext);
		params = new LayoutParams(p, p);

		LinearLayout hlay = new LinearLayout(mContext);

		hlay.setOrientation(LinearLayout.HORIZONTAL);

		hlay.addView(addbut(R.drawable.up, new OnClickListener() {
			public void onClick(View v) {
				popups.remove(popUp);
				popUp.dismiss();
			}
		}), params);
		
		vmuLcd = new VmuLcd(mContext);
		hlay.addView(vmuLcd, params);

		hlay.addView(addbut(R.drawable.vmu_swap, new OnClickListener() {
			public void onClick(View v) {
				JNIdc.vmuSwap();
				popUp.dismiss();
			}
		}), params);

		hlay.addView(addbut(R.drawable.config, new OnClickListener() {
			public void onClick(View v) {
				displayConfigPopup(popUp);
				popups.remove(popUp);
				popUp.dismiss();
			}
		}), params);

		hlay.addView(addbut(R.drawable.disk_unknown, new OnClickListener() {
			public void onClick(View v) {
				displayDebugPopup(popUp);
				popups.remove(popUp);
				popUp.dismiss();
			}
		}), params);

		hlay.addView(addbut(R.drawable.close, new OnClickListener() {
			public void onClick(View v) {
				Intent inte = new Intent(mContext, MainActivity.class);
				mContext.startActivity(inte);
				((Activity) mContext).finish();
			}
		}), params);

		// layout.addView(hlay,params);
		popUp.setContentView(hlay);
		return popUp;
	}

	void displayDebugPopup(final PopupWindow popUp) {
		final PopupWindow popUpDebug = new PopupWindow(mContext);

		int p = getPixelsFromDp(60, mContext);
		LayoutParams debugParams = new LayoutParams(p, p);

		LinearLayout hlay = new LinearLayout(mContext);

		hlay.setOrientation(LinearLayout.HORIZONTAL);

		hlay.addView(addbut(R.drawable.up, new OnClickListener() {
			public void onClick(View v) {
				popups.remove(popUpDebug);
				popUpDebug.dismiss();
				mContext.displayPopUp(popUp);
			}
		}), debugParams);

		hlay.addView(addbut(R.drawable.clear_cache, new OnClickListener() {
			public void onClick(View v) {
				JNIdc.send(0, 0); // Killing texture cache
				popUpDebug.dismiss();
			}
		}), debugParams);

		hlay.addView(addbut(R.drawable.profiler, new OnClickListener() {
			public void onClick(View v) {
				JNIdc.send(1, 3000); // sample_Start(param);
				popUpDebug.dismiss();
			}
		}), debugParams);

		hlay.addView(addbut(R.drawable.profiler, new OnClickListener() {
			public void onClick(View v) {
				JNIdc.send(1, 0); // sample_Start(param);
				popUpDebug.dismiss();
			}
		}), debugParams);

		hlay.addView(addbut(R.drawable.print_stats, new OnClickListener() {
			public void onClick(View v) {
				JNIdc.send(0, 2);
				popUpDebug.dismiss(); // print_stats=true;
			}
		}), debugParams);

		hlay.addView(addbut(R.drawable.close, new OnClickListener() {
			public void onClick(View v) {
				popups.remove(popUpDebug);
				popUpDebug.dismiss();
			}
		}), debugParams);

		popUpDebug.setContentView(hlay);
		popups.add(popUpDebug);
		mContext.displayDebug(popUpDebug);
	}

	void displayConfigPopup(final PopupWindow popUp) {
		final PopupWindow popUpConfig = new PopupWindow(mContext);

		int p = getPixelsFromDp(60, mContext);
		LayoutParams configParams = new LayoutParams(p, p);

		LinearLayout hlay = new LinearLayout(mContext);

		hlay.setOrientation(LinearLayout.HORIZONTAL);

		hlay.addView(addbut(R.drawable.up, new OnClickListener() {
			public void onClick(View v) {
				popups.remove(popUpConfig);
				popUpConfig.dismiss();
				mContext.displayPopUp(popUp);
			}
		}), configParams);

		View fullscreen;
		if (!widescreen) {
			fullscreen = addbut(R.drawable.widescreen, new OnClickListener() {
				public void onClick(View v) {
					JNIdc.widescreen(1);
					popUpConfig.dismiss();
					widescreen = true;
				}
			});
		} else {
			fullscreen = addbut(R.drawable.normal_view, new OnClickListener() {
				public void onClick(View v) {
					JNIdc.widescreen(0);
					popUpConfig.dismiss();
					widescreen = false;
				}
			});
		}
		hlay.addView(fullscreen, params);

		final ImageButton frames_up = new ImageButton(mContext);
		final ImageButton frames_down = new ImageButton(mContext);

		frames_up.setImageResource(R.drawable.frames_up);
		frames_up.setScaleType(ScaleType.FIT_CENTER);
		frames_up.setOnClickListener(new OnClickListener() {
			public void onClick(View v) {
				frameskip++;
				JNIdc.frameskip(frameskip);
				enableState(frames_up, frames_down);
			}
		});

		frames_down.setImageResource(R.drawable.frames_down);
		frames_down.setScaleType(ScaleType.FIT_CENTER);
		frames_down.setOnClickListener(new OnClickListener() {
			public void onClick(View v) {
				frameskip--;
				JNIdc.frameskip(frameskip);
				enableState(frames_up, frames_down);
			}
		});

		hlay.addView(frames_up, params);
		hlay.addView(frames_down, params);
		enableState(frames_up, frames_down);

		View framelimit;
		if (!limitframes) {
			framelimit = addbut(R.drawable.frames_limit_on,
					new OnClickListener() {
						public void onClick(View v) {
							JNIdc.limitfps(1);
							popUpConfig.dismiss();
							limitframes = true;
						}
					});
		} else {
			framelimit = addbut(R.drawable.frames_limit_off,
					new OnClickListener() {
						public void onClick(View v) {
							JNIdc.limitfps(0);
							popUpConfig.dismiss();
							limitframes = false;
						}
					});
		}
		hlay.addView(framelimit, params);

		if (prefs.getBoolean("sound_enabled", true)) {
			View audiosetting;
			if (!audiodisabled) {
				audiosetting = addbut(R.drawable.mute_sound,
						new OnClickListener() {
					public void onClick(View v) {
						mContext.mView.audioDisable(true);
						popUpConfig.dismiss();
						audiodisabled = true;
					}
				});
			} else {
				audiosetting = addbut(R.drawable.enable_sound,
						new OnClickListener() {
					public void onClick(View v) {
						mContext.mView.audioDisable(false);
						popUpConfig.dismiss();
						audiodisabled = false;
					}
				});
			}
			hlay.addView(audiosetting, params);
		}

		hlay.addView(addbut(R.drawable.close, new OnClickListener() {
			public void onClick(View v) {
				popups.remove(popUpConfig);
				popUpConfig.dismiss();
			}
		}), configParams);

		popUpConfig.setContentView(hlay);
		popups.add(popUpConfig);
		mContext.displayConfig(popUpConfig);
	}
	
	private void enableState(View frames_up, View frames_down) {
		if (frameskip <= 0) {
			frames_down.setEnabled(false);
		} else {
			frames_down.setEnabled(true);
		}
		if (frameskip >= 5) {
			frames_up.setEnabled(false);
		} else {
			frames_up.setEnabled(true);
		}
	}
	
	public boolean dismissPopUps() {
		for (PopupWindow popup : popups) {
			if (popup.isShowing()) {
				popup.dismiss();
				popups.remove(popup);
				return true;
			}
		}
		return false;
	}

	public static int getPixelsFromDp(float dps, Context context) {
		return (int) (dps * context.getResources().getDisplayMetrics().density + 0.5f);
	}

	View addbut(int x, OnClickListener ocl) {
		ImageButton but = new ImageButton(mContext);

		but.setImageResource(x);
		but.setScaleType(ScaleType.FIT_CENTER);
		but.setOnClickListener(ocl);

		return but;
	}
	
	public VmuLcd getVmuLcd(){
		return vmuLcd;
	}
}
