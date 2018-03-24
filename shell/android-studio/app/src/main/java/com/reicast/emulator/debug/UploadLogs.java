package com.reicast.emulator.debug;

import android.annotation.SuppressLint;
import android.annotation.TargetApi;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.AsyncTask;
import android.os.Build;
import android.os.StrictMode;

import com.reicast.emulator.config.Config;

import org.apache.http.Header;
import org.apache.http.HttpResponse;
import org.apache.http.HttpStatus;
import org.apache.http.NameValuePair;
import org.apache.http.client.HttpClient;
import org.apache.http.client.entity.UrlEncodedFormEntity;
import org.apache.http.client.methods.HttpPost;
import org.apache.http.impl.client.DefaultHttpClient;
import org.apache.http.message.BasicNameValuePair;
import org.apache.http.util.EntityUtils;

import java.io.IOException;
import java.net.MalformedURLException;
import java.util.ArrayList;

/**
 * Upload the specialized logcat to reicast issues
 * 
 * @param mContext
 *            The context this method will be executed from
 * @param currentTime
 *            The system time at which the log was made
 */
public class UploadLogs extends AsyncTask<String, Integer, Object> {

	private String currentTime;
	private String logUrl;
	private Context mContext;

	public UploadLogs(Context mContext, String currentTime) {
		this.mContext = mContext;
		this.currentTime = currentTime;
	}
	
	private void RedirectSubmission(Header[] headers, String content) {
		UploadLogs mUploadLogs = new UploadLogs(mContext,
				currentTime);
		mUploadLogs.setPostUrl(headers[headers.length - 1]
				.getValue());
		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.HONEYCOMB) {
			mUploadLogs.executeOnExecutor(
					AsyncTask.THREAD_POOL_EXECUTOR, content);
		} else {
			mUploadLogs.execute(content);
		}
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
		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.GINGERBREAD) {
			StrictMode.ThreadPolicy policy = new StrictMode.ThreadPolicy.Builder()
					.permitAll().build();
			StrictMode.setThreadPolicy(policy);
		}
	}

	@TargetApi(Build.VERSION_CODES.HONEYCOMB)
	@Override
	protected Object doInBackground(String... params) {
		HttpClient client = new DefaultHttpClient();
		HttpPost post = new HttpPost(logUrl);
		try {
			ArrayList<NameValuePair> mPairs = new ArrayList<NameValuePair>();
			mPairs.add(new BasicNameValuePair("name", currentTime));
			mPairs.add(new BasicNameValuePair("issue", params[0]));
			post.setEntity(new UrlEncodedFormEntity(mPairs));
			HttpResponse getResponse = client.execute(post);
			final int statusCode = getResponse.getStatusLine().getStatusCode();
			if (statusCode != HttpStatus.SC_OK) {
				Header[] headers = getResponse.getHeaders("Location");
				if (headers != null && headers.length != 0) {
					RedirectSubmission(headers, params[0]);
				} else {
					return null;
				}
			} else {
				return EntityUtils.toString(getResponse.getEntity());
			}
		} catch (MalformedURLException e) {
			e.printStackTrace();
			post.abort();
		} catch (IOException e) {
			e.printStackTrace();
			post.abort();
		} catch (Exception e) {
			e.printStackTrace();
			post.abort();
		}
		return null;
	}

	@TargetApi(Build.VERSION_CODES.HONEYCOMB)
	@SuppressWarnings("deprecation")
	@Override
	protected void onPostExecute(Object response) {
		if (response != null && !response.equals(null)) {
			String logLink = Config.report_url + currentTime + ".txt";
			if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.HONEYCOMB) {
				android.content.ClipboardManager clipboard = (android.content.ClipboardManager) mContext
						.getSystemService(Context.CLIPBOARD_SERVICE);
				android.content.ClipData clip = android.content.ClipData
						.newPlainText("logcat", logLink);
				clipboard.setPrimaryClip(clip);
			} else {
				android.text.ClipboardManager clipboard = (android.text.ClipboardManager) mContext
						.getSystemService(Context.CLIPBOARD_SERVICE);
				clipboard.setText(logLink);
			}
			Intent browserIntent = new Intent(Intent.ACTION_VIEW,
					Uri.parse(Config.git_issues));
			mContext.startActivity(browserIntent);
		}
	}
}
