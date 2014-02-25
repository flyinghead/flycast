package com.reicast.emulator.debug;

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileWriter;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;

import android.content.Context;
import android.os.AsyncTask;
import android.os.Build;
import android.os.Handler;
import android.widget.Toast;

import com.reicast.emulator.R;

public class GenerateLogs extends AsyncTask<String, Integer, String> {

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

	private String unHandledIOE;

	private Context mContext;
	private String currentTime;

	public GenerateLogs(Context mContext) {
		this.mContext = mContext;
		this.currentTime = String.valueOf(System.currentTimeMillis());
	}

	/**
	 * Obtain the specific parameters of the current device
	 * 
	 */
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
			} else if (build_sdk >= 16 && build_sdk < 19) {
				build_version = JB;
			} else if (build_sdk >= 19) {
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

	/**
	 * Read the output of a shell command
	 * 
	 * @param string
	 *            The shell command being issued to the terminal
	 */
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

	public void setUnhandled(String unHandledIOE) {
		this.unHandledIOE = unHandledIOE;
	}

	@Override
	protected String doInBackground(String... params) {
		File logFile = new File(params[0], currentTime + ".txt");
		Process mLogcatProc = null;
		BufferedReader reader = null;
		final StringBuilder log = new StringBuilder();
		String separator = System.getProperty("line.separator");
		log.append(discoverCPUData());
		if (unHandledIOE != null) {
			log.append(separator);
			log.append(separator);
			log.append("Unhandled Exceptions");
			log.append(separator);
			log.append(separator);
			log.append(unHandledIOE);
		}
		try {
			mLogcatProc = Runtime.getRuntime().exec(
					new String[] { "logcat", "-d", "AndroidRuntime:E *:S" });
			reader = new BufferedReader(new InputStreamReader(
					mLogcatProc.getInputStream()));
			String line;
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
			log.append("Application Core Output");
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
					new String[] { "logcat", "-d", "GL3JNIView:E *:S" });
			reader = new BufferedReader(new InputStreamReader(
					mLogcatProc.getInputStream()));
			log.append(separator);
			log.append(separator);
			log.append("Open GLES View Output");
			log.append(separator);
			log.append(separator);
			while ((line = reader.readLine()) != null) {
				log.append(line);
				log.append(separator);
			}
			reader.close();
			mLogcatProc = null;
			reader = null;
			File memory = new File(mContext.getFilesDir(), "mem_alloc.txt");
			if (memory.exists()) {
				log.append(separator);
				log.append(separator);
				log.append("Memory Allocation Table");
				log.append(separator);
				log.append(separator);
				FileInputStream fis = new FileInputStream(memory);
				reader = new BufferedReader(new InputStreamReader(fis));
				while ((line = reader.readLine()) != null) {
					log.append(line);
					log.append(separator);
				}
				fis.close();
				fis = null;
				reader.close();
				reader = null;
			}
			BufferedWriter writer = new BufferedWriter(new FileWriter(logFile));
			writer.write(log.toString());
			writer.flush();
			writer.close();
			return log.toString();
		} catch (IOException e) {

		}
		return null;
	}

	@Override
	protected void onPostExecute(final String response) {
		if (response != null && !response.equals(null)) {
			Toast.makeText(mContext, mContext.getString(R.string.log_saved),
					Toast.LENGTH_SHORT).show();
			Toast.makeText(mContext, mContext.getString(R.string.platform),
					Toast.LENGTH_SHORT).show();
			UploadLogs mUploadLogs = new UploadLogs(mContext, currentTime);
			if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.HONEYCOMB) {
				mUploadLogs.executeOnExecutor(
						AsyncTask.THREAD_POOL_EXECUTOR, response);
			} else {
				mUploadLogs.execute(response);
			}
		}
	}
}
