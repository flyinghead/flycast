package com.reicast.emulator.config;

import java.io.File;
import java.io.FileOutputStream;
import java.util.Scanner;

import org.apache.commons.lang3.StringUtils;

import android.app.Activity;
import android.content.SharedPreferences;
import android.os.AsyncTask;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.preference.PreferenceManager;
import android.support.v4.app.Fragment;
import android.text.Editable;
import android.text.TextWatcher;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemSelectedListener;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.CompoundButton;
import android.widget.CompoundButton.OnCheckedChangeListener;
import android.widget.EditText;
import android.widget.SeekBar;
import android.widget.SeekBar.OnSeekBarChangeListener;
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.Toast;

import com.android.util.DreamTime;
import com.reicast.emulator.MainActivity;
import com.reicast.emulator.R;
import com.reicast.emulator.debug.GenerateLogs;
import com.reicast.emulator.emu.GL2JNIView;
import com.reicast.emulator.emu.JNIdc;

import de.ankri.views.Switch;

public class ConfigureFragment extends Fragment {

	Activity parentActivity;
	TextView mainFrames;
	OnClickListener mCallback;

	public static boolean dynarecopt = true;
	public static boolean idleskip = false;
	public static boolean unstableopt = false;
	public static int cable = 3;
	public static int dcregion = 3;
	public static int broadcast = 4;
	public static boolean limitfps = true;
	public static boolean nobatch = false;
	public static boolean mipmaps = true;
	public static boolean widescreen = false;
	public static boolean subdivide = false;
	public static int frameskip = 0;
	public static boolean pvrrender = false;
	public static String cheatdisk = "null";

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

		getCurrentConfiguration(mPrefs);

		// Generate the menu options and fill in existing settings
		final Switch force_gpu_opt = (Switch) getView().findViewById(
				R.id.force_gpu_option);
		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR1) {
			OnCheckedChangeListener force_gpu_options = new OnCheckedChangeListener() {

				public void onCheckedChanged(CompoundButton buttonView,
						boolean isChecked) {
					mPrefs.edit().putBoolean("force_gpu", isChecked).commit();
					MainActivity.force_gpu = isChecked;
				}
			};
			force_gpu_opt.setChecked(MainActivity.force_gpu);
			force_gpu_opt.setOnCheckedChangeListener(force_gpu_options);
		} else {
			force_gpu_opt.setEnabled(false);
		}

		Switch force_software_opt = (Switch) getView().findViewById(
				R.id.software_option);
		OnCheckedChangeListener force_software = new OnCheckedChangeListener() {

			public void onCheckedChanged(CompoundButton buttonView,
					boolean isChecked) {
				mPrefs.edit().putInt("render_type", isChecked ? 1 : 2).commit();
				if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR1) {
					if (isChecked) {
						force_gpu_opt.setEnabled(false);
						mPrefs.edit().putBoolean("force_gpu", false).commit();
						MainActivity.force_gpu = false;
					} else {
						force_gpu_opt.setEnabled(true);
					}
				}
			}
		};
		int software = mPrefs.getInt("render_type",
				GL2JNIView.LAYER_TYPE_HARDWARE);
		force_software_opt.setChecked(software == GL2JNIView.LAYER_TYPE_SOFTWARE);
		force_software_opt.setOnCheckedChangeListener(force_software);
		
		Switch sound_opt = (Switch) getView().findViewById(
				R.id.sound_option);
		OnCheckedChangeListener emu_sound = new OnCheckedChangeListener() {

			public void onCheckedChanged(CompoundButton buttonView,
					boolean isChecked) {
				mPrefs.edit().putBoolean("sound_enabled", isChecked).commit();
			}
		};
		boolean sound = mPrefs.getBoolean("sound_enabled",
				true);
		sound_opt.setChecked(sound);
		sound_opt.setOnCheckedChangeListener(emu_sound);

		OnCheckedChangeListener dynarec_options = new OnCheckedChangeListener() {

			public void onCheckedChanged(CompoundButton buttonView,
					boolean isChecked) {
				mPrefs.edit().putBoolean("dynarec_opt", isChecked).commit();
				ConfigureFragment.dynarecopt = isChecked;
			}
		};
		Switch dynarec_opt = (Switch) getView().findViewById(
				R.id.dynarec_option);
		dynarec_opt.setChecked(ConfigureFragment.dynarecopt);
		dynarec_opt.setOnCheckedChangeListener(dynarec_options);

		OnCheckedChangeListener unstable_option = new OnCheckedChangeListener() {

			public void onCheckedChanged(CompoundButton buttonView,
					boolean isChecked) {
				mPrefs.edit().putBoolean("unstable_opt", isChecked).commit();
				ConfigureFragment.unstableopt = isChecked;
			}
		};
		Switch unstable_opt = (Switch) getView().findViewById(
				R.id.unstable_option);
		if (ConfigureFragment.unstableopt) {
			unstable_opt.setChecked(true);
		} else {
			unstable_opt.setChecked(false);
		}
		unstable_opt.setOnCheckedChangeListener(unstable_option);

		String[] regions = parentActivity.getResources().getStringArray(
				R.array.region);

		Spinner region_spnr = (Spinner) getView().findViewById(
				R.id.region_spinner);
		ArrayAdapter<String> localeAdapter = new ArrayAdapter<String>(
				parentActivity, android.R.layout.simple_spinner_item, regions);
		localeAdapter
				.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
		region_spnr.setAdapter(localeAdapter);

		region_spnr.setSelection(ConfigureFragment.dcregion, true);

		region_spnr.setOnItemSelectedListener(new OnItemSelectedListener() {

			public void onItemSelected(AdapterView<?> parent, View view,
					int pos, long id) {
				mPrefs.edit().putInt("dc_region", pos).commit();
				ConfigureFragment.dcregion = pos;

			}

			public void onNothingSelected(AdapterView<?> arg0) {
				mPrefs.edit().putInt("dc_region", ConfigureFragment.dcregion)
						.commit();
			}

		});

		OnCheckedChangeListener limitfps_option = new OnCheckedChangeListener() {

			public void onCheckedChanged(CompoundButton buttonView,
					boolean isChecked) {
				mPrefs.edit().putBoolean("limit_fps", isChecked).commit();
				ConfigureFragment.limitfps = isChecked;
			}
		};
		Switch limit_fps = (Switch) getView()
				.findViewById(R.id.limitfps_option);
		limit_fps.setChecked(ConfigureFragment.limitfps);
		limit_fps.setOnCheckedChangeListener(limitfps_option);

		OnCheckedChangeListener mipmaps_option = new OnCheckedChangeListener() {

			public void onCheckedChanged(CompoundButton buttonView,
					boolean isChecked) {
				mPrefs.edit().putBoolean("use_mipmaps", isChecked).commit();
				ConfigureFragment.mipmaps = isChecked;
			}
		};
		Switch mipmap_opt = (Switch) getView()
				.findViewById(R.id.mipmaps_option);
		mipmap_opt.setChecked(ConfigureFragment.mipmaps);
		mipmap_opt.setOnCheckedChangeListener(mipmaps_option);

		OnCheckedChangeListener full_screen = new OnCheckedChangeListener() {

			public void onCheckedChanged(CompoundButton buttonView,
					boolean isChecked) {
				mPrefs.edit().putBoolean("stretch_view", isChecked).commit();
				ConfigureFragment.widescreen = isChecked;
			}
		};
		Switch stretch_view = (Switch) getView().findViewById(
				R.id.stretch_option);
		stretch_view.setChecked(ConfigureFragment.widescreen);
		stretch_view.setOnCheckedChangeListener(full_screen);

		mainFrames = (TextView) getView().findViewById(R.id.current_frames);
		mainFrames.setText(String.valueOf(ConfigureFragment.frameskip));

		SeekBar frameSeek = (SeekBar) getView()
				.findViewById(R.id.frame_seekbar);

		frameSeek.setProgress(ConfigureFragment.frameskip);

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
				ConfigureFragment.frameskip = progressChanged;
			}
		});

		OnCheckedChangeListener pvr_rendering = new OnCheckedChangeListener() {

			public void onCheckedChanged(CompoundButton buttonView,
					boolean isChecked) {
				mPrefs.edit().putBoolean("pvr_render", isChecked).commit();
				ConfigureFragment.pvrrender = isChecked;
			}
		};
		Switch pvr_render = (Switch) getView().findViewById(R.id.render_option);
		pvr_render.setChecked(ConfigureFragment.pvrrender);
		pvr_render.setOnCheckedChangeListener(pvr_rendering);

		final EditText cheatEdit = (EditText) getView().findViewById(
				R.id.cheat_disk);
		cheatEdit.setText(ConfigureFragment.cheatdisk);

		cheatEdit.addTextChangedListener(new TextWatcher() {
			public void afterTextChanged(Editable s) {
				if (cheatEdit.getText() != null) {
					ConfigureFragment.cheatdisk = cheatEdit.getText()
							.toString();
					mPrefs.edit()
							.putString("cheat_disk",
									ConfigureFragment.cheatdisk).commit();
				}
			}

			public void beforeTextChanged(CharSequence s, int start, int count,
					int after) {
			}

			public void onTextChanged(CharSequence s, int start, int before,
					int count) {
			}
		});
	}

	public void generateErrorLog() {
		Toast.makeText(parentActivity,
				parentActivity.getString(R.string.platform), Toast.LENGTH_SHORT)
				.show();
		GenerateLogs mGenerateLogs = new GenerateLogs(parentActivity);
		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.HONEYCOMB) {
			mGenerateLogs.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR,
					home_directory);
		} else {
			mGenerateLogs.execute(home_directory);
		}
	}

	public static void getCurrentConfiguration(SharedPreferences mPrefs) {
		ConfigureFragment.dynarecopt = mPrefs.getBoolean("dynarec_opt",
				ConfigureFragment.dynarecopt);
		JNIdc.dynarec(ConfigureFragment.dynarecopt ? 1 : 0);
		JNIdc.idleskip(ConfigureFragment.idleskip ? 1 : 0);
		ConfigureFragment.unstableopt = mPrefs.getBoolean("unstable_opt",
				ConfigureFragment.unstableopt);
		JNIdc.unstable(ConfigureFragment.unstableopt ? 1 : 0);
		JNIdc.cable(ConfigureFragment.cable);
		ConfigureFragment.dcregion = mPrefs.getInt("dc_region", ConfigureFragment.dcregion);
		JNIdc.region(ConfigureFragment.dcregion);
		JNIdc.broadcast(ConfigureFragment.broadcast);
		ConfigureFragment.limitfps = mPrefs.getBoolean("limit_fps",
				ConfigureFragment.limitfps);
		JNIdc.limitfps(ConfigureFragment.limitfps ? 1 : 0);
		JNIdc.nobatch(ConfigureFragment.nobatch ? 1 : 0);
		ConfigureFragment.mipmaps = mPrefs.getBoolean("use_mipmaps",
				ConfigureFragment.mipmaps);
		JNIdc.mipmaps(ConfigureFragment.mipmaps ? 1 : 0);
		ConfigureFragment.widescreen = mPrefs.getBoolean("stretch_view",
				ConfigureFragment.widescreen);
		JNIdc.widescreen(ConfigureFragment.widescreen ? 1 : 0);
		JNIdc.subdivide(ConfigureFragment.subdivide ? 1 : 0);
		ConfigureFragment.frameskip = mPrefs.getInt("frame_skip",
				ConfigureFragment.frameskip);
		JNIdc.frameskip(ConfigureFragment.frameskip);
		ConfigureFragment.pvrrender = mPrefs.getBoolean("pvr_render",
				ConfigureFragment.pvrrender);
		JNIdc.pvrrender(ConfigureFragment.pvrrender ? 1 : 0);
		ConfigureFragment.cheatdisk = mPrefs.getString("cheat_disk",
				ConfigureFragment.cheatdisk);
		JNIdc.cheatdisk(ConfigureFragment.cheatdisk);
		JNIdc.dreamtime(DreamTime.getDreamtime());
		
//		StringBuilder rebuildFile = new StringBuilder();
//		rebuildFile.append("[config]" + "\n");
//		rebuildFile.append("Dynarec.Enabled="
//				+ String.valueOf(ConfigureFragment.dynarecopt ? 1 : 0)
//				+ "\n");
//		rebuildFile.append("Dynarec.idleskip=1" + "\n");
//		rebuildFile.append("Dynarec.unstable-opt="
//				+ String.valueOf(ConfigureFragment.unstableopt ? 1 : 0)
//				+ "\n");
//		rebuildFile.append("Dreamcast.Cable=3" + "\n");
//		rebuildFile.append("Dreamcast.RTC=" + DreamTime.getDreamtime()
//				+ "\n");
//		rebuildFile.append("Dreamcast.Region="
//				+ String.valueOf(ConfigureFragment.dcregion) + "\n");
//		rebuildFile.append("Dreamcast.Broadcast=4" + "\n");
//		rebuildFile.append("aica.LimitFPS="
//				+ String.valueOf(ConfigureFragment.limitfps ? 1 : 0)
//				+ "\n");
//		rebuildFile.append("aica.NoBatch=0" + "\n");
//		rebuildFile.append("rend.UseMipmaps="
//				+ String.valueOf(ConfigureFragment.mipmaps ? 1 : 0)
//				+ "\n");
//		rebuildFile.append("rend.WideScreen="
//				+ String.valueOf(ConfigureFragment.widescreen ? 1 : 0)
//				+ "\n");
//		rebuildFile.append("pvr.Subdivide=0" + "\n");
//		rebuildFile.append("ta.skip="
//				+ String.valueOf(ConfigureFragment.frameskip) + "\n");
//		rebuildFile.append("pvr.rend="
//				+ String.valueOf(ConfigureFragment.pvrrender ? 1 : 0)
//				+ "\n");
//		rebuildFile.append("image=" + ConfigureFragment.cheatdisk
//				+ "\n");
	}
}
