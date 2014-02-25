package com.reicast.emulator.config;

import java.io.File;

import org.apache.commons.lang3.ArrayUtils;

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

import com.reicast.emulator.R;
import com.reicast.emulator.debug.GenerateLogs;
import com.reicast.emulator.emu.GL2JNIView;

import de.ankri.views.Switch;

public class ConfigureFragment extends Fragment {

	Activity parentActivity;
	OnClickListener mCallback;
	private Config config;

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
		config = new Config(parentActivity);
		config.getConfigurationPrefs();

		// Generate the menu options and fill in existing settings

		OnCheckedChangeListener dynarec_options = new OnCheckedChangeListener() {

			public void onCheckedChanged(CompoundButton buttonView,
					boolean isChecked) {
				mPrefs.edit().putBoolean("dynarec_opt", isChecked).commit();
				Config.dynarecopt = isChecked;
			}
		};
		Switch dynarec_opt = (Switch) getView().findViewById(
				R.id.dynarec_option);
		dynarec_opt.setChecked(Config.dynarecopt);
		dynarec_opt.setOnCheckedChangeListener(dynarec_options);

		OnCheckedChangeListener unstable_option = new OnCheckedChangeListener() {

			public void onCheckedChanged(CompoundButton buttonView,
					boolean isChecked) {
				mPrefs.edit().putBoolean("unstable_opt", isChecked).commit();
				Config.unstableopt = isChecked;
			}
		};
		Switch unstable_opt = (Switch) getView().findViewById(
				R.id.unstable_option);
		if (Config.unstableopt) {
			unstable_opt.setChecked(true);
		} else {
			unstable_opt.setChecked(false);
		}
		unstable_opt.setOnCheckedChangeListener(unstable_option);

		String[] regions = parentActivity.getResources().getStringArray(
				R.array.region);
		Spinner region_spnr = (Spinner) getView().findViewById(
				R.id.region_spinner);
		ArrayAdapter<String> regionAdapter = new ArrayAdapter<String>(
				parentActivity, R.layout.spinner_selected, regions);
		regionAdapter
				.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
		region_spnr.setAdapter(regionAdapter);

		region_spnr.setSelection(Config.dcregion, true);

		region_spnr.setOnItemSelectedListener(new OnItemSelectedListener() {

			public void onItemSelected(AdapterView<?> parent, View view,
					int pos, long id) {
				mPrefs.edit().putInt("dc_region", pos).commit();
				Config.dcregion = pos;

			}

			public void onNothingSelected(AdapterView<?> arg0) {

			}

		});

		OnCheckedChangeListener limitfps_option = new OnCheckedChangeListener() {

			public void onCheckedChanged(CompoundButton buttonView,
					boolean isChecked) {
				mPrefs.edit().putBoolean("limit_fps", isChecked).commit();
				Config.limitfps = isChecked;
			}
		};
		Switch limit_fps = (Switch) getView()
				.findViewById(R.id.limitfps_option);
		limit_fps.setChecked(Config.limitfps);
		limit_fps.setOnCheckedChangeListener(limitfps_option);

		OnCheckedChangeListener mipmaps_option = new OnCheckedChangeListener() {

			public void onCheckedChanged(CompoundButton buttonView,
					boolean isChecked) {
				mPrefs.edit().putBoolean("use_mipmaps", isChecked).commit();
				Config.mipmaps = isChecked;
			}
		};
		Switch mipmap_opt = (Switch) getView()
				.findViewById(R.id.mipmaps_option);
		mipmap_opt.setChecked(Config.mipmaps);
		mipmap_opt.setOnCheckedChangeListener(mipmaps_option);

		OnCheckedChangeListener full_screen = new OnCheckedChangeListener() {

			public void onCheckedChanged(CompoundButton buttonView,
					boolean isChecked) {
				mPrefs.edit().putBoolean("stretch_view", isChecked).commit();
				Config.widescreen = isChecked;
			}
		};
		Switch stretch_view = (Switch) getView().findViewById(
				R.id.stretch_option);
		stretch_view.setChecked(Config.widescreen);
		stretch_view.setOnCheckedChangeListener(full_screen);

		final TextView mainFrames = (TextView) getView().findViewById(R.id.current_frames);
		mainFrames.setText(String.valueOf(Config.frameskip));

		SeekBar frameSeek = (SeekBar) getView()
				.findViewById(R.id.frame_seekbar);
		frameSeek.setProgress(Config.frameskip);
		frameSeek.setIndeterminate(false);
		frameSeek.setOnSeekBarChangeListener(new OnSeekBarChangeListener() {
			public void onProgressChanged(SeekBar seekBar, int progress,
					boolean fromUser) {
				mainFrames.setText(String.valueOf(progress));
			}

			public void onStartTrackingTouch(SeekBar seekBar) {
				// TODO Auto-generated method stub
			}

			public void onStopTrackingTouch(SeekBar seekBar) {
				int progress = seekBar.getProgress();
				mPrefs.edit().putInt("frame_skip", progress).commit();
				Config.frameskip = progress;
			}
		});

		OnCheckedChangeListener pvr_rendering = new OnCheckedChangeListener() {

			public void onCheckedChanged(CompoundButton buttonView,
					boolean isChecked) {
				mPrefs.edit().putBoolean("pvr_render", isChecked).commit();
				Config.pvrrender = isChecked;
			}
		};
		Switch pvr_render = (Switch) getView().findViewById(R.id.render_option);
		pvr_render.setChecked(Config.pvrrender);
		pvr_render.setOnCheckedChangeListener(pvr_rendering);

		final EditText cheatEdit = (EditText) getView().findViewById(
				R.id.cheat_disk);
		String disk = Config.cheatdisk;
		if (disk != null && disk.contains("/")) {
			cheatEdit.setText(disk.substring(disk.lastIndexOf("/"),
					disk.length()));
		} else {
			cheatEdit.setText(disk);
		}

		cheatEdit.addTextChangedListener(new TextWatcher() {
			public void afterTextChanged(Editable s) {
				if (cheatEdit.getText() != null) {
					String disk = cheatEdit.getText().toString();
					if (disk != null && disk.contains("/")) {
						cheatEdit.setText(disk.substring(disk.lastIndexOf("/"),
								disk.length()));
					} else {
						cheatEdit.setText(disk);
					}
					mPrefs.edit().putString("cheat_disk", disk).commit();
					Config.cheatdisk = disk;
				}
			}

			public void beforeTextChanged(CharSequence s, int start, int count,
					int after) {
			}

			public void onTextChanged(CharSequence s, int start, int before,
					int count) {
			}
		});

		final Switch fps_opt = (Switch) getView().findViewById(R.id.fps_option);
		OnCheckedChangeListener fps_options = new OnCheckedChangeListener() {

			public void onCheckedChanged(CompoundButton buttonView,
					boolean isChecked) {
				mPrefs.edit().putBoolean("show_fps", isChecked).commit();
			}
		};
		boolean counter = mPrefs.getBoolean("show_fps", false);
		fps_opt.setChecked(counter);
		fps_opt.setOnCheckedChangeListener(fps_options);

		final Switch force_gpu_opt = (Switch) getView().findViewById(
				R.id.force_gpu_option);
		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR2) {
			OnCheckedChangeListener force_gpu_options = new OnCheckedChangeListener() {

				public void onCheckedChanged(CompoundButton buttonView,
						boolean isChecked) {
					mPrefs.edit().putBoolean("force_gpu", isChecked).commit();
				}
			};
			boolean enhanced = mPrefs.getBoolean("force_gpu", true);
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
		force_software_opt
				.setChecked(software == GL2JNIView.LAYER_TYPE_SOFTWARE);
		force_software_opt.setOnCheckedChangeListener(force_software);

		Switch sound_opt = (Switch) getView().findViewById(R.id.sound_option);
		OnCheckedChangeListener emu_sound = new OnCheckedChangeListener() {

			public void onCheckedChanged(CompoundButton buttonView,
					boolean isChecked) {
				mPrefs.edit().putBoolean("sound_enabled", isChecked).commit();
			}
		};
		boolean sound = mPrefs.getBoolean("sound_enabled", true);
		sound_opt.setChecked(sound);
		sound_opt.setOnCheckedChangeListener(emu_sound);

		String[] depths = parentActivity.getResources().getStringArray(
				R.array.depth);

		Spinner depth_spnr = (Spinner) getView().findViewById(
				R.id.depth_spinner);
		ArrayAdapter<String> depthAdapter = new ArrayAdapter<String>(
				parentActivity, R.layout.spinner_selected, depths);
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
}
