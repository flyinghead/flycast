package com.reicast.emulator.debug;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.MalformedURLException;
import java.net.URL;

import android.app.Activity;
import android.app.Notification;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.AsyncTask;
import android.os.Build;
import android.os.Bundle;
import android.support.v4.app.NotificationCompat;
import android.util.Log;
import android.widget.RemoteViews;
import android.widget.Toast;

public class GitHash extends Activity {

	private Context mContext;

	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		mContext = this.getApplicationContext();
		Bundle extras = getIntent().getExtras();
		if (extras != null) {
			String hash = extras.getString("hashtag");
			NetworkHandler mDownload = new NetworkHandler(mContext);
			if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.HONEYCOMB) {
				mDownload.executeOnExecutor(
						AsyncTask.THREAD_POOL_EXECUTOR, hash);
			} else {
				mDownload.execute(hash);
			}
		}
	}
	
	public class NetworkHandler extends AsyncTask<String, Integer, File> {

		private Context mContext;
		private NotificationManager mNM;
		private Notification notification;
		private int progress = 0;
		private String hash;

		short timestamp = (short) System.currentTimeMillis();

		NetworkHandler(Context mContext) {
			this.mContext = mContext;
			mNM = (NotificationManager) mContext
					.getSystemService(NOTIFICATION_SERVICE);
		}

		@Override
		protected File doInBackground(String... paths) {
			String apk = "reicast-emulator-" + paths[0] + ".apk";
			String file = "http://twisted.dyndns.tv:3194/ReicastBot/compiled/" + apk;
			File SDCard = mContext.getExternalCacheDir();
			File apkFile = new File(SDCard, apk);
			try {
				URL url = new URL(file);
				HttpURLConnection urlConnection = (HttpURLConnection) url
						.openConnection();
				HttpURLConnection.setFollowRedirects(true);
				urlConnection.setRequestMethod("GET");
				urlConnection.setDoOutput(true);
				urlConnection.connect();
				FileOutputStream fileOutput = new FileOutputStream(apkFile);
				InputStream inputStream = urlConnection.getInputStream();
				int totalSize = urlConnection.getContentLength();
				int downloadedSize = 0;
				byte[] buffer = new byte[1024];
				int bufferLength = 0;
				int priorProgress = 0;
				while ((bufferLength = inputStream.read(buffer)) > 0) {
					downloadedSize += bufferLength;
					int currentSize = (int) (downloadedSize * 100 / totalSize);
					if (currentSize > priorProgress) {
						priorProgress = (int) (downloadedSize * 100 / totalSize);
						publishProgress(currentSize);
					}
					fileOutput.write(buffer, 0, bufferLength);
				}
				fileOutput.close();
				inputStream.close();
				return apkFile.getAbsoluteFile();
			} catch (MalformedURLException e) {
				Log.d(Debug.APP_TAG, "MalformedURLException: " + e.getMessage());
			} catch (IOException e) {
				Log.d(Debug.APP_TAG, "IOException: " + e.getMessage());
			} catch (Exception e) {
				Log.d(Debug.APP_TAG, "Exception: " + e.getMessage());
			}
			return null;
		}

		@Override
		protected void onProgressUpdate(Integer... progress) {
			notification.contentView.setProgressBar(R.id.status_progress, 100,
					progress[0], false);
			mNM.notify(timestamp, notification);
		}

		@Override
		protected void onPreExecute() {
			Intent intent = new Intent(mContext, GitHash.class);
			final PendingIntent pendingIntent = PendingIntent.getActivity(
					mContext.getApplicationContext(), 0, intent, 0);
			NotificationCompat.Builder builder = new NotificationCompat.Builder(
					mContext.getApplicationContext());
			builder.setContentTitle("reicast " + hash)
					.setSmallIcon(R.drawable.ic_launcher)
					.setAutoCancel(false).setOngoing(true)
					.setPriority(Notification.PRIORITY_HIGH);
			notification = builder.build();
			notification.flags |= Notification.FLAG_NO_CLEAR;
			notification.contentView = new RemoteViews(mContext
					.getApplicationContext().getPackageName(),
					R.layout.download_prog);
			notification.contentIntent = pendingIntent;
			notification.contentView.setImageViewResource(R.id.status_icon,
					R.drawable.ic_launcher);
			notification.contentView.setTextViewText(R.id.status_text, "Downloading " + hash + "...");
			notification.contentView.setProgressBar(R.id.status_progress, 100,
					progress, false);
			mNM.notify(timestamp, notification);
			// Use a resourceId as an unique identifier
		}

		@Override
		protected void onPostExecute(File download) {
			mNM.cancel(timestamp);
			if (download != null) {
				System.gc();
				Intent installIntent;
				if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.ICE_CREAM_SANDWICH) {
					installIntent = new Intent(Intent.ACTION_INSTALL_PACKAGE);
					installIntent.setData(Uri.fromFile(download));
					installIntent.putExtra(Intent.EXTRA_NOT_UNKNOWN_SOURCE,
							true);
					installIntent.putExtra(Intent.EXTRA_RETURN_RESULT, true);
					installIntent.putExtra(Intent.EXTRA_ALLOW_REPLACE, true);
					installIntent.putExtra(Intent.EXTRA_INSTALLER_PACKAGE_NAME,
							getApplicationInfo().packageName);
				} else {
					installIntent = new Intent(Intent.ACTION_VIEW);
					installIntent.setDataAndType(Uri.fromFile(download),
							"application/vnd.android.package-archive");
				}
				startActivityForResult(installIntent, 0);
			} else {
				Toast.makeText(mContext, "Download Unavailable!", Toast.LENGTH_SHORT).show();
				finish();
			}
		}
	}

	@Override
	protected void onActivityResult(int requestCode, int resultCode, Intent data) {
		super.onActivityResult(requestCode, resultCode, data);
		finish();
	}

	@Override
	protected void onPause() {
		super.onPause();
	}

	@Override
	protected void onDestroy() {
		super.onDestroy();
	}

	@Override
	protected void onResume() {
		super.onResume();
	}
}
