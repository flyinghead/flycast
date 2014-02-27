package com.reicast.emulator.emu;

import java.io.File;
import java.util.Vector;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Environment;
import android.view.Gravity;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup.LayoutParams;
import android.widget.ImageButton;
import android.widget.ImageView.ScaleType;
import android.widget.LinearLayout;
import android.widget.PopupWindow;
import android.widget.TextView;

import com.reicast.emulator.GL2JNIActivity;
import com.reicast.emulator.GL2JNINative;
import com.reicast.emulator.MainActivity;
import com.reicast.emulator.R;
import com.reicast.emulator.config.Config;
import com.reicast.emulator.periph.VmuLcd;

public class OnScreenMenu {

	private Activity mContext;
	private SharedPreferences prefs;
	private LinearLayout hlay;
	private LayoutParams params;

	private VmuLcd vmuLcd;

	private Vector<PopupWindow> popups;

	private File sdcard = Environment.getExternalStorageDirectory();
	private String home_directory = sdcard + "/dc";

	private int frames = Config.frameskip;
	private boolean screen = Config.widescreen;
	private boolean limit = Config.limitfps;
	private boolean audio;
	private boolean masteraudio;
	private boolean boosted = false;

	public OnScreenMenu(Activity context, SharedPreferences prefs) {
		if (context instanceof GL2JNINative) {
			this.mContext = (GL2JNINative) context;
		}
		if (context instanceof GL2JNIActivity) {
			this.mContext = (GL2JNIActivity) context;
		}
		popups = new Vector<PopupWindow>();
		if (prefs != null) {
			this.prefs = prefs;
			home_directory = prefs.getString("home_directory", home_directory);
			masteraudio = prefs.getBoolean("sound_enabled", true);
			audio = masteraudio;
		}
		vmuLcd = new VmuLcd(mContext);
		vmuLcd.setOnClickListener(new OnClickListener() {
			public void onClick(View v) {
				if (mContext instanceof GL2JNINative) {
					((GL2JNINative) OnScreenMenu.this.mContext).toggleVmu();
				}
				if (mContext instanceof GL2JNIActivity) {
					((GL2JNIActivity) OnScreenMenu.this.mContext).toggleVmu();
				}
			}
		});
	}

	void displayDebugPopup(final PopupWindow popUp) {
		if (mContext instanceof GL2JNINative) {
			((GL2JNINative) mContext).displayDebug(new DebugPopup(mContext));
		}
		if (mContext instanceof GL2JNIActivity) {
			((GL2JNIActivity) mContext).displayDebug(new DebugPopup(mContext));
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
		}

		public void setText(int frames) {
			fpsText.setText(String.valueOf(frames));
			fpsText.invalidate();
		}
	}

	private void removePopUp(PopupWindow window) {
		window.dismiss();
		popups.remove(window);
		if (mContext instanceof GL2JNINative) {
			((GL2JNINative) mContext)
					.displayPopUp(((GL2JNINative) OnScreenMenu.this.mContext).popUp);
		}
		if (mContext instanceof GL2JNIActivity) {
			((GL2JNIActivity) mContext)
					.displayPopUp(((GL2JNIActivity) OnScreenMenu.this.mContext).popUp);
		}
	}

	public class DebugPopup extends PopupWindow {

		public DebugPopup(Context c) {
			super(c);
			setBackgroundDrawable(null);
			int p = getPixelsFromDp(72, mContext);
			LayoutParams debugParams = new LayoutParams(p, p);

			LinearLayout hlay = new LinearLayout(mContext);

			hlay.setOrientation(LinearLayout.HORIZONTAL);

			hlay.addView(addbut(R.drawable.up, new OnClickListener() {
				public void onClick(View v) {
					removePopUp(DebugPopup.this);
				}
			}), debugParams);

			hlay.addView(addbut(R.drawable.clear_cache, new OnClickListener() {
				public void onClick(View v) {
					JNIdc.send(0, 0); // Killing texture cache
					dismiss();
				}
			}), debugParams);

			hlay.addView(addbut(R.drawable.profiler, new OnClickListener() {
				public void onClick(View v) {
					JNIdc.send(1, 3000); // sample_Start(param);
					dismiss();
				}
			}), debugParams);

			hlay.addView(addbut(R.drawable.profiler, new OnClickListener() {
				public void onClick(View v) {
					JNIdc.send(1, 0); // sample_Start(param);
					dismiss();
				}
			}), debugParams);

			hlay.addView(addbut(R.drawable.print_stats, new OnClickListener() {
				public void onClick(View v) {
					JNIdc.send(0, 2);
					dismiss(); // print_stats=true;
				}
			}), debugParams);

			hlay.addView(addbut(R.drawable.close, new OnClickListener() {
				public void onClick(View v) {
					popups.remove(DebugPopup.this);
					dismiss();
				}
			}), debugParams);

			setContentView(hlay);
			popups.add(this);
		}
	}

	void displayConfigPopup(final PopupWindow popUp) {
		if (mContext instanceof GL2JNINative) {
			((GL2JNINative) mContext).displayConfig(new ConfigPopup(mContext));
		}
		if (mContext instanceof GL2JNIActivity) {
			((GL2JNIActivity) mContext)
					.displayConfig(new ConfigPopup(mContext));
		}
	}

	public class ConfigPopup extends PopupWindow {

		private View fullscreen;
		private View framelimit;
		private View audiosetting;
		private View fastforward;
		private View fdown;
		private View fup;

		public ConfigPopup(Context c) {
			super(c);
			setBackgroundDrawable(null);
			int p = getPixelsFromDp(72, mContext);
			LayoutParams configParams = new LayoutParams(p, p);

			LinearLayout hlay = new LinearLayout(mContext);

			hlay.setOrientation(LinearLayout.HORIZONTAL);

			hlay.addView(addbut(R.drawable.up, new OnClickListener() {
				public void onClick(View v) {
					removePopUp(ConfigPopup.this);
				}
			}), configParams);

			fullscreen = addbut(R.drawable.widescreen, new OnClickListener() {
				public void onClick(View v) {
					if (screen) {
						JNIdc.widescreen(0);
						screen = false;
						((ImageButton) fullscreen)
								.setImageResource(R.drawable.widescreen);
					} else {
						JNIdc.widescreen(1);
						screen = true;
						((ImageButton) fullscreen)
								.setImageResource(R.drawable.normal_view);
					}
				}
			});
			if (screen) {
				((ImageButton) fullscreen)
						.setImageResource(R.drawable.normal_view);

			}
			hlay.addView(fullscreen, params);

			fdown = addbut(R.drawable.frames_down, new OnClickListener() {
				public void onClick(View v) {
					if (frames > 0) {
						frames--;
					}
					JNIdc.frameskip(frames);
					enableState(fdown, fup);
				}
			});
			fup = addbut(R.drawable.frames_up, new OnClickListener() {
				public void onClick(View v) {
					if (frames < 5) {
						frames++;
					}
					JNIdc.frameskip(frames);
					enableState(fdown, fup);
				}
			});

			hlay.addView(fdown, params);
			hlay.addView(fup, params);
			enableState(fdown, fup);

			framelimit = addbut(R.drawable.frames_limit_on,
					new OnClickListener() {
						public void onClick(View v) {
							if (limit) {
								JNIdc.limitfps(0);
								limit = false;
								((ImageButton) framelimit)
										.setImageResource(R.drawable.frames_limit_on);
							} else {
								JNIdc.limitfps(1);
								limit = true;
								((ImageButton) framelimit)
										.setImageResource(R.drawable.frames_limit_off);
							}
						}
					});
			if (limit) {
				((ImageButton) framelimit)
						.setImageResource(R.drawable.frames_limit_off);
			}
			hlay.addView(framelimit, params);

			audiosetting = addbut(R.drawable.enable_sound,
					new OnClickListener() {
						public void onClick(View v) {
							if (audio) {
								if (mContext instanceof GL2JNINative) {
									((GL2JNINative) mContext).mView
											.audioDisable(true);
								}
								if (mContext instanceof GL2JNIActivity) {
									((GL2JNIActivity) mContext).mView
											.audioDisable(true);
								}
								audio = false;
								((ImageButton) audiosetting)
										.setImageResource(R.drawable.enable_sound);
							} else {
								if (mContext instanceof GL2JNINative) {
									((GL2JNINative) mContext).mView
											.audioDisable(false);
								}
								if (mContext instanceof GL2JNIActivity) {
									((GL2JNIActivity) mContext).mView
											.audioDisable(false);
								}
								audio = true;
								((ImageButton) audiosetting)
										.setImageResource(R.drawable.mute_sound);
							}
						}
					});
			if (audio) {
				((ImageButton) audiosetting)
						.setImageResource(R.drawable.mute_sound);
			}
			if (!masteraudio) {
				audiosetting.setEnabled(false);
			}
			hlay.addView(audiosetting, params);

			fastforward = addbut(R.drawable.star, new OnClickListener() {
				public void onClick(View v) {
					if (boosted) {
						if (mContext instanceof GL2JNINative) {
							((GL2JNINative) mContext).mView
									.audioDisable(!audio);
						}
						if (mContext instanceof GL2JNIActivity) {
							((GL2JNIActivity) mContext).mView
									.audioDisable(!audio);
						}
						audiosetting.setEnabled(true);
						JNIdc.limitfps(limit ? 1 : 0);
						framelimit.setEnabled(true);
						JNIdc.frameskip(frames);
						enableState(fdown, fup);
						if (mContext instanceof GL2JNINative) {
							((GL2JNINative) mContext).mView.fastForward(false);
						}
						if (mContext instanceof GL2JNIActivity) {
							((GL2JNIActivity) mContext).mView.fastForward(false);
						}
						boosted = false;
						((ImageButton) fastforward)
								.setImageResource(R.drawable.star);
					} else {
						if (mContext instanceof GL2JNINative) {
							((GL2JNINative) mContext).mView
									.audioDisable(true);
						}
						if (mContext instanceof GL2JNIActivity) {
							((GL2JNIActivity) mContext).mView
									.audioDisable(true);
						}
						audiosetting.setEnabled(false);
						JNIdc.limitfps(0);
						framelimit.setEnabled(false);
						JNIdc.frameskip(5);
						fdown.setEnabled(false);
						fup.setEnabled(false);
						if (mContext instanceof GL2JNINative) {
							((GL2JNINative) mContext).mView.fastForward(true);
						}
						if (mContext instanceof GL2JNIActivity) {
							((GL2JNIActivity) mContext).mView.fastForward(true);
						}
						boosted = true;
						((ImageButton) fastforward)
								.setImageResource(R.drawable.reset);
					}
						}
					});
			if (boosted) {
				((ImageButton) fastforward)
						.setImageResource(R.drawable.reset);
			}
			hlay.addView(fastforward, params);

			hlay.addView(addbut(R.drawable.close, new OnClickListener() {
				public void onClick(View v) {
					popups.remove(ConfigPopup.this);
					dismiss();
				}
			}), configParams);

			setContentView(hlay);
			popups.add(this);
		}
	}

	/**
	 * Toggle the frameskip button visibility by current value
	 * 
	 * @param fdown
	 *            The frameskip reduction button view
	 * @param fup
	 *            The frameskip increase button view
	 */
	private void enableState(View fdown, View fup) {
		if (frames == 0) {
			fdown.setEnabled(false);
		} else {
			fdown.setEnabled(true);
		}
		if (frames == 5) {
			fup.setEnabled(false);
		} else {
			fup.setEnabled(true);
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

	public VmuLcd getVmu() {
		return vmuLcd;
	}

	View addbut(int x, OnClickListener ocl) {
		ImageButton but = new ImageButton(mContext);

		but.setImageResource(x);
		but.setScaleType(ScaleType.FIT_CENTER);
		but.setOnClickListener(ocl);

		return but;
	}

	public class VmuPopup extends PopupWindow {
		LayoutParams vparams;
		LinearLayout vlay;

		public VmuPopup(Context c) {
			super(c);
			setBackgroundDrawable(null);
			int pX = OnScreenMenu.getPixelsFromDp(96, mContext);
			int pY = OnScreenMenu.getPixelsFromDp(68, mContext);
			vparams = new LayoutParams(pX, pY);
			vlay = new LinearLayout(mContext);
			vlay.setOrientation(LinearLayout.HORIZONTAL);
			setContentView(vlay);
		}

		public void showVmu() {
			vmuLcd.configureScale(96);
			vlay.addView(vmuLcd, vparams);
		}

	}

	public class MainPopup extends PopupWindow {

		private View rsticksetting;

		public MainPopup(Context c) {
			super(c);
			setBackgroundDrawable(null);
			int p = getPixelsFromDp(72, mContext);
			params = new LayoutParams(p, p);
			hlay = new LinearLayout(mContext);
			hlay.setOrientation(LinearLayout.HORIZONTAL);

			int vpX = getPixelsFromDp(72, mContext);
			int vpY = getPixelsFromDp(52, mContext);
			LinearLayout.LayoutParams vmuParams = new LinearLayout.LayoutParams(
					vpX, vpY);
			vmuParams.weight = 1.0f;
			vmuParams.gravity = Gravity.CENTER_VERTICAL;
			vmuParams.rightMargin = 4;
			hlay.addView(vmuLcd, vmuParams);

			hlay.addView(addbut(R.drawable.up, new OnClickListener() {
				public void onClick(View v) {
					popups.remove(MainPopup.this);
					dismiss();
				}
			}), params);

			hlay.addView(addbut(R.drawable.vmu_swap, new OnClickListener() {
				public void onClick(View v) {
					JNIdc.vmuSwap();
					dismiss();
				}
			}), params);

			rsticksetting = addbut(R.drawable.toggle_a_b,
					new OnClickListener() {
						public void onClick(View v) {
							if (prefs.getBoolean("right_buttons", true)) {
								prefs.edit().putBoolean("right_buttons", false)
										.commit();
								((ImageButton) rsticksetting)
										.setImageResource(R.drawable.toggle_a_b);
							} else {
								prefs.edit().putBoolean("right_buttons", true)
										.commit();
								((ImageButton) rsticksetting)
										.setImageResource(R.drawable.toggle_r_l);
							}
							dismiss();
						}
					});
			if (prefs.getBoolean("right_buttons", true)) {
				((ImageButton) rsticksetting)
						.setImageResource(R.drawable.toggle_r_l);
			}
			hlay.addView(rsticksetting, params);

			hlay.addView(addbut(R.drawable.config, new OnClickListener() {
				public void onClick(View v) {
					displayConfigPopup(MainPopup.this);
					popups.remove(MainPopup.this);
					dismiss();
				}
			}), params);

			hlay.addView(addbut(R.drawable.disk_unknown, new OnClickListener() {
				public void onClick(View v) {
					displayDebugPopup(MainPopup.this);
					popups.remove(MainPopup.this);
					dismiss();
				}
			}), params);

			hlay.addView(addbut(R.drawable.close, new OnClickListener() {
				public void onClick(View v) {
					Intent inte = new Intent(mContext, MainActivity.class);
					mContext.startActivity(inte);
					((Activity) mContext).finish();
				}
			}), params);

			setContentView(hlay);
			this.setAnimationStyle(R.style.Animation);
		}

		public void showVmu() {
			vmuLcd.configureScale(72);
			hlay.addView(vmuLcd, 0, params);
		}
	}
}
