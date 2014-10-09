package com.reicast.emulator.config;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

import android.app.Activity;
import android.content.SharedPreferences;
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
import android.widget.Button;
import android.widget.CompoundButton;
import android.widget.Toast;
import android.widget.CompoundButton.OnCheckedChangeListener;
import android.widget.EditText;
import android.widget.SeekBar;
import android.widget.SeekBar.OnSeekBarChangeListener;
import android.widget.Spinner;
import android.widget.TextView;

import com.reicast.emulator.R;
import com.reicast.emulator.emu.GL2JNIView;
import com.reicast.emulator.emu.JNIdc;

import de.ankri.views.Switch;

public class OptionsFragment extends Fragment {

	private Config config;
	
	private Button mainBrowse;
	private Button gameBrowse;
	private OnClickListener mCallback;

	private SharedPreferences mPrefs;
	private File sdcard = Environment.getExternalStorageDirectory();
	private String home_directory = sdcard + "/dc";
	private String game_directory = sdcard + "/dc";
	
	private String[] codes;

	// Container Activity must implement this interface
	public interface OnClickListener {
		public void onMainBrowseSelected(String path_entry, boolean games);
	}

	@Override
	public void onAttach(Activity activity) {
		super.onAttach(activity);
		
		// This makes sure that the container activity has implemented
		// the callback interface. If not, it throws an exception
		try {
			mCallback = (OnClickListener) activity;
		} catch (ClassCastException e) {
			throw new ClassCastException(activity.toString()
					+ " must implement OnClickListener");
		}

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

		//parentActivity = getActivity();
		

		mPrefs = PreferenceManager.getDefaultSharedPreferences(getActivity());
		home_directory = mPrefs.getString(Config.pref_home, home_directory);
		config = new Config(getActivity());
		config.getConfigurationPrefs();

		// Generate the menu options and fill in existing settings
		
		mainBrowse = (Button) getView().findViewById(R.id.browse_main_path);

		final EditText editBrowse = (EditText) getView().findViewById(
				R.id.main_path);
		editBrowse.setText(home_directory);

		mainBrowse.setOnClickListener(new View.OnClickListener() {
			public void onClick(View view) {
				mCallback.onMainBrowseSelected(home_directory, false);
			}
		});

		editBrowse.addTextChangedListener(new TextWatcher() {
			public void afterTextChanged(Editable s) {
				if (editBrowse.getText() != null) {
					home_directory = editBrowse.getText().toString();
					if (home_directory.endsWith("/data")) {
						home_directory.replace("/data", "");
						Toast.makeText(getActivity(), R.string.data_folder,
								Toast.LENGTH_SHORT).show();
					}
					mPrefs.edit().putString("home_directory", home_directory)
							.commit();
					JNIdc.config(home_directory);
				}
			}

			public void beforeTextChanged(CharSequence s, int start, int count, int after) {
			}

			public void onTextChanged(CharSequence s, int start, int before, int count) {
			}
		});
		
		OnCheckedChangeListener details_options = new OnCheckedChangeListener() {

			public void onCheckedChanged(CompoundButton buttonView,
					boolean isChecked) {
				mPrefs.edit().putBoolean(Config.pref_gamedetails, isChecked).commit();
			}
		};
		Switch details_opt = (Switch) getView().findViewById(
				R.id.details_option);
		details_opt.setChecked(mPrefs.getBoolean(Config.pref_gamedetails, false));
		details_opt.setOnCheckedChangeListener(details_options);

		gameBrowse = (Button) getView().findViewById(R.id.browse_game_path);

		final EditText editGames = (EditText) getView().findViewById(
				R.id.game_path);
		game_directory = mPrefs.getString("game_directory", game_directory);
		editGames.setText(game_directory);

		gameBrowse.setOnClickListener(new View.OnClickListener() {
			public void onClick(View view) {
				if (editBrowse.getText() != null) {
					game_directory = editGames.getText().toString();
					//mPrefs.edit().putString("game_directory", game_directory).commit();
				}
				mCallback.onMainBrowseSelected(game_directory, true);
			}
		});

		editGames.addTextChangedListener(new TextWatcher() {
			public void afterTextChanged(Editable s) {
				if (editBrowse.getText() != null) {
					game_directory = editGames.getText().toString();
					mPrefs.edit().putString("game_directory", game_directory)
					.commit();
				}
			}

			public void beforeTextChanged(CharSequence s, int start, int count, int after) {
			}

			public void onTextChanged(CharSequence s, int start, int before, int count) {
			}
		});

		String[] bios = getResources().getStringArray(R.array.bios);
		codes = getResources().getStringArray(R.array.bioscode);
		Spinner bios_spnr = (Spinner) getView().findViewById(
				R.id.bios_spinner);
		ArrayAdapter<String> biosAdapter = new ArrayAdapter<String>(
				getActivity(), android.R.layout.simple_spinner_item, bios);
		biosAdapter
				.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
		bios_spnr.setAdapter(biosAdapter);
		String region = mPrefs.getString("localized", codes[4]);
		bios_spnr.setSelection(biosAdapter.getPosition(region), true);
		bios_spnr.setOnItemSelectedListener(new OnItemSelectedListener() {

			public void onItemSelected(AdapterView<?> parent, View view,
					int pos, long id) {
                flashBios(codes[pos]);
			}

			public void onNothingSelected(AdapterView<?> arg0) {
				
			}

		});

		OnCheckedChangeListener native_options = new OnCheckedChangeListener() {

			public void onCheckedChanged(CompoundButton buttonView,
					boolean isChecked) {
				mPrefs.edit().putBoolean(Config.pref_nativeact, isChecked).commit();
				Config.nativeact = isChecked;
			}
		};
		Switch native_opt = (Switch) getView().findViewById(
				R.id.native_option);
		native_opt.setChecked(Config.nativeact);
		native_opt.setOnCheckedChangeListener(native_options);

		OnCheckedChangeListener dynarec_options = new OnCheckedChangeListener() {

			public void onCheckedChanged(CompoundButton buttonView,
					boolean isChecked) {
				mPrefs.edit().putBoolean(Config.pref_dynarecopt, isChecked).commit();
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
				mPrefs.edit().putBoolean(Config.pref_unstable, isChecked).commit();
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

		String[] cables = getResources().getStringArray(
				R.array.cable);
		Spinner cable_spnr = (Spinner) getView().findViewById(
				R.id.cable_spinner);
		ArrayAdapter<String> cableAdapter = new ArrayAdapter<String>(
				getActivity(), R.layout.spinner_selected, cables);
		cableAdapter
				.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
		cable_spnr.setAdapter(cableAdapter);

		cable_spnr.setSelection(Config.cable - 1, true);

		cable_spnr.setOnItemSelectedListener(new OnItemSelectedListener() {

			public void onItemSelected(AdapterView<?> parent, View view,
					int pos, long id) {
				mPrefs.edit().putInt(Config.pref_cable, pos + 1).commit();
				Config.cable = pos + 1;
			}

			public void onNothingSelected(AdapterView<?> arg0) {

			}

		});

//		String[] regions = ArrayUtils.remove(parentActivity.getResources()
//				.getStringArray(R.array.region), 4);
		String[] regions = getResources()
				.getStringArray(R.array.region);
		Spinner region_spnr = (Spinner) getView().findViewById(
				R.id.region_spinner);
		ArrayAdapter<String> regionAdapter = new ArrayAdapter<String>(
				getActivity(), R.layout.spinner_selected, regions);
		regionAdapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
		region_spnr.setAdapter(regionAdapter);
		region_spnr.setSelection(Config.dcregion, true);
		region_spnr.setOnItemSelectedListener(new OnItemSelectedListener() {
			public void onItemSelected(AdapterView<?> parent, View view, int pos, long id) {
				mPrefs.edit().putInt(Config.pref_dcregion, pos).commit();
				Config.dcregion = pos;

			}

			public void onNothingSelected(AdapterView<?> arg0) {

			}
		});

		String[] broadcasts = getResources().getStringArray(R.array.broadcast);
		Spinner broadcast_spnr = (Spinner) getView().findViewById(R.id.broadcast_spinner);
		ArrayAdapter<String> broadcastAdapter = new ArrayAdapter<String>(
				getActivity(), R.layout.spinner_selected, broadcasts);
		broadcastAdapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
		broadcast_spnr.setAdapter(broadcastAdapter);

		int select = 0;
		String cast = String.valueOf(Config.broadcast);
		for (int i = 0; i < broadcasts.length; i++) {
			if (broadcasts[i].startsWith(cast + " - "))
				select = i;
		}

		broadcast_spnr.setSelection(select, true);
		broadcast_spnr.setOnItemSelectedListener(new OnItemSelectedListener() {

			public void onItemSelected(AdapterView<?> parent, View view, int pos, long id) {
				String item = parent.getItemAtPosition(pos).toString();
				String selection = item.substring(0, item.indexOf(" - "));
				mPrefs.edit()
						.putInt(Config.pref_broadcast, Integer.parseInt(selection))
						.commit();
				Config.broadcast = Integer.parseInt(selection);

			}

			public void onNothingSelected(AdapterView<?> arg0) {

			}
		});

		OnCheckedChangeListener limitfps_option = new OnCheckedChangeListener() {

			public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
				mPrefs.edit().putBoolean(Config.pref_limitfps, isChecked).commit();
				Config.limitfps = isChecked;
			}
		};
		Switch limit_fps = (Switch) getView().findViewById(R.id.limitfps_option);
		limit_fps.setChecked(Config.limitfps);
		limit_fps.setOnCheckedChangeListener(limitfps_option);

		OnCheckedChangeListener mipmaps_option = new OnCheckedChangeListener() {

			public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
				mPrefs.edit().putBoolean(Config.pref_mipmaps, isChecked).commit();
				Config.mipmaps = isChecked;
			}
		};
		Switch mipmap_opt = (Switch) getView().findViewById(R.id.mipmaps_option);
		mipmap_opt.setChecked(Config.mipmaps);
		mipmap_opt.setOnCheckedChangeListener(mipmaps_option);

		OnCheckedChangeListener full_screen = new OnCheckedChangeListener() {

			public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
				mPrefs.edit().putBoolean(Config.pref_widescreen, isChecked).commit();
				Config.widescreen = isChecked;
			}
		};
		Switch stretch_view = (Switch) getView().findViewById(R.id.stretch_option);
		stretch_view.setChecked(Config.widescreen);
		stretch_view.setOnCheckedChangeListener(full_screen);

		final TextView mainFrames = (TextView) getView().findViewById(R.id.current_frames);
		mainFrames.setText(String.valueOf(Config.frameskip));

		SeekBar frameSeek = (SeekBar) getView().findViewById(R.id.frame_seekbar);
		frameSeek.setProgress(Config.frameskip);
		frameSeek.setIndeterminate(false);
		frameSeek.setOnSeekBarChangeListener(new OnSeekBarChangeListener() {
			public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
				mainFrames.setText(String.valueOf(progress));
			}

			public void onStartTrackingTouch(SeekBar seekBar) {
				// TODO Auto-generated method stub
			}

			public void onStopTrackingTouch(SeekBar seekBar) {
				int progress = seekBar.getProgress();
				mPrefs.edit().putInt(Config.pref_frameskip, progress).commit();
				Config.frameskip = progress;
			}
		});

		OnCheckedChangeListener pvr_rendering = new OnCheckedChangeListener() {

			public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
				mPrefs.edit().putBoolean(Config.pref_pvrrender, isChecked).commit();
				Config.pvrrender = isChecked;
			}
		};
		Switch pvr_render = (Switch) getView().findViewById(R.id.render_option);
		pvr_render.setChecked(Config.pvrrender);
		pvr_render.setOnCheckedChangeListener(pvr_rendering);

		final EditText cheatEdit = (EditText) getView().findViewById(R.id.cheat_disk);
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
					mPrefs.edit().putString(Config.pref_cheatdisk, disk).commit();
					Config.cheatdisk = disk;
				}
			}

			public void beforeTextChanged(CharSequence s, int start, int count, int after) {
			}

			public void onTextChanged(CharSequence s, int start, int before, int count) {
			}
		});

		final Switch fps_opt = (Switch) getView().findViewById(R.id.fps_option);
		OnCheckedChangeListener fps_options = new OnCheckedChangeListener() {

			public void onCheckedChanged(CompoundButton buttonView,
					boolean isChecked) {
				mPrefs.edit().putBoolean(Config.pref_showfps, isChecked).commit();
			}
		};
		boolean counter = mPrefs.getBoolean(Config.pref_showfps, false);
		fps_opt.setChecked(counter);
		fps_opt.setOnCheckedChangeListener(fps_options);

		final Switch force_gpu_opt = (Switch) getView().findViewById(R.id.force_gpu_option);
		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR2) {
			OnCheckedChangeListener force_gpu_options = new OnCheckedChangeListener() {

				public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
					mPrefs.edit().putBoolean(Config.pref_forcegpu, isChecked).commit();
				}
			};
			boolean enhanced = mPrefs.getBoolean(Config.pref_forcegpu, true);
			force_gpu_opt.setChecked(enhanced);
			force_gpu_opt.setOnCheckedChangeListener(force_gpu_options);
		} else {
			force_gpu_opt.setEnabled(false);
		}

		Switch force_software_opt = (Switch) getView().findViewById(
				R.id.software_option);
		OnCheckedChangeListener force_software = new OnCheckedChangeListener() {

			public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
				mPrefs.edit()
						.putInt(Config.pref_rendertype,
								isChecked ? GL2JNIView.LAYER_TYPE_SOFTWARE
										: GL2JNIView.LAYER_TYPE_HARDWARE)
						.commit();
				if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR1) {
					if (isChecked) {
						force_gpu_opt.setEnabled(false);
						mPrefs.edit().putBoolean(Config.pref_forcegpu, false).commit();
					} else {
						force_gpu_opt.setEnabled(true);
					}
				}
			}
		};
		int software = mPrefs.getInt(Config.pref_rendertype, GL2JNIView.LAYER_TYPE_HARDWARE);
		force_software_opt.setChecked(software == GL2JNIView.LAYER_TYPE_SOFTWARE);
		force_software_opt.setOnCheckedChangeListener(force_software);

		Switch sound_opt = (Switch) getView().findViewById(R.id.sound_option);
		OnCheckedChangeListener emu_sound = new OnCheckedChangeListener() {

			public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
				mPrefs.edit().putBoolean(Config.pref_nosound, isChecked).commit();
				Config.nosound = isChecked;
			}
		};
		boolean sound = mPrefs.getBoolean(Config.pref_nosound, false);
		sound_opt.setChecked(sound);
		sound_opt.setOnCheckedChangeListener(emu_sound);

		String[] depths = getResources().getStringArray(R.array.depth);

		Spinner depth_spnr = (Spinner) getView().findViewById(R.id.depth_spinner);
		ArrayAdapter<String> depthAdapter = new ArrayAdapter<String>(
				getActivity(), R.layout.spinner_selected, depths);
		depthAdapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
		depth_spnr.setAdapter(depthAdapter);

		String depth = String.valueOf(mPrefs.getInt(Config.pref_renderdepth, 24));
		depth_spnr.setSelection(depthAdapter.getPosition(depth), true);

		depth_spnr.setOnItemSelectedListener(new OnItemSelectedListener() {

			public void onItemSelected(AdapterView<?> parent, View view, int pos, long id) {
				int render = Integer.parseInt(parent.getItemAtPosition(pos).toString());
				mPrefs.edit().putInt(Config.pref_renderdepth, render).commit();
			}

			public void onNothingSelected(AdapterView<?> arg0) {

			}
		});
	}
	
	private void flashBios(String localized) {
		File local = new File(home_directory, "data/dc_flash[" + localized
				+ "].bin");
		File flash = new File(home_directory, "data/dc_flash.bin");

		if (local.exists()) {
			if (flash.exists()) {
				flash.delete();
			}
			local.renameTo(flash);
		}
		mPrefs.edit().putString("localized", localized).commit();
	}
}
