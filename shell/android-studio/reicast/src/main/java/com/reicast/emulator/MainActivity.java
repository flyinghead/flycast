package com.reicast.emulator;

import android.Manifest;
import android.app.AlertDialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.preference.PreferenceManager;
import android.support.annotation.NonNull;
import android.support.v4.app.ActivityCompat;
import android.support.v7.app.AppCompatActivity;
import android.view.View;
import android.view.View.OnSystemUiVisibilityChangeListener;
import android.view.WindowManager;

import com.reicast.emulator.debug.GenerateLogs;
import com.reicast.emulator.emu.JNIdc;

import java.lang.Thread.UncaughtExceptionHandler;
import java.util.List;

public class MainActivity extends AppCompatActivity {
	private static final int PERMISSION_REQUEST = 1001;

	private boolean hasAndroidMarket = false;
	private boolean renderer_started = false;
	private Uri gameUri;

	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		setContentView(R.layout.activity_main);

		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT) {
			getWindow().getDecorView().setOnSystemUiVisibilityChangeListener (new OnSystemUiVisibilityChangeListener() {
				public void onSystemUiVisibilityChange(int visibility) {
					if ((visibility & View.SYSTEM_UI_FLAG_FULLSCREEN) == 0) {
						getWindow().getDecorView().setSystemUiVisibility(
								View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
										| View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
										| View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
										| View.SYSTEM_UI_FLAG_FULLSCREEN
										| View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY);
					}
				}
			});
		} else {
			getWindow().setFlags (WindowManager.LayoutParams.FLAG_FULLSCREEN,
					WindowManager.LayoutParams.FLAG_FULLSCREEN);
		}

		final SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(this);

		String prior_error = prefs.getString("prior_error", null);
		if (prior_error != null) {
			displayLogOutput(prior_error);
			prefs.edit().remove("prior_error").apply();
		}
		UncaughtExceptionHandler mUEHandler = new Thread.UncaughtExceptionHandler() {
			public void uncaughtException(Thread t, Throwable error) {
				if (error != null) {
					StringBuilder output = new StringBuilder();
					for (StackTraceElement trace : error.getStackTrace()) {
						output.append(trace.toString());
						output.append("\n");
					}
					prefs.edit().putString("prior_error", output.toString()).apply();
					error.printStackTrace();
					android.os.Process.killProcess(android.os.Process.myPid());
					System.exit(0);
				}
			}
		};
		Thread.setDefaultUncaughtExceptionHandler(mUEHandler);

		Intent market = new Intent(Intent.ACTION_VIEW, Uri.parse("market://search?q=dummy"));
		if (isCallable(market)) {
			hasAndroidMarket = true;
		}

		if (!getFilesDir().exists()) {
			getFilesDir().mkdir();
		}

		// When viewing a resource, pass its URI to the native code for opening
		Intent intent = getIntent();
		if (intent.getAction() != null) {
			if (intent.getAction().equals(Intent.ACTION_VIEW)) {
				gameUri = Uri.parse(intent.getData().toString());
				// Flush the intent to prevent multiple calls
				getIntent().setData(null);
				setIntent(null);
			}
		}

		if (prior_error == null) {
			if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
				ActivityCompat.requestPermissions(MainActivity.this,
						new String[]{
								Manifest.permission.READ_EXTERNAL_STORAGE,
								Manifest.permission.WRITE_EXTERNAL_STORAGE
						},
						PERMISSION_REQUEST);
			} else
				startNativeRenderer();
		}
	}

	public void generateErrorLog() {
		new GenerateLogs(MainActivity.this).execute(getFilesDir().getAbsolutePath());
	}

	/**
	 * Display a dialog to notify the user of prior crash
	 *
	 * @param error
	 *            A generalized summary of the crash cause
	 */
	private void displayLogOutput(final String error) {
		AlertDialog.Builder builder = new AlertDialog.Builder(MainActivity.this);
		builder.setTitle(R.string.report_issue);
		builder.setMessage(error);
		builder.setPositiveButton(R.string.report,
				new DialogInterface.OnClickListener() {
					public void onClick(DialogInterface dialog, int which) {
						generateErrorLog();
						startNativeRenderer();
					}
				});
		builder.setNegativeButton(R.string.dismiss,
				new DialogInterface.OnClickListener() {
					public void onClick(DialogInterface dialog, int which) {
						dialog.dismiss();
						startNativeRenderer();
					}
				});
		builder.create();
		builder.show();
	}

	private void startNativeRenderer() {
		if (renderer_started)
			return;
		renderer_started = true;

		if (gameUri != null && Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
			gameUri = Uri.parse(gameUri.toString().replace("content://"
					+ gameUri.getAuthority() + "/external_files", "/storage"));
		}

		if (gameUri != null)
			JNIdc.setGameUri(gameUri.toString());
		Intent intent = new Intent("com.reicast.EMULATOR",
				//gameUri, getApplicationContext(), GL2JNIActivity.class);
				gameUri, getApplicationContext(), NativeGLActivity.class);
		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N)
			intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
		intent.addFlags(Intent.FLAG_ACTIVITY_NO_ANIMATION);
		startActivity(intent);
	}

	@Override
	public void onWindowFocusChanged(boolean hasFocus) {
		super.onWindowFocusChanged(hasFocus);
		if (hasFocus && Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT) {
			getWindow().getDecorView().setSystemUiVisibility(
					View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
							| View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
							| View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
							| View.SYSTEM_UI_FLAG_FULLSCREEN
							| View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY);
		}
	}

	private boolean isCallable(Intent intent) {
		List<ResolveInfo> list = getPackageManager().queryIntentActivities(
				intent, PackageManager.MATCH_DEFAULT_ONLY);
		return list.size() > 0;
	}

	@Override
	public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions, @NonNull int[] grantResults) {
		super.onRequestPermissionsResult(requestCode, permissions, grantResults);
		if (permissions.length > 0 && (Manifest.permission.READ_EXTERNAL_STORAGE.equals(permissions[0]) || Manifest.permission.WRITE_EXTERNAL_STORAGE.equals(permissions[0]))
				&& grantResults[0] == PackageManager.PERMISSION_GRANTED) {
			startNativeRenderer();
		}
	}

}
