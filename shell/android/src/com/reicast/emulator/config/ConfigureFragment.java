package com.reicast.emulator.config;

import java.io.File;

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
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemSelectedListener;
import android.widget.ArrayAdapter;
import android.widget.CompoundButton;
import android.widget.CompoundButton.OnCheckedChangeListener;
import android.widget.EditText;
import android.widget.SeekBar;
import android.widget.SeekBar.OnSeekBarChangeListener;
import android.widget.Spinner;
import android.widget.TextView;

import com.android.util.DreamTime;
import com.reicast.emulator.R;
import com.reicast.emulator.debug.GenerateLogs;
import com.reicast.emulator.emu.GL2JNIView;
import com.reicast.emulator.emu.JNIdc;

import de.ankri.views.Switch;

public class ConfigureFragment extends Fragment {

	Activity parentActivity;
	TextView mainFrames;
	OnClickListener mCallback;
	/*
	 * default settings for emu-cfg
	 */
	public static boolean dynarecopt = true;
	public static boolean idleskip = true;
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

		parentActivity = getActivity();

		mPrefs = PreferenceManager.getDefaultSharedPreferences(parentActivity);
		home_directory = mPrefs.getString("home_directory", home_directory);
		loadCfgSettingsFromPrefs(mPrefs);
		/*
		 * section for all non-emu-cfg settings
		 */
		final Switch fps_opt = (Switch) getView().findViewById(
				R.id.fps_option);
			OnCheckedChangeListener fps_options = new OnCheckedChangeListener() {

				public void onCheckedChanged(CompoundButton buttonView,
						boolean isChecked) {
					mPrefs.edit().putBoolean("show_fps", isChecked).commit();
				}
			};
			boolean counter = mPrefs.getBoolean("show_fps",
					false);
			fps_opt.setChecked(counter);
			fps_opt.setOnCheckedChangeListener(fps_options);

		final Switch force_gpu_opt = (Switch) getView().findViewById(
				R.id.force_gpu_option);
		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR1) {
			OnCheckedChangeListener force_gpu_options = new OnCheckedChangeListener() {

				public void onCheckedChanged(CompoundButton buttonView,
						boolean isChecked) {
					mPrefs.edit().putBoolean("force_gpu", isChecked).commit();
				}
			};
			boolean enhanced = mPrefs.getBoolean("force_gpu",
					true);
			force_gpu_opt.setChecked(enhanced);
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
		/*
		 * section for all emu-cfg settings
		 */
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

		String[] depths = parentActivity.getResources().getStringArray(
				R.array.depth);

		Spinner depth_spnr = (Spinner) getView().findViewById(
				R.id.depth_spinner);
		ArrayAdapter<String> depthAdapter = new ArrayAdapter<String>(
				parentActivity, android.R.layout.simple_spinner_item, depths);
		depthAdapter
				.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
		depth_spnr.setAdapter(depthAdapter);

		String depth = String.valueOf(mPrefs.getInt("depth_render", 24));
		depth_spnr.setSelection(depthAdapter.getPosition(depth), true);

		depth_spnr.setOnItemSelectedListener(new OnItemSelectedListener() {

			public void onItemSelected(AdapterView<?> parent, View view,
					int pos, long id) {
				int render = Integer.valueOf(parent.getItemAtPosition(pos)
						.toString());
				mPrefs.edit().putInt("depth_render", render).commit();

			}

			public void onNothingSelected(AdapterView<?> arg0) {

			}

		});
	}

	public void generateErrorLog() {
		GenerateLogs mGenerateLogs = new GenerateLogs(parentActivity);
		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.HONEYCOMB) {
			mGenerateLogs.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR,
					home_directory);
		} else {
			mGenerateLogs.execute(home_directory);
		}
	}
	
	private static void loadCfgSettingsFromPrefs(SharedPreferences prefs){
		ConfigureFragment.dynarecopt = prefs.getBoolean("dynarec_opt",
				ConfigureFragment.dynarecopt);
		//idleskip not configurable
		ConfigureFragment.unstableopt = prefs.getBoolean("unstable_opt",
				ConfigureFragment.unstableopt);
		//cable not configurable
		//rtc configured automatically
		ConfigureFragment.dcregion = prefs.getInt("dc_region", ConfigureFragment.dcregion);
		//broadcast not configurable
		ConfigureFragment.limitfps = prefs.getBoolean("limit_fps",
				ConfigureFragment.limitfps);
		//nobatch not configurable
		ConfigureFragment.mipmaps = prefs.getBoolean("use_mipmaps",
				ConfigureFragment.mipmaps);
		ConfigureFragment.widescreen = prefs.getBoolean("stretch_view",
				ConfigureFragment.widescreen);
		//subdivide not configurable
		ConfigureFragment.frameskip = prefs.getInt("frame_skip",
				ConfigureFragment.frameskip);
		ConfigureFragment.pvrrender = prefs.getBoolean("pvr_render",
				ConfigureFragment.pvrrender);
		
		ConfigureFragment.cheatdisk = prefs.getString("cheat_disk",
				ConfigureFragment.cheatdisk);
	}
	
	public static void pushCfgToEmu(SharedPreferences prefs){
		//make sure all settings are loaded
		loadCfgSettingsFromPrefs(prefs);
		
		JNIdc.dynarec(ConfigureFragment.dynarecopt ? 1 : 0);
		JNIdc.idleskip(ConfigureFragment.idleskip ? 1 : 0);
		JNIdc.unstable(ConfigureFragment.unstableopt ? 1 : 0);
		JNIdc.cable(ConfigureFragment.cable);
		JNIdc.dreamtime(DreamTime.getDreamtime());
		JNIdc.region(ConfigureFragment.dcregion);
		JNIdc.broadcast(ConfigureFragment.broadcast);
		JNIdc.limitfps(ConfigureFragment.limitfps ? 1 : 0);
		JNIdc.nobatch(ConfigureFragment.nobatch ? 1 : 0);
		JNIdc.mipmaps(ConfigureFragment.mipmaps ? 1 : 0);
		JNIdc.widescreen(ConfigureFragment.widescreen ? 1 : 0);
		JNIdc.subdivide(ConfigureFragment.subdivide ? 1 : 0);
		JNIdc.frameskip(ConfigureFragment.frameskip);
		JNIdc.pvrrender(ConfigureFragment.pvrrender ? 1 : 0);
		
		JNIdc.cheatdisk(ConfigureFragment.cheatdisk);
	}

}
