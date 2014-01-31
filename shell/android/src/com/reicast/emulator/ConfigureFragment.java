package com.reicast.emulator;

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.File;
import java.io.FileOutputStream;
import java.io.FileWriter;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.util.Scanner;

import org.apache.commons.lang3.StringUtils;

import android.app.Activity;
import android.content.SharedPreferences;
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
import de.ankri.views.Switch;

public class ConfigureFragment extends Fragment {

	Activity parentActivity;
	TextView mainFrames;
	OnClickListener mCallback;

	boolean dynarecopt = true;
	boolean unstableopt = false;
	int dcregion = 3;
	boolean limitfps = true;
	boolean mipmaps = true;
	boolean widescreen = false;
	int frameskip = 0;
	boolean pvrrender = false;
	String cheatdisk = "null";

	boolean tegra = false;
	boolean qualcomm = false;

	private SharedPreferences mPrefs;
	private File sdcard = Environment.getExternalStorageDirectory();
	private String home_directory = sdcard + "/dc";

	public static final String build_model = android.os.Build.MODEL;
	public static final String build_device = android.os.Build.DEVICE;
	public static final String build_board = android.os.Build.BOARD;
	public static final int build_sdk = android.os.Build.VERSION.SDK_INT;

	public static final String DN = "Donut";
	public static final String EC = "Eclair";
	public static final String FR = "Froyo";
	public static final String GB = "Gingerbread";
	public static final String HC = "Honeycomb";
	public static final String IC = "Ice Cream Sandwich";
	public static final String JB = "JellyBean";
	public static final String KK = "KitKat";
	public static final String NF = "Not Found";

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

		String platform = readOutput("/system/bin/getprop ro.board.platform");
		if (platform != null && !platform.equals(null)) {
			Toast.makeText(parentActivity,
					parentActivity.getString(R.string.platform, platform),
					Toast.LENGTH_SHORT).show();
			if (platform.contains("msm")) {
				qualcomm = true;
			}
			if (platform.contains("tegra")) {
				tegra = true;
			}
		}

		try {
			File config = new File(home_directory, "emu.cfg");
			if (config.exists()) {
				Scanner scanner = new Scanner(config);
				String currentLine;
				while (scanner.hasNextLine()) {
					currentLine = scanner.nextLine();

					// Check if the existing emu.cfg has the setting and get
					// current value

					if (StringUtils.containsIgnoreCase(currentLine,
							"Dynarec.Enabled")) {
						dynarecopt = Boolean.valueOf(currentLine.replace(
								"Dynarec.Enabled=", ""));
					}
					if (StringUtils.containsIgnoreCase(currentLine,
							"Dynarec.unstable-opt")) {
						unstableopt = Boolean.valueOf(currentLine.replace(
								"Dynarec.unstable-opt=", ""));
					}
					if (StringUtils.containsIgnoreCase(currentLine,
							"Dreamcast.Region")) {
						dcregion = Integer.valueOf(currentLine.replace(
								"Dreamcast.Region=", ""));
					}
					if (StringUtils.containsIgnoreCase(currentLine,
							"aica.LimitFPS")) {
						limitfps = Boolean.valueOf(currentLine.replace(
								"aica.LimitFPS=", ""));
					}
					if (StringUtils.containsIgnoreCase(currentLine,
							"rend.UseMipmaps")) {
						mipmaps = Boolean.valueOf(currentLine.replace(
								"rend.UseMipmaps=", ""));
					}
					if (StringUtils.containsIgnoreCase(currentLine,
							"rend.WideScreen")) {
						widescreen = Boolean.valueOf(currentLine.replace(
								"rend.WideScreen=", ""));
					}
					if (StringUtils.containsIgnoreCase(currentLine, "ta.skip")) {
						frameskip = Integer.valueOf(currentLine.replace(
								"ta.skip=", ""));
					}
					if (StringUtils.containsIgnoreCase(currentLine, "pvr.rend")) {
						pvrrender = Boolean.valueOf(currentLine.replace(
								"pvr.rend=", ""));
					}
					if (StringUtils.containsIgnoreCase(currentLine, "image")) {
						cheatdisk = currentLine.replace("image=", "");
					}

				}
				scanner.close();
			}
		} catch (Exception e) {
			Log.d("reicast", "Exception: " + e);
		}

		// Generate the menu options and fill in existing settings

		OnCheckedChangeListener dynarec_options = new OnCheckedChangeListener() {

			public void onCheckedChanged(CompoundButton buttonView,
					boolean isChecked) {
				mPrefs.edit().putBoolean("dynarec_opt", isChecked).commit();
				dynarecopt = isChecked;
				if (!executeAppendConfig("Dynarec.Enabled",
						String.valueOf(isChecked ? 1 : 0))) {
					executeWriteConfig();
				}
			}
		};
		Switch dynarec_opt = (Switch) getView().findViewById(
				R.id.dynarec_option);
		boolean dynarec = mPrefs.getBoolean("dynarec_opt", dynarecopt);
		if (dynarec) {
			dynarec_opt.setChecked(true);
		} else {
			dynarec_opt.setChecked(false);
		}
		dynarec_opt.setOnCheckedChangeListener(dynarec_options);

		OnCheckedChangeListener unstable_option = new OnCheckedChangeListener() {

			public void onCheckedChanged(CompoundButton buttonView,
					boolean isChecked) {
				mPrefs.edit().putBoolean("unstable_opt", isChecked).commit();
				unstableopt = isChecked;
				if (!executeAppendConfig("Dynarec.unstable-opt",
						String.valueOf(isChecked ? 1 : 0))) {
					executeWriteConfig();
				}
			}
		};
		Switch unstable_opt = (Switch) getView().findViewById(
				R.id.unstable_option);
		boolean unstable = mPrefs.getBoolean("unstable_opt", unstableopt);
		if (unstable) {
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

		int dc_region = mPrefs.getInt("dc_region", dcregion);
		region_spnr.setSelection(dc_region, true);

		region_spnr.setOnItemSelectedListener(new OnItemSelectedListener() {

			public void onItemSelected(AdapterView<?> parent, View view,
					int pos, long id) {
				mPrefs.edit().putInt("dc_region", pos).commit();
				dcregion = pos;
				if (!executeAppendConfig("Dreamcast.Region",
						String.valueOf(pos))) {
					executeWriteConfig();
				}

			}

			public void onNothingSelected(AdapterView<?> arg0) {
				mPrefs.edit().putInt("dc_region", dcregion).commit();
			}

		});

		OnCheckedChangeListener limitfps_option = new OnCheckedChangeListener() {

			public void onCheckedChanged(CompoundButton buttonView,
					boolean isChecked) {
				mPrefs.edit().putBoolean("limit_fps", isChecked).commit();
				limitfps = isChecked;
				if (!executeAppendConfig("aica.LimitFPS",
						String.valueOf(isChecked ? 1 : 0))) {
					executeWriteConfig();
				}
			}
		};
		Switch limit_fps = (Switch) getView()
				.findViewById(R.id.limitfps_option);
		boolean limited = mPrefs.getBoolean("limit_fps", limitfps);
		if (limited) {
			limit_fps.setChecked(true);
		} else {
			limit_fps.setChecked(false);
		}
		limit_fps.setOnCheckedChangeListener(limitfps_option);

		OnCheckedChangeListener mipmaps_option = new OnCheckedChangeListener() {

			public void onCheckedChanged(CompoundButton buttonView,
					boolean isChecked) {
				mPrefs.edit().putBoolean("use_mipmaps", isChecked).commit();
				mipmaps = isChecked;
				if (!executeAppendConfig("rend.UseMipmaps",
						String.valueOf(isChecked ? 1 : 0))) {
					executeWriteConfig();
				}
			}
		};
		Switch mipmap_opt = (Switch) getView()
				.findViewById(R.id.mipmaps_option);
		boolean mipmapped = mPrefs.getBoolean("use_mipmaps", mipmaps);
		if (mipmapped) {
			mipmap_opt.setChecked(true);
		} else {
			mipmap_opt.setChecked(false);
		}
		mipmap_opt.setOnCheckedChangeListener(mipmaps_option);

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
		Switch stretch_view = (Switch) getView().findViewById(
				R.id.stretch_option);
		boolean stretched = mPrefs.getBoolean("stretch_view", widescreen);
		if (stretched) {
			stretch_view.setChecked(true);
		} else {
			stretch_view.setChecked(false);
		}
		stretch_view.setOnCheckedChangeListener(full_screen);

		mainFrames = (TextView) getView().findViewById(R.id.current_frames);
		mainFrames.setText(String.valueOf(frameskip));

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
				mPrefs.edit().putBoolean("pvr_render", isChecked).commit();
				pvrrender = isChecked;
				if (!executeAppendConfig("pvr.rend",
						String.valueOf(isChecked ? 1 : 0))) {
					executeWriteConfig();
				}
			}
		};
		Switch pvr_render = (Switch) getView().findViewById(R.id.render_option);
		boolean rendered = mPrefs.getBoolean("pvr_render", pvrrender);
		if (rendered) {
			pvr_render.setChecked(true);
		} else {
			pvr_render.setChecked(false);
		}
		pvr_render.setOnCheckedChangeListener(pvr_rendering);

		final EditText cheatEdit = (EditText) getView().findViewById(
				R.id.cheat_disk);
		cheatEdit.setText(cheatdisk);

		cheatEdit.addTextChangedListener(new TextWatcher() {
			public void afterTextChanged(Editable s) {
				if (cheatEdit.getText() != null) {
					cheatdisk = cheatEdit.getText().toString();
					mPrefs.edit().putString("cheat_disk", cheatdisk).commit();
					if (!executeAppendConfig("image", cheatdisk)) {
						executeWriteConfig();
					}
				}
			}

			public void beforeTextChanged(CharSequence s, int start, int count,
					int after) {
			}

			public void onTextChanged(CharSequence s, int start, int before,
					int count) {
			}
		});

		Button debug_button = (Button) getView()
				.findViewById(R.id.debug_button);
		debug_button.setOnClickListener(new View.OnClickListener() {
			public void onClick(View v) {
				generateErrorLog();
			}
		});
	}

	public void generateErrorLog() {
		String currentTime = String.valueOf(System.currentTimeMillis());
		String logOuput = home_directory + "/" + currentTime + ".txt";
		Process mLogcatProc = null;
		BufferedReader reader = null;
		try {
			mLogcatProc = Runtime.getRuntime().exec(
					new String[] { "logcat", "-d", "AndroidRuntime:E *:S" });
			reader = new BufferedReader(new InputStreamReader(
					mLogcatProc.getInputStream()));
			String line;
			final StringBuilder log = new StringBuilder();
			String separator = System.getProperty("line.separator");
			log.append(discoverCPUData());
			log.append(separator);
			log.append(separator);
			log.append("AndroidRuntime Output");
			log.append(separator);
			log.append(separator);
			while ((line = reader.readLine()) != null) {
				log.append(line);
				log.append(separator);
			}
			reader.close();
			mLogcatProc = null;
			reader = null;
			int PID = android.os.Process.getUidForName("com.reicast.emulator");
			mLogcatProc = Runtime.getRuntime().exec(
					new String[] { "logcat", "-d", "|", "grep " + PID });
			reader = new BufferedReader(new InputStreamReader(
					mLogcatProc.getInputStream()));
			log.append(separator);
			log.append(separator);
			log.append("Application ID Output");
			log.append(separator);
			log.append(separator);
			while ((line = reader.readLine()) != null) {
				log.append(line);
				log.append(separator);
			}
			reader.close();
			mLogcatProc = null;
			reader = null;
			mLogcatProc = Runtime.getRuntime().exec(
					new String[] { "logcat", "-d", "reidc:V *:S" });
			reader = new BufferedReader(new InputStreamReader(
					mLogcatProc.getInputStream()));
			log.append(separator);
			log.append(separator);
			log.append("Native Interface Output");
			log.append(separator);
			log.append(separator);
			while ((line = reader.readLine()) != null) {
				log.append(line);
				log.append(separator);
			}
			reader.close();
			mLogcatProc = null;
			reader = null;
			mLogcatProc = Runtime.getRuntime().exec(
					new String[] { "logcat", "-d", "newdc:V *:S" });
			reader = new BufferedReader(new InputStreamReader(
					mLogcatProc.getInputStream()));
			log.append(separator);
			log.append(separator);
			log.append("Native Library Output");
			log.append(separator);
			log.append(separator);
			while ((line = reader.readLine()) != null) {
				log.append(line);
				log.append(separator);
			}
			reader.close();
			reader = null;
			File file = new File(logOuput);
			BufferedWriter writer = new BufferedWriter(new FileWriter(file));
			writer.write(log.toString());
			writer.flush();
			writer.close();
		} catch (IOException e) {

		}
	}

	private String discoverCPUData() {
		String s = "MODEL: " + Build.MODEL;
		s += "\r\n";
		s += "DEVICE: " + build_device;
		s += "\r\n";
		s += "BOARD: " + build_board;
		s += "\r\n";
		if (String.valueOf(build_sdk) != null) {
			String build_version = NF;
			if (build_sdk >= 4 && build_sdk < 7) {
				build_version = DN;
			} else if (build_sdk == 7) {
				build_version = EC;
			} else if (build_sdk == 8) {
				build_version = FR;
			} else if (build_sdk >= 9 && build_sdk < 11) {
				build_version = GB;
			} else if (build_sdk >= 11 && build_sdk < 14) {
				build_version = HC;
			} else if (build_sdk >= 14 && build_sdk < 16) {
				build_version = IC;
			} else if (build_sdk >= 16 && build_sdk < 17) {
				build_version = JB;
			} else if (build_sdk >= 17) {
				build_version = KK;
			}
			s += build_version + " (" + build_sdk + ")";
		} else {
			String prop_build_version = "ro.build.version.release";
			String prop_sdk_version = "ro.build.version.sdk";
			String build_version = readOutput("/system/bin/getprop "
					+ prop_build_version);
			String sdk_version = readOutput("/system/bin/getprop "
					+ prop_sdk_version);
			s += build_version + " (" + sdk_version + ")";
		}
		return s;
	}

	public static String readOutput(String command) {
		try {
			Process p = Runtime.getRuntime().exec(command);
			InputStream is = null;
			if (p.waitFor() == 0) {
				is = p.getInputStream();
			} else {
				is = p.getErrorStream();
			}
			BufferedReader br = new BufferedReader(new InputStreamReader(is),
					2048);
			String line = br.readLine();
			br.close();
			return line;
		} catch (Exception ex) {
			return "ERROR: " + ex.getMessage();
		}
	}

	private boolean executeAppendConfig(String identifier, String value) {
		File config = new File(home_directory, "emu.cfg");
		if (config.exists()) {
			try {

				// Read existing emu.cfg and substitute new setting value

				StringBuilder rebuildFile = new StringBuilder();
				Scanner scanner = new Scanner(config);
				String currentLine;
				while (scanner.hasNextLine()) {
					currentLine = scanner.nextLine();
					if (StringUtils.containsIgnoreCase(currentLine, identifier)) {
						rebuildFile.append(identifier + "=" + value + "\n");
					} else if (StringUtils.containsIgnoreCase(currentLine,
							"Dreamcast.RTC")) {
						rebuildFile.append("Dreamcast.RTC="
								+ String.valueOf(System.currentTimeMillis())
								+ "\n");
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

			// Write new emu.cfg using current display values

			StringBuilder rebuildFile = new StringBuilder();
			rebuildFile.append("[config]" + "\n");
			rebuildFile.append("Dynarec.Enabled="
					+ String.valueOf(dynarecopt ? 1 : 0) + "\n");
			rebuildFile.append("Dynarec.idleskip=1" + "\n");
			rebuildFile.append("Dynarec.unstable-opt="
					+ String.valueOf(unstableopt ? 1 : 0) + "\n");
			rebuildFile.append("Dreamcast.Cable=3" + "\n");
			rebuildFile.append("Dreamcast.RTC="
					+ String.valueOf(System.currentTimeMillis()) + "\n");
			rebuildFile.append("Dreamcast.Region=" + String.valueOf(dcregion)
					+ "\n");
			rebuildFile.append("Dreamcast.Broadcast=4" + "\n");
			rebuildFile.append("aica.LimitFPS="
					+ String.valueOf(limitfps ? 1 : 0) + "\n");
			rebuildFile.append("aica.NoBatch=0" + "\n");
			rebuildFile.append("rend.UseMipmaps="
					+ String.valueOf(mipmaps ? 1 : 0) + "\n");
			rebuildFile.append("rend.WideScreen="
					+ String.valueOf(widescreen ? 1 : 0) + "\n");
			rebuildFile.append("pvr.Subdivide=0" + "\n");
			rebuildFile.append("ta.skip=" + String.valueOf(frameskip) + "\n");
			rebuildFile.append("pvr.rend=" + String.valueOf(pvrrender ? 1 : 0)
					+ "\n");
			rebuildFile.append("image=" + cheatdisk + "\n");
			FileOutputStream fos = new FileOutputStream(config);
			fos.write(rebuildFile.toString().getBytes());
			fos.close();
		} catch (Exception e) {
			Log.d("reicast", "Exception: " + e);
		}
	}
}
