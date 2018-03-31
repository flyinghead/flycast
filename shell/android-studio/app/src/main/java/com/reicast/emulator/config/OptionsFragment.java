package com.reicast.emulator.config;

import android.app.Activity;
import android.content.Context;
import android.content.SharedPreferences;
import android.graphics.drawable.Drawable;
import android.os.AsyncTask;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.preference.PreferenceManager;
import android.support.constraint.ConstraintLayout;
import android.support.design.widget.Snackbar;
import android.support.graphics.drawable.VectorDrawableCompat;
import android.support.v4.app.Fragment;
import android.text.Editable;
import android.text.TextWatcher;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.inputmethod.InputMethodManager;
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

import com.android.util.FileUtils;
import com.reicast.emulator.Emulator;
import com.reicast.emulator.FileBrowser;
import com.reicast.emulator.R;
import com.reicast.emulator.emu.GL2JNIView;
import com.reicast.emulator.emu.JNIdc;

import org.apache.commons.lang3.StringUtils;

import java.io.File;
import java.io.FilenameFilter;
import java.util.Collection;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;

public class OptionsFragment extends Fragment {

	private Emulator app;
	
	private Button mainBrowse;
	private Button gameBrowse;
	private Spinner mSpnrThemes;
	private OnClickListener mCallback;

	private SharedPreferences mPrefs;
	private File sdcard = Environment.getExternalStorageDirectory();
	private String home_directory = sdcard.getAbsolutePath();
	private String game_directory = sdcard.getAbsolutePath();
	
	private String[] codes;

	// Container Activity must implement this interface
	public interface OnClickListener {
        void onMainBrowseSelected(boolean browse, String path_entry, boolean games, String query);
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
		
		mPrefs = PreferenceManager.getDefaultSharedPreferences(getActivity());
		
		// Specialized handler for devices with an extSdCard mount for external
		HashSet<String> extStorage = FileBrowser.getExternalMounts();
		if (extStorage != null && !extStorage.isEmpty()) {
			for (Iterator<String> sd = extStorage.iterator(); sd.hasNext();) {
				String sdCardPath = sd.next().replace("mnt/media_rw", "storage");
				if (!sdCardPath.equals(sdcard.getAbsolutePath())) {
					game_directory = sdCardPath;
				}
			}
		}
		
		home_directory = mPrefs.getString(Config.pref_home, home_directory);
		app = (Emulator) getActivity().getApplicationContext();
		app.getConfigurationPrefs(mPrefs);

		// Generate the menu options and fill in existing settings
		
		mainBrowse = (Button) getView().findViewById(R.id.browse_main_path);
		mSpnrThemes = (Spinner) getView().findViewById(R.id.pick_button_theme);
		new LocateThemes().execute(home_directory + "/themes");

		final EditText editBrowse = (EditText) getView().findViewById(R.id.main_path);
		editBrowse.setText(home_directory);

		mainBrowse.setOnClickListener(new View.OnClickListener() {
			public void onClick(View view) {
				mPrefs.edit().remove(Config.pref_home).apply();
                hideSoftKeyBoard();
				mCallback.onMainBrowseSelected(false, home_directory, false, null);
			}
		});

		editBrowse.addTextChangedListener(new TextWatcher() {
			public void afterTextChanged(Editable s) {
				if (editBrowse.getText() != null) {
					home_directory = editBrowse.getText().toString();
					if (home_directory.endsWith("/data")) {
						home_directory.replace("/data", "");
						showToastMessage(getActivity().getString(R.string.data_folder),
								Snackbar.LENGTH_SHORT);
					}
					mPrefs.edit().putString(Config.pref_home, home_directory).apply();
					JNIdc.config(home_directory);
					new LocateThemes().execute(home_directory + "/themes");
				}
			}

			public void beforeTextChanged(CharSequence s, int start, int count, int after) {
			}

			public void onTextChanged(CharSequence s, int start, int before, int count) {
			}
		});
		
		OnCheckedChangeListener reios_options = new OnCheckedChangeListener() {

			public void onCheckedChanged(CompoundButton buttonView,
					boolean isChecked) {
				mPrefs.edit().putBoolean(Emulator.pref_usereios, isChecked).apply();
			}
		};
		CompoundButton reios_opt = (CompoundButton) getView().findViewById(
				R.id.reios_option);
		reios_opt.setChecked(mPrefs.getBoolean(Emulator.pref_usereios, false));
		reios_opt.setOnCheckedChangeListener(reios_options);
		
		OnCheckedChangeListener details_options = new OnCheckedChangeListener() {

			public void onCheckedChanged(CompoundButton buttonView,
					boolean isChecked) {
				mPrefs.edit().putBoolean(Config.pref_gamedetails, isChecked).apply();
				if (!isChecked) {
					File dir = new File(getActivity().getExternalFilesDir(null), "images");
					for (File file : dir.listFiles()) {
						if (!file.isDirectory()) {
							file.delete();
						}
					}
				}
			}
		};
		CompoundButton details_opt = (CompoundButton) getView().findViewById(
				R.id.details_option);
		details_opt.setChecked(mPrefs.getBoolean(Config.pref_gamedetails, false));
		details_opt.setOnCheckedChangeListener(details_options);

		gameBrowse = (Button) getView().findViewById(R.id.browse_game_path);

		final EditText editGames = (EditText) getView().findViewById(
				R.id.game_path);
		game_directory = mPrefs.getString(Config.pref_games, game_directory);
		editGames.setText(game_directory);

		gameBrowse.setOnClickListener(new View.OnClickListener() {
			public void onClick(View view) {
				mPrefs.edit().remove(Config.pref_games).apply();
				if (editBrowse.getText() != null) {
					game_directory = editGames.getText().toString();
				}
                hideSoftKeyBoard();
				mCallback.onMainBrowseSelected(false, game_directory, true, null);
			}
		});

		editGames.addTextChangedListener(new TextWatcher() {
			public void afterTextChanged(Editable s) {
				if (editBrowse.getText() != null) {
					game_directory = editGames.getText().toString();
					mPrefs.edit().putString(Config.pref_games, game_directory).apply();
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
				mPrefs.edit().putBoolean(Emulator.pref_nativeact, isChecked).apply();
				Emulator.nativeact = isChecked;
			}
		};
		CompoundButton native_opt = (CompoundButton) getView().findViewById(
				R.id.native_option);
		native_opt.setChecked(Emulator.nativeact);
		native_opt.setOnCheckedChangeListener(native_options);

		OnCheckedChangeListener dynarec_options = new OnCheckedChangeListener() {

			public void onCheckedChanged(CompoundButton buttonView,
					boolean isChecked) {
				mPrefs.edit().putBoolean(Emulator.pref_dynarecopt, isChecked).apply();
				Emulator.dynarecopt = isChecked;
			}
		};
		CompoundButton dynarec_opt = (CompoundButton) getView().findViewById(
				R.id.dynarec_option);
		dynarec_opt.setChecked(Emulator.dynarecopt);
		dynarec_opt.setOnCheckedChangeListener(dynarec_options);

		OnCheckedChangeListener unstable_option = new OnCheckedChangeListener() {

			public void onCheckedChanged(CompoundButton buttonView,
					boolean isChecked) {
				mPrefs.edit().putBoolean(Emulator.pref_unstable, isChecked).apply();
				Emulator.unstableopt = isChecked;
			}
		};
		CompoundButton unstable_opt = (CompoundButton) getView().findViewById(
				R.id.unstable_option);
		if (Emulator.unstableopt) {
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

		cable_spnr.setSelection(Emulator.cable - 1, true);

		cable_spnr.setOnItemSelectedListener(new OnItemSelectedListener() {

			public void onItemSelected(AdapterView<?> parent, View view,
					int pos, long id) {
				mPrefs.edit().putInt(Emulator.pref_cable, pos + 1).apply();
				Emulator.cable = pos + 1;
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
		region_spnr.setSelection(Emulator.dcregion, true);
		region_spnr.setOnItemSelectedListener(new OnItemSelectedListener() {
			public void onItemSelected(AdapterView<?> parent, View view, int pos, long id) {
				mPrefs.edit().putInt(Emulator.pref_dcregion, pos).apply();
				Emulator.dcregion = pos;

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
		String cast = String.valueOf(Emulator.broadcast);
		for (int i = 0; i < broadcasts.length; i++) {
			if (broadcasts[i].startsWith(cast + " - "))
				select = i;
		}

		broadcast_spnr.setSelection(select, true);
		broadcast_spnr.setOnItemSelectedListener(new OnItemSelectedListener() {

			public void onItemSelected(AdapterView<?> parent, View view, int pos, long id) {
				String item = parent.getItemAtPosition(pos).toString();
				String selection = item.substring(0, item.indexOf(" - "));
				mPrefs.edit().putInt(Emulator.pref_broadcast, Integer.parseInt(selection)).apply();
				Emulator.broadcast = Integer.parseInt(selection);

			}

			public void onNothingSelected(AdapterView<?> arg0) {

			}
		});

		OnCheckedChangeListener limitfps_option = new OnCheckedChangeListener() {

			public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
				mPrefs.edit().putBoolean(Emulator.pref_limitfps, isChecked).apply();
				Emulator.limitfps = isChecked;
			}
		};
		CompoundButton limit_fps = (CompoundButton) getView().findViewById(R.id.limitfps_option);
		limit_fps.setChecked(Emulator.limitfps);
		limit_fps.setOnCheckedChangeListener(limitfps_option);

		OnCheckedChangeListener mipmaps_option = new OnCheckedChangeListener() {

			public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
				mPrefs.edit().putBoolean(Emulator.pref_mipmaps, isChecked).apply();
				Emulator.mipmaps = isChecked;
			}
		};
		CompoundButton mipmap_opt = (CompoundButton) getView().findViewById(R.id.mipmaps_option);
		mipmap_opt.setChecked(Emulator.mipmaps);
		mipmap_opt.setOnCheckedChangeListener(mipmaps_option);

		OnCheckedChangeListener full_screen = new OnCheckedChangeListener() {

			public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
				mPrefs.edit().putBoolean(Emulator.pref_widescreen, isChecked).apply();
				Emulator.widescreen = isChecked;
			}
		};
		CompoundButton stretch_view = (CompoundButton) getView().findViewById(R.id.stretch_option);
		stretch_view.setChecked(Emulator.widescreen);
		stretch_view.setOnCheckedChangeListener(full_screen);

		final EditText mainFrames = (EditText) getView().findViewById(R.id.current_frames);
		mainFrames.setText(String.valueOf(Emulator.frameskip));

		final SeekBar frameSeek = (SeekBar) getView().findViewById(R.id.frame_seekbar);
		frameSeek.setProgress(Emulator.frameskip);
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
				mPrefs.edit().putInt(Emulator.pref_frameskip, progress).apply();
				Emulator.frameskip = progress;
			}
		});
		mainFrames.addTextChangedListener(new TextWatcher() {
			public void afterTextChanged(Editable s) {
				String frameText = mainFrames.getText().toString();
				if (frameText != null) {
					int frames = Integer.parseInt(frameText);
					frameSeek.setProgress(frames);
					mPrefs.edit().putInt(Emulator.pref_frameskip, frames).apply();
					Emulator.frameskip = frames;
				}
			}

			public void beforeTextChanged(CharSequence s, int start, int count, int after) {
			}

			public void onTextChanged(CharSequence s, int start, int before, int count) {
			}
		});

		OnCheckedChangeListener pvr_rendering = new OnCheckedChangeListener() {

			public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
				mPrefs.edit().putBoolean(Emulator.pref_pvrrender, isChecked).apply();
				Emulator.pvrrender = isChecked;
			}
		};
		CompoundButton pvr_render = (CompoundButton) getView().findViewById(R.id.render_option);
		pvr_render.setChecked(Emulator.pvrrender);
		pvr_render.setOnCheckedChangeListener(pvr_rendering);

        OnCheckedChangeListener synchronous = new OnCheckedChangeListener() {
			public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
				mPrefs.edit().putBoolean(Emulator.pref_syncedrender, isChecked).apply();
				Emulator.syncedrender = isChecked;
			}
		};
		CompoundButton synced_render = (CompoundButton) getView().findViewById(R.id.syncrender_option);
		synced_render.setChecked(Emulator.syncedrender);
		synced_render.setOnCheckedChangeListener(synchronous);

		final EditText cheatEdit = (EditText) getView().findViewById(R.id.cheat_disk);
		String disk = Emulator.cheatdisk;
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
					mPrefs.edit().putString(Emulator.pref_cheatdisk, disk).apply();
					Emulator.cheatdisk = disk;
				}
			}

			public void beforeTextChanged(CharSequence s, int start, int count, int after) {
			}

			public void onTextChanged(CharSequence s, int start, int before, int count) {
			}
		});

		final CompoundButton fps_opt = (CompoundButton) getView().findViewById(R.id.fps_option);
		OnCheckedChangeListener fps_options = new OnCheckedChangeListener() {

			public void onCheckedChanged(CompoundButton buttonView,
					boolean isChecked) {
				mPrefs.edit().putBoolean(Config.pref_showfps, isChecked).apply();
			}
		};
		boolean counter = mPrefs.getBoolean(Config.pref_showfps, false);
		fps_opt.setChecked(counter);
		fps_opt.setOnCheckedChangeListener(fps_options);

		final CompoundButton force_gpu_opt = (CompoundButton) getView().findViewById(R.id.force_gpu_option);
		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR2) {
			OnCheckedChangeListener force_gpu_options = new OnCheckedChangeListener() {

				public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
					mPrefs.edit().putBoolean(Config.pref_forcegpu, isChecked).apply();
				}
			};
			boolean enhanced = mPrefs.getBoolean(Config.pref_forcegpu, true);
			force_gpu_opt.setChecked(enhanced);
			force_gpu_opt.setOnCheckedChangeListener(force_gpu_options);
		} else {
			force_gpu_opt.setEnabled(false);
		}

		CompoundButton force_software_opt = (CompoundButton) getView().findViewById(
				R.id.software_option);
		OnCheckedChangeListener force_software = new OnCheckedChangeListener() {

			public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
				mPrefs.edit()
						.putInt(Config.pref_rendertype, isChecked
								? GL2JNIView.LAYER_TYPE_SOFTWARE : GL2JNIView.LAYER_TYPE_HARDWARE
						).apply();
				if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR1) {
					if (isChecked) {
						force_gpu_opt.setEnabled(false);
						mPrefs.edit().putBoolean(Config.pref_forcegpu, false).apply();
					} else {
						force_gpu_opt.setEnabled(true);
					}
				}
			}
		};
		int software = mPrefs.getInt(Config.pref_rendertype, GL2JNIView.LAYER_TYPE_HARDWARE);
		force_software_opt.setChecked(software == GL2JNIView.LAYER_TYPE_SOFTWARE);
		force_software_opt.setOnCheckedChangeListener(force_software);

		CompoundButton sound_opt = (CompoundButton) getView().findViewById(R.id.sound_option);
		OnCheckedChangeListener emu_sound = new OnCheckedChangeListener() {

			public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
				mPrefs.edit().putBoolean(Emulator.pref_nosound, isChecked).apply();
				Emulator.nosound = isChecked;
			}
		};
		boolean sound = mPrefs.getBoolean(Emulator.pref_nosound, false);
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
				mPrefs.edit().putInt(Config.pref_renderdepth, render).apply();
			}

			public void onNothingSelected(AdapterView<?> arg0) {

			}
		});
	}
	
	private final class LocateThemes extends AsyncTask<String, Integer, List<File>> {
		@Override
		protected List<File> doInBackground(String... paths) {
			File storage = new File(paths[0]);
			String[] mediaTypes = getResources().getStringArray(R.array.themes);
			FilenameFilter[] filter = new FilenameFilter[mediaTypes.length];
			int i = 0;
			for (final String type : mediaTypes) {
				filter[i] = new FilenameFilter() {
					public boolean accept(File dir, String name) {
						if (dir.getName().startsWith(".")
								|| name.startsWith(".")) {
							return false;
						} else {
							return StringUtils.endsWithIgnoreCase(name, "."
									+ type);
						}
					}
				};
				i++;
			}
			FileUtils fileUtils = new FileUtils();
			Collection<File> files = fileUtils.listFiles(storage, filter, 0);
			return (List<File>) files;
		}

		@Override
		protected void onPostExecute(List<File> items) {
			if (items != null && !items.isEmpty()) {
				String[] themes = new String[items.size() + 1];
				for (int i = 0; i < items.size(); i ++) {
					themes[i] = items.get(i).getName();
				}
				themes[items.size()] = "None";
				ArrayAdapter<String> themeAdapter = new ArrayAdapter<String>(
						getActivity(), android.R.layout.simple_spinner_item, themes);
				themeAdapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
				mSpnrThemes.setAdapter(themeAdapter);
				mSpnrThemes.setOnItemSelectedListener(new OnItemSelectedListener() {
					@Override
					public void onItemSelected(AdapterView<?> parentView, View selectedItemView, int position, long id) {
						String theme = String.valueOf(parentView.getItemAtPosition(position));
						if (theme.equals("None")) {
							mPrefs.edit().remove(Config.pref_theme).apply();
						} else {
							String theme_path = home_directory + "/themes/" + theme;
							mPrefs.edit().putString(Config.pref_theme, theme_path).apply();
						}
					}
					@Override
					public void onNothingSelected(AdapterView<?> parentView) {

					}
				});
			} else {
				mSpnrThemes.setEnabled(false);
			}
		}
	}

	private void hideSoftKeyBoard() {
		InputMethodManager iMm = (InputMethodManager) getActivity()
				.getSystemService(Context.INPUT_METHOD_SERVICE);
		if (iMm.isAcceptingText()) {
			iMm.hideSoftInputFromWindow(getActivity().getCurrentFocus()
					.getWindowToken(), 0);
		}
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
		mPrefs.edit().putString("localized", localized).apply();
	}

	private void showToastMessage(String message, int duration) {
		ConstraintLayout layout = (ConstraintLayout) getActivity().findViewById(R.id.mainui_layout);
		Snackbar snackbar = Snackbar.make(layout, message, duration);
		View snackbarLayout = snackbar.getView();
		TextView textView = (TextView) snackbarLayout.findViewById(
				android.support.design.R.id.snackbar_text);
		textView.setGravity(Gravity.CENTER_VERTICAL);
		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR1)
			textView.setTextAlignment(View.TEXT_ALIGNMENT_GRAVITY);
		Drawable drawable;
		if (android.os.Build.VERSION.SDK_INT > Build.VERSION_CODES.M) {
			drawable = getResources().getDrawable(
					R.drawable.ic_settings, getActivity().getTheme());
		} else {
			drawable = VectorDrawableCompat.create(getResources(),
					R.drawable.ic_settings, getActivity().getTheme());
		}
		textView.setCompoundDrawablesWithIntrinsicBounds(drawable, null, null, null);
		textView.setCompoundDrawablePadding(getResources()
				.getDimensionPixelOffset(R.dimen.snackbar_icon_padding));
		snackbar.show();
	}
}
