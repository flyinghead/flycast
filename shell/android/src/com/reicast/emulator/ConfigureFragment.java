package com.reicast.emulator;

import java.io.File;
import java.io.FileOutputStream;
import java.util.Scanner;

import org.apache.commons.lang3.StringUtils;

import android.annotation.TargetApi;
import android.app.Activity;
import android.content.SharedPreferences;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.preference.PreferenceManager;
import android.support.v4.app.Fragment;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.CheckBox;
import android.widget.CompoundButton;
import android.widget.CompoundButton.OnCheckedChangeListener;
import android.widget.SeekBar;
import android.widget.SeekBar.OnSeekBarChangeListener;
import android.widget.Switch;
import android.widget.TextView;

@TargetApi(Build.VERSION_CODES.JELLY_BEAN)
public class ConfigureFragment extends Fragment {

	Activity parentActivity;
	TextView mainFrames;
	OnClickListener mCallback;
	boolean widescreen = false;
	boolean unstable_opt = false;
	int frameskip = 0;

	private SharedPreferences mPrefs;
	private File sdcard = Environment.getExternalStorageDirectory();
	private String home_directory = sdcard + "/dc";

	// Container Activity must implement this interface
	public interface OnClickListener {
		public void onMainBrowseSelected(String path_entry, boolean games);
	}

	@Override
	public void onAttach(Activity activity) {
		super.onAttach(activity);

	}

	@Override
	public View onCreateView(LayoutInflater inflater, ViewGroup container,
			Bundle savedInstanceState) {
		// Inflate the layout for this fragment
		return inflater.inflate(R.layout.configure_fragment, container, false);
	}

	@Override
	public void onViewCreated(View view, Bundle savedInstanceState) {
		// setContentView(R.layout.activity_main);

		parentActivity = getActivity();

		mPrefs = PreferenceManager.getDefaultSharedPreferences(parentActivity);
		home_directory = mPrefs.getString("home_directory", home_directory);

		try {
			File config = new File(home_directory, "emu.cfg");
			if (config.exists()) {
				Scanner scanner = new Scanner(config);
				String currentLine;
				while (scanner.hasNextLine()) {
					currentLine = scanner.nextLine();
					if (StringUtils.containsIgnoreCase(currentLine,
							"rend.WideScreen")) {
						widescreen = Boolean.valueOf(currentLine.replace(
								"rend.WideScreen=", ""));
					}
					if (StringUtils.containsIgnoreCase(currentLine, "ta.skip")) {
						frameskip = Integer.valueOf(currentLine.replace(
								"ta.skip=", ""));
					}
					if (StringUtils.containsIgnoreCase(currentLine, "Dynarec.unstable-opt")) {
						unstable_opt = Boolean.valueOf(currentLine.replace(
								"Dynarec.unstable-opt=", ""));
					}
				}
				scanner.close();
			}
		} catch (Exception e) {
			Log.d("reicast", "Exception: " + e);
		}

		mainFrames = (TextView) getView().findViewById(R.id.current_frames);
		mainFrames.setText(String.valueOf(frameskip));

		OnCheckedChangeListener full_screen = new OnCheckedChangeListener() {

			public void onCheckedChanged(CompoundButton buttonView,
					boolean isChecked) {
				mPrefs.edit().putBoolean("stretch_view", isChecked).commit();
				widescreen = isChecked;
				if (!executeAppendConfig("rend.WideScreen",
						String.valueOf(isChecked ? 1 : 0))) {
					executeWriteConfig();
				}
			}
		};
		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.HONEYCOMB_MR1) {
			Switch stretch_view = (Switch) getView().findViewById(
					R.id.stretch_option);
			boolean stretched = mPrefs.getBoolean("stretch_view", widescreen);
			if (stretched) {
				stretch_view.setChecked(true);
			} else {
				stretch_view.setChecked(false);
			}
			stretch_view.setOnCheckedChangeListener(full_screen);
		} else {
			CheckBox stretch_view = (CheckBox) getView().findViewById(
					R.id.stretch_option);
			boolean stretched = mPrefs.getBoolean("stretch_view", widescreen);
			if (stretched) {
				stretch_view.setChecked(true);
			} else {
				stretch_view.setChecked(false);
			}
			stretch_view.setOnCheckedChangeListener(full_screen);
		}

		SeekBar frameSeek = (SeekBar) getView()
				.findViewById(R.id.frame_seekbar);

		int userFrames = mPrefs.getInt("frame_skip", frameskip);
		frameSeek.setProgress(userFrames);

		frameSeek.setOnSeekBarChangeListener(new OnSeekBarChangeListener() {
			int progressChanged = 0;

			public void onProgressChanged(SeekBar seekBar, int progress,
					boolean fromUser) {
				progressChanged = progress;
				mainFrames.setText(String.valueOf(progress));
			}

			public void onStartTrackingTouch(SeekBar seekBar) {
				// TODO Auto-generated method stub
			}

			public void onStopTrackingTouch(SeekBar seekBar) {
				mPrefs.edit().putInt("frame_skip", progressChanged).commit();
				frameskip = progressChanged;
				if (!executeAppendConfig("ta.skip",
						String.valueOf(progressChanged))) {
					executeWriteConfig();
				}
			}
		});

		OnCheckedChangeListener pvr_rendering = new OnCheckedChangeListener() {

			public void onCheckedChanged(CompoundButton buttonView,
					boolean isChecked) {
				mPrefs.edit().putBoolean("unstable_opt", isChecked).commit();
				unstable_opt = isChecked;
				if (!executeAppendConfig("Dynarec.unstable-opt",
						String.valueOf(isChecked ? 1 : 0))) {
					executeWriteConfig();
				}
			}
		};
		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.HONEYCOMB_MR1) {
			Switch pvr_render = (Switch) getView().findViewById(
					R.id.render_option);
			boolean rendered = mPrefs.getBoolean("unstable_opt", unstable_opt);
			if (rendered) {
				pvr_render.setChecked(true);
			} else {
				pvr_render.setChecked(false);
			}
			pvr_render.setOnCheckedChangeListener(pvr_rendering);
		} else {
			CheckBox pvr_render = (CheckBox) getView().findViewById(
					R.id.render_option);
			boolean rendered = mPrefs.getBoolean("unstable_opt", unstable_opt);
			if (rendered) {
				pvr_render.setChecked(true);
			} else {
				pvr_render.setChecked(false);
			}
			pvr_render.setOnCheckedChangeListener(pvr_rendering);
		}
	}

	private boolean executeAppendConfig(String identifier, String value) {
		File config = new File(home_directory, "emu.cfg");
		if (config.exists()) {
			try {
				StringBuilder rebuildFile = new StringBuilder();
				Scanner scanner = new Scanner(config);
				String currentLine;
				while (scanner.hasNextLine()) {
					currentLine = scanner.nextLine();
					if (StringUtils.containsIgnoreCase(currentLine, identifier)) {
						rebuildFile.append(identifier + "=" + value + "\n");
					} else {
						rebuildFile.append(currentLine + "\n");
					}
				}
				scanner.close();
				config.delete();
				FileOutputStream fos = new FileOutputStream(config);
				fos.write(rebuildFile.toString().getBytes());
				fos.close();
				return true;
			} catch (Exception e) {
				Log.d("reicast", "Exception: " + e);
			}
		}
		return false;
	}

	private void executeWriteConfig() {
		try {
			File config = new File(home_directory, "emu.cfg");
			if (config.exists()) {
				config.delete();
			}
			StringBuilder rebuildFile = new StringBuilder();
			rebuildFile.append("[config]" + "\n");
			rebuildFile.append("Dynarec.Enabled=1" + "\n");
			rebuildFile.append("Dynarec.idleskip=1" + "\n");
			rebuildFile.append("Dynarec.unstable-opt=" + String.valueOf(unstable_opt ? 1 : 0) + "\n");
			rebuildFile.append("Dreamcast.Cable=3" + "\n");
			rebuildFile.append("Dreamcast.RTC=2018927206" + "\n");
			rebuildFile.append("Dreamcast.Region=3" + "\n");
			rebuildFile.append("Dreamcast.Broadcast=4" + "\n");
			rebuildFile.append("aica.LimitFPS=1" + "\n");
			rebuildFile.append("aica.NoBatch=0" + "\n");
			rebuildFile.append("rend.UseMipmaps=1" + "\n");
			rebuildFile.append("rend.WideScreen="
					+ String.valueOf(widescreen ? 1 : 0) + "\n");
			rebuildFile.append("pvr.Subdivide=0" + "\n");
			rebuildFile.append("ta.skip=" + String.valueOf(frameskip) + "\n");
			rebuildFile.append("pvr.rend=1" + "\n");
			rebuildFile.append("image=null" + "\n");
			FileOutputStream fos = new FileOutputStream(config);
			fos.write(rebuildFile.toString().getBytes());
			fos.close();
		} catch (Exception e) {
			Log.d("reicast", "Exception: " + e);
		}
	}
}
