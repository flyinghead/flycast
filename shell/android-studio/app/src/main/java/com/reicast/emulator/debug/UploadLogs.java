package com.reicast.emulator.debug;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.AsyncTask;

import com.reicast.emulator.config.Config;

import java.io.BufferedInputStream;
import java.io.BufferedWriter;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.OutputStreamWriter;
import java.io.UnsupportedEncodingException;
import java.net.HttpURLConnection;
import java.net.MalformedURLException;
import java.net.URL;
import java.net.URLEncoder;
import java.util.AbstractMap;
import java.util.ArrayList;
import java.util.List;

import javax.net.ssl.HttpsURLConnection;

/**
 * Upload the specialized logcat to reicast issues
 */
public class UploadLogs extends AsyncTask<String, Integer, Object> {

	private String currentTime;
	private String logUrl;
	private Context mContext;

	/**
	 * @param mContext
	 *            The context this method will be executed from
	 * @param currentTime
	 *            The system time at which the log was made
	 */
	public UploadLogs(Context mContext, String currentTime) {
		this.mContext = mContext;
		this.currentTime = currentTime;
	}
	
	private void RedirectSubmission(String header, String content) {
		UploadLogs mUploadLogs = new UploadLogs(mContext, currentTime);
		mUploadLogs.setPostUrl(header);
		mUploadLogs.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR, content);
	}
	
	/**
	 * Set the URL for where the log will be uploaded
	 * 
	 * @param logUrl
	 *            The URL of the log upload server
	 */
	public void setPostUrl(String logUrl) {
		this.logUrl = logUrl;
	}

	@SuppressLint("NewApi")
	protected void onPreExecute() {
		if (logUrl == null || logUrl.equals(null)) {
			logUrl = Config.log_url;
		}
	}

	private String getQuery(List<AbstractMap.SimpleEntry> params)
			throws UnsupportedEncodingException {
		StringBuilder result = new StringBuilder();
		boolean first = true;

		for (AbstractMap.SimpleEntry pair : params) {
			if (first)
				first = false;
			else
				result.append("&");

			result.append(URLEncoder.encode((String) pair.getKey(), "UTF-8"));
			result.append("=");
			result.append(URLEncoder.encode((String) pair.getValue(), "UTF-8"));
		}
		return result.toString();
	}

	@Override
	protected Object doInBackground(String... params) {
		HttpURLConnection conn = null;
		try {
			conn = (HttpURLConnection) new URL(logUrl).openConnection();
			conn.setRequestMethod("POST");
			conn.setDoInput(true);
			conn.setDoOutput(true);

			ArrayList<AbstractMap.SimpleEntry> mPairs = new ArrayList<>();
			mPairs.add(new AbstractMap.SimpleEntry("name", currentTime));
			mPairs.add(new AbstractMap.SimpleEntry("issue", params[0]));

			OutputStream os = conn.getOutputStream();
			BufferedWriter writer = new BufferedWriter(new OutputStreamWriter(os, "UTF-8"));
			writer.write(getQuery(mPairs));
			writer.flush();
			writer.close();
			os.close();

			int responseCode=conn.getResponseCode();

			if (responseCode != HttpsURLConnection.HTTP_OK) {
				String header = conn.getHeaderField("Location");
				if (header != null && header.length() != 0) {
					RedirectSubmission(header, params[0]);
				} else {
					return null;
				}
			} else {
				InputStream is = new BufferedInputStream(conn.getInputStream());
				ByteArrayOutputStream result = new ByteArrayOutputStream();
				byte[] buffer = new byte[1024];
				int length;
				while ((length = is.read(buffer)) != -1) {
					result.write(buffer, 0, length);
				}
				return result.toString("UTF-8");
			}
		} catch (MalformedURLException e) {
			e.printStackTrace();
			if (conn != null) conn.disconnect();
		} catch (IOException e) {
			e.printStackTrace();
			if (conn != null) conn.disconnect();
		} catch (Exception e) {
			e.printStackTrace();
			if (conn != null) conn.disconnect();
		}
		return null;
	}

	@SuppressWarnings("deprecation")
	@Override
	protected void onPostExecute(Object response) {
		if (response != null && !response.equals(null)) {
			String logLink = Config.report_url + currentTime + ".txt";
			android.content.ClipboardManager clipboard = (android.content.ClipboardManager) mContext
					.getSystemService(Context.CLIPBOARD_SERVICE);
			android.content.ClipData clip = android.content.ClipData.newPlainText("logcat", logLink);
			clipboard.setPrimaryClip(clip);
			Intent browserIntent = new Intent(Intent.ACTION_VIEW, Uri.parse(Config.git_issues));
			mContext.startActivity(browserIntent);
		}
	}
}
