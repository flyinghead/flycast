package com.reicast.emulator.config;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
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
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputMethodManager;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemSelectedListener;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.CompoundButton;
import android.widget.CompoundButton.OnCheckedChangeListener;
import android.widget.EditText;
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
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.FilenameFilter;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.lang.ref.WeakReference;
import java.util.Collection;
import java.util.HashSet;
import java.util.List;

public class OptionsFragment extends Fragment {

	private Spinner mSpnrThemes;
	private OnClickListener mCallback;

	private SharedPreferences mPrefs;
	private File sdcard = Environment.getExternalStorageDirectory();
	private String home_directory = sdcard.getAbsolutePath();
	private String game_directory = sdcard.getAbsolutePath();

	private String[] codes;

	// Container Activity must implement this interface
	public interface OnClickListener {
		void onMainBrowseSelected(String path_entry, boolean games, String query);
		void launchBIOSdetection();
	}

	@Override  @SuppressWarnings("deprecation")
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
	public void onAttach(Context context) {
		super.onAttach(context);

		// This makes sure that the container activity has implemented
		// the callback interface. If not, it throws an exception
		try {
			mCallback = (OnClickListener) context;
		} catch (ClassCastException e) {
			throw new ClassCastException(context.getClass().toString()
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
			for (String sd : extStorage) {
				String sdCardPath = sd.replace("mnt/media_rw", "storage");
				if (!sdCardPath.equals(sdcard.getAbsolutePath())) {
					game_directory = sdCardPath;
				}
			}
		}

		home_directory = mPrefs.getString(Config.pref_home, home_directory);
		Emulator app = (Emulator) getActivity().getApplicationContext();
		app.getConfigurationPrefs(mPrefs);

		// Generate the menu options and fill in existing settings

		Button mainBrowse = (Button) getView().findViewById(R.id.browse_main_path);
		mSpnrThemes = (Spinner) getView().findViewById(R.id.pick_button_theme);
		new LocateThemes(this).execute(home_directory + "/themes");

		final EditText editBrowse = (EditText) getView().findViewById(R.id.main_path);
		editBrowse.setText(home_directory);

		mainBrowse.setOnClickListener(new View.OnClickListener() {
			public void onClick(View view) {
				mPrefs.edit().remove(Config.pref_home).apply();
				hideSoftKeyBoard();
				mCallback.launchBIOSdetection();
			}
		});
		editBrowse.setOnEditorActionListener(new EditText.OnEditorActionListener() {
			@Override
			public boolean onEditorAction(TextView v, int actionId, KeyEvent event) {
				if (actionId == EditorInfo.IME_ACTION_DONE
						|| (event != null && event.getKeyCode() == KeyEvent.KEYCODE_ENTER
						&& event.getAction() == KeyEvent.ACTION_DOWN)) {
					if (event == null || !event.isShiftPressed()) {
						if (v.getText() != null) {
							home_directory = v.getText().toString();
							if (home_directory.endsWith("/data")) {
								home_directory.replace("/data", "");
								showToastMessage(getActivity().getString(R.string.data_folder),
										Snackbar.LENGTH_SHORT);
							}
							mPrefs.edit().putString(Config.pref_home, home_directory).apply();
							JNIdc.config(home_directory);
							new LocateThemes(OptionsFragment.this).execute(home_directory + "/themes");
						}
						hideSoftKeyBoard();
						return true;
					}
				}
				return false;
			}
		});

		OnCheckedChangeListener details_options = new OnCheckedChangeListener() {

			public void onCheckedChanged(CompoundButton buttonView,
										 boolean isChecked) {
				mPrefs.edit().putBoolean(Config.pref_gamedetails, isChecked).apply();
				if (!isChecked) {
					File dir = new File(getActivity().getExternalFilesDir(null), "images");
                    File[] files = dir == null ? null : dir.listFiles();
					if (files != null) {
						for (File file : files) {
							if (!file.isDirectory()) {
								file.delete();
							}
						}
					}
				}
			}
		};
		CompoundButton details_opt = (CompoundButton) getView().findViewById(R.id.details_option);
		details_opt.setChecked(mPrefs.getBoolean(Config.pref_gamedetails, false));
		details_opt.setOnCheckedChangeListener(details_options);

		Button gameBrowse = (Button) getView().findViewById(R.id.browse_game_path);

		final EditText editGames = (EditText) getView().findViewById(R.id.game_path);
		game_directory = mPrefs.getString(Config.pref_games, game_directory);
		editGames.setText(game_directory);

		gameBrowse.setOnClickListener(new View.OnClickListener() {
			public void onClick(View view) {
				mPrefs.edit().remove(Config.pref_games).apply();
				if (editBrowse.getText() != null) {
					game_directory = editGames.getText().toString();
				}
				hideSoftKeyBoard();
				mCallback.onMainBrowseSelected(game_directory, true, null);
			}
		});
		editGames.setOnEditorActionListener(new EditText.OnEditorActionListener() {
			@Override
			public boolean onEditorAction(TextView v, int actionId, KeyEvent event) {
				if (actionId == EditorInfo.IME_ACTION_DONE
						|| (event != null && event.getKeyCode() == KeyEvent.KEYCODE_ENTER
						&& event.getAction() == KeyEvent.ACTION_DOWN)) {
					if (event == null || !event.isShiftPressed()) {
						if (v.getText() != null) {
							game_directory = v.getText().toString();
							mPrefs.edit().putString(Config.pref_games, game_directory).apply();
						}
						hideSoftKeyBoard();
						return true;
					}
				}
				return false;
			}
		});

		String[] bios = getResources().getStringArray(R.array.bios);
		codes = getResources().getStringArray(R.array.bioscode);
		Spinner bios_spnr = (Spinner) getView().findViewById(R.id.bios_spinner);
		ArrayAdapter<String> biosAdapter = new ArrayAdapter<>(
				getActivity(), android.R.layout.simple_spinner_item, bios);
		biosAdapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
		bios_spnr.setAdapter(biosAdapter);
		String region = mPrefs.getString("localized", codes[4]);
		bios_spnr.setSelection(biosAdapter.getPosition(region), true);
		bios_spnr.setOnItemSelectedListener(new OnItemSelectedListener() {

			public void onItemSelected(AdapterView<?> parent, View view, int pos, long id) {
				flashBios(codes[pos]);
			}

			public void onNothingSelected(AdapterView<?> arg0) {

			}

		});

		CompoundButton force_software_opt = (CompoundButton) getView().findViewById(
				R.id.software_option);
		OnCheckedChangeListener force_software = new OnCheckedChangeListener() {

			public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
				mPrefs.edit().putInt(Config.pref_rendertype, isChecked ?
						GL2JNIView.LAYER_TYPE_SOFTWARE : GL2JNIView.LAYER_TYPE_HARDWARE).apply();
			}
		};
		int software = mPrefs.getInt(Config.pref_rendertype, GL2JNIView.LAYER_TYPE_HARDWARE);
		force_software_opt.setChecked(software == GL2JNIView.LAYER_TYPE_SOFTWARE);
		force_software_opt.setOnCheckedChangeListener(force_software);

		String[] depths = getResources().getStringArray(R.array.depth);

		Spinner depth_spnr = (Spinner) getView().findViewById(R.id.depth_spinner);
		ArrayAdapter<String> depthAdapter = new ArrayAdapter<>(
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

		Button resetEmu = (Button) getView().findViewById(R.id.reset_emu_btn);
		resetEmu.setOnClickListener(new View.OnClickListener() {
			public void onClick(View view) {
				AlertDialog.Builder b = new AlertDialog.Builder(getActivity());
				b.setIcon(android.R.drawable.ic_dialog_alert);
				b.setTitle(getActivity().getString(R.string.reset_emu_title) + "?");
				b.setMessage(getActivity().getString(R.string.reset_emu_details));
				b.setPositiveButton(android.R.string.yes, new DialogInterface.OnClickListener() {
					public void onClick(DialogInterface dialog, int id) {
						resetEmuSettings();
					}
				});
				b.setNegativeButton(android.R.string.no, null);
				b.show();
			}
		});
	}

	private static class LocateThemes extends AsyncTask<String, Integer, List<File>> {
		private WeakReference<OptionsFragment> options;

		LocateThemes(OptionsFragment context) {
			options = new WeakReference<>(context);
		}

		@Override
		protected List<File> doInBackground(String... paths) {
			File storage = new File(paths[0]);
			String[] mediaTypes = options.get().getResources().getStringArray(R.array.themes);
			FilenameFilter[] filter = new FilenameFilter[mediaTypes.length];
			int i = 0;
			for (final String type : mediaTypes) {
				filter[i] = new FilenameFilter() {
					public boolean accept(File dir, String name) {
						return !dir.getName().startsWith(".") && !name.startsWith(".")
								&& StringUtils.endsWithIgnoreCase(name, "." + type);
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
				ArrayAdapter<String> themeAdapter = new ArrayAdapter<>(
						options.get().getActivity(), android.R.layout.simple_spinner_item, themes);
				themeAdapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
				options.get().mSpnrThemes.setAdapter(themeAdapter);
				options.get().mSpnrThemes.setOnItemSelectedListener(new OnItemSelectedListener() {
					@Override
					public void onItemSelected(AdapterView<?> parentView, View selectedItemView, int position, long id) {
						String theme = String.valueOf(parentView.getItemAtPosition(position));
						if (theme.equals("None")) {
							options.get().mPrefs.edit().remove(Config.pref_theme).apply();
						} else {
							String theme_path = options.get().home_directory + "/themes/" + theme;
							options.get().mPrefs.edit().putString(Config.pref_theme, theme_path).apply();
						}
					}
					@Override
					public void onNothingSelected(AdapterView<?> parentView) {

					}
				});
			} else {
				options.get().mSpnrThemes.setEnabled(false);
			}
		}
	}

	private void hideSoftKeyBoard() {
		try {
			InputMethodManager iMm = (InputMethodManager) getActivity()
					.getSystemService(Context.INPUT_METHOD_SERVICE);
			if (iMm != null && iMm.isAcceptingText()) {
				iMm.hideSoftInputFromWindow(getActivity().getCurrentFocus().getWindowToken(), 0);
			}
		} catch (NullPointerException e) {
			// Keyboard may still be visible
		}
	}

	private void copy(File src, File dst) throws IOException {
		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT) {
			try (InputStream in = new FileInputStream(src)) {
				try (OutputStream out = new FileOutputStream(dst)) {
					// Transfer bytes from in to out
					byte[] buf = new byte[1024];
					int len;
					while ((len = in.read(buf)) > 0) {
						out.write(buf, 0, len);
					}
				}
			}
		} else {
			InputStream in = new FileInputStream(src);
			OutputStream out = new FileOutputStream(dst);
			try {
				// Transfer bytes from in to out
				byte[] buf = new byte[1024];
				int len;
				while ((len = in.read(buf)) > 0) {
					out.write(buf, 0, len);
				}
			} finally {
				in.close();
				out.close();
			}
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
			try {
				copy(local, flash);
			} catch (IOException ex) {
				ex.printStackTrace();
				local.renameTo(flash);
			}
			mPrefs.edit().putString("localized", localized).apply();
		}
	}

	private void resetEmuSettings() {
		mPrefs.edit().remove(Emulator.pref_usereios).apply();
		mPrefs.edit().remove(Config.pref_gamedetails).apply();
		mPrefs.edit().remove(Emulator.pref_dynarecopt).apply();
		mPrefs.edit().remove(Emulator.pref_unstable).apply();
		mPrefs.edit().remove(Emulator.pref_cable).apply();
		mPrefs.edit().remove(Emulator.pref_dcregion).apply();
		mPrefs.edit().remove(Emulator.pref_broadcast).apply();
		mPrefs.edit().remove(Emulator.pref_language).apply();
		mPrefs.edit().remove(Emulator.pref_limitfps).apply();
		mPrefs.edit().remove(Emulator.pref_mipmaps).apply();
		mPrefs.edit().remove(Emulator.pref_widescreen).apply();
		mPrefs.edit().remove(Emulator.pref_frameskip).apply();
		mPrefs.edit().remove(Emulator.pref_pvrrender).apply();
		mPrefs.edit().remove(Emulator.pref_syncedrender).apply();
		mPrefs.edit().remove(Emulator.pref_bootdisk).apply();
		mPrefs.edit().remove(Emulator.pref_showfps).apply();
		mPrefs.edit().remove(Config.pref_rendertype).apply();
		mPrefs.edit().remove(Emulator.pref_nosound).apply();
		mPrefs.edit().remove(Emulator.pref_nobatch).apply();
		mPrefs.edit().remove(Emulator.pref_customtextures).apply();
		mPrefs.edit().remove(Emulator.pref_modvols).apply();
		mPrefs.edit().remove(Emulator.pref_clipping).apply();
		mPrefs.edit().remove(Emulator.pref_dynsafemode).apply();
		mPrefs.edit().remove(Config.pref_renderdepth).apply();
		mPrefs.edit().remove(Config.pref_theme).apply();


		Emulator.usereios = false;
		Emulator.dynarecopt = true;
		Emulator.unstableopt = false;
		Emulator.cable = 3;
		Emulator.dcregion = 3;
		Emulator.broadcast = 4;
		Emulator.language = 6;
		Emulator.limitfps = true;
		Emulator.mipmaps = true;
		Emulator.widescreen = false;
		Emulator.frameskip = 0;
		Emulator.pvrrender = 0;
		Emulator.syncedrender = true;
		Emulator.bootdisk = null;
		Emulator.nosound = false;
		Emulator.nobatch = false;
		Emulator.customtextures = false;
		Emulator.modvols = true;
		Emulator.clipping = true;
		Emulator.dynsafemode = true;

		getActivity().finish();
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
