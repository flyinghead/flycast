package com.reicast.emulator.debug;

import java.io.IOException;
import java.net.MalformedURLException;
import java.util.ArrayList;

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

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.AsyncTask;
import android.os.Build;

import com.reicast.emulator.R;


public class UploadLogs extends AsyncTask<String, Integer, Object> {

	private String currentTime;
	private String logUrl;
	private Context mContext;

	public UploadLogs(Context mContext, String currentTime) {
		this.mContext = mContext;
		this.currentTime = currentTime;
	}

	public void setPostUrl(String logUrl) {
		this.logUrl = logUrl;
	}

	@SuppressLint("NewApi")
	protected void onPreExecute() {

	}

	@Override
	protected Object doInBackground(String... params) {
		HttpClient client = new DefaultHttpClient();
		if (logUrl == null || logUrl.equals(null)) {
			logUrl = "http://twisted.dyndns.tv:3194/ReicastBot/report/submit.php";
		}
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
					UploadLogs mUploadLogs = new UploadLogs(mContext,
							currentTime);
					mUploadLogs.setPostUrl(headers[headers.length - 1]
							.getValue());
					if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.HONEYCOMB) {
						mUploadLogs.executeOnExecutor(
								AsyncTask.THREAD_POOL_EXECUTOR, params[0]);
					} else {
						mUploadLogs.execute(params[0]);
					}
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

	@Override
	protected void onPostExecute(Object response) {
		if (response != null && !response.equals(null)) {
			String logLink = "http://reicast.loungekatt.com/report/logs/"
					+ currentTime + ".txt";
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
					Uri.parse(mContext.getString(R.string.git_issues)));
			mContext.startActivity(browserIntent);
		}
	}
}
