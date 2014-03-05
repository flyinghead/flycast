package com.reicast.emulator.debug;

import java.io.BufferedReader;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.InetAddress;
import java.net.MalformedURLException;
import java.net.NetworkInterface;
import java.net.ProtocolException;
import java.net.URL;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Map.Entry;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.regex.Pattern;

import org.apache.http.HttpEntity;
import org.apache.http.HttpResponse;
import org.apache.http.NameValuePair;
import org.apache.http.client.ClientProtocolException;
import org.apache.http.client.HttpClient;
import org.apache.http.client.ResponseHandler;
import org.apache.http.client.entity.UrlEncodedFormEntity;
import org.apache.http.client.methods.HttpPost;
import org.apache.http.conn.util.InetAddressUtils;
import org.apache.http.impl.client.DefaultHttpClient;
import org.apache.http.message.BasicNameValuePair;

import android.accounts.Account;
import android.accounts.AccountManager;
import android.app.Activity;
import android.app.AlertDialog;
import android.app.Dialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.SharedPreferences;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager.NameNotFoundException;
import android.content.res.Resources;
import android.os.AsyncTask;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.os.Handler;
import android.preference.PreferenceManager;
import android.util.Log;
import android.util.Patterns;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.widget.BaseAdapter;
import android.widget.Button;
import android.widget.CompoundButton;
import android.widget.CompoundButton.OnCheckedChangeListener;
import android.widget.EditText;
import android.widget.ListView;
import android.widget.TextView;

import com.google.android.gms.common.ConnectionResult;
import com.google.android.gms.common.GooglePlayServicesUtil;
import com.google.android.gms.gcm.GoogleCloudMessaging;

import de.ankri.views.Switch;

public class Debug extends Activity {

	public static final String APP_TAG = "reicast-debug";

	public static final String EXTRA_MESSAGE = "message";
	private static final String PROPERTY_APP_VERSION = "appVersion";
	private static final String PROPERTY_ON_SERVER_EXPIRATION_TIME = "onServerExpirationTimeMs";
	static final String SERVER_URL = "http://twisted.dyndns.tv:3194/ReicastBot/plugin/register.php";
	public static final long REGISTRATION_EXPIRY_TIME_MS = 1000 * 3600 * 24 * 30;
	String SENDER_ID = "847786358946";
	static final String TAG = "GCM::Service";
	GoogleCloudMessaging gcm;
	public static final String REG_ID = "registration_id";
	AtomicInteger msgId = new AtomicInteger();

	public static final String cloudUrl = "http://twisted.dyndns.tv:3194/ReicastBot/plugin/submit.php";
	public static final String numberUrl = "http://twisted.dyndns.tv:3194/ReicastBot/plugin/number.php";
	public static final String archiveUrl = "http://twisted.dyndns.tv:3194/ReicastBot/plugin/archive.php";

	public static final String PREF_IDENTITY = "pref_user_identity";

	private Context mContext;
	private SharedPreferences mPrefs;

	private File sdcard = Environment.getExternalStorageDirectory();

	private ExpandableListView exlist = new ExpandableListView();

	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		setContentView(R.layout.dialog_message);
		mContext = this.getApplicationContext();
		mPrefs = PreferenceManager.getDefaultSharedPreferences(mContext);
		int resultCode = GooglePlayServicesUtil
				.isGooglePlayServicesAvailable(mContext);
		if (resultCode == ConnectionResult.SUCCESS) {
			String regid = getRegistrationId(mContext);
			if (regid.length() == 0) {
				RegisterTask mRegisterTask = new RegisterTask();
				if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.HONEYCOMB) {
					mRegisterTask
							.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
				} else {
					mRegisterTask.execute();
				}
			}
			submitGcmUser(regid);
			gcm = GoogleCloudMessaging.getInstance(mContext);
		} else {
			GooglePlayServicesUtil.getErrorDialog(resultCode, this, 1).show();
		}

		OnCheckedChangeListener notify_options = new OnCheckedChangeListener() {

			public void onCheckedChanged(CompoundButton buttonView,
					boolean isChecked) {
				mPrefs.edit().putBoolean("enable_messaging", isChecked).commit();
			}
		};
		Switch notify_opt = (Switch) findViewById(R.id.notify_option);
		notify_opt.setChecked(mPrefs.getBoolean("enable_messaging", true));
		notify_opt.setOnCheckedChangeListener(notify_options);

		RequestArchive mRequestArchive = new RequestArchive() {
			@Override
			protected void onPostExecute(List<String[]> jsonArray) {
				if (jsonArray != null && !jsonArray.isEmpty()) {
					ListView wrapper = (ListView) findViewById(R.id.dialog_list);
					DisplayDialog dialog = new DisplayDialog(Debug.this);
					MessageAdapter adapter = new MessageAdapter(mContext,
							jsonArray, dialog);
					wrapper.setAdapter(adapter);
					adapter.notifyDataSetChanged();
					exlist.setListViewHeightBasedOnChildren(wrapper);
					wrapper.invalidate();
				}
			}
		};
		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.HONEYCOMB) {
			mRequestArchive.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR,
					archiveUrl);
		} else {
			mRequestArchive.execute(archiveUrl);
		}
		final EditText msgEntry = (EditText) findViewById(R.id.txt_message);
		Button submit = (Button) findViewById(R.id.txt_button);
		submit.setOnClickListener(new OnClickListener() {
			@Override
			public void onClick(View v) {
				if (msgEntry.getText() != null
						&& !msgEntry.getText().toString().equals("")) {
					String message = msgEntry.getText().toString();
					submitGcmBroadcast(message);
					msgEntry.setText("");
				}
			}

		});
		Button debug = (Button) findViewById(R.id.debug_button);
			debug.setOnClickListener(new View.OnClickListener() {
				public void onClick(View view) {
					generateErrorLog();
				}
			});
			try {
				Resources res = getPackageManager().getResourcesForApplication("com.reicast.emulator");
				InputStream file = res.getAssets().open("build");
				if (file != null) {
					BufferedReader reader = new BufferedReader(
							new InputStreamReader(file));
					Log.d("reicast-debug", "Hash: " + reader.readLine());
					file.close();
				}
			} catch (IOException ioe) {
				ioe.printStackTrace();
			} catch (NameNotFoundException e) {
				e.printStackTrace();
			}
	}

	public void generateErrorLog() {
		GenerateLogs mGenerateLogs = new GenerateLogs(Debug.this);
		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.HONEYCOMB) {
			mGenerateLogs.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR,
					sdcard.getAbsolutePath());
		} else {
			mGenerateLogs.execute(sdcard.getAbsolutePath());
		}

	}

	public class MessageAdapter extends BaseAdapter {
		@SuppressWarnings("unused")
		private Context mContext;
		private List<String[]> data;
		private DisplayDialog note;

		public MessageAdapter(Context c, List<String[]> d, DisplayDialog n) {
			mContext = c;
			data = d;
			note = n;
		}

		public int getCount() {
			return data.size();
		}

		public Object getItem(int position) {
			return position;
		}

		public long getItemId(int position) {
			return position;
		}

		public View getView(final int position, View convertView,
				ViewGroup parent) {
			System.gc();

			LayoutInflater inflater = (LayoutInflater) getSystemService(Context.LAYOUT_INFLATER_SERVICE);
			View vi = convertView;
			if (convertView == null)
				vi = inflater.inflate(R.layout.dialog_item, null);

			String[] message = data.get(position);
			final String title = message[0] + " [" + message[2] + "]";
			final String content = message[1];

			TextView itemName = (TextView) vi.findViewById(R.id.item_name);
			itemName.setText(title);

			TextView itemContent = (TextView) vi
					.findViewById(R.id.item_content);
			itemContent.setText(content);

			vi.setOnClickListener(new OnClickListener() {
				@Override
				public void onClick(View v) {
					new Handler().post(new Runnable() {
						public void run() {
							note.showMessage(title, content);
						}
					});
				}
			});

			return vi;
		}
	}

	public class DisplayDialog extends AlertDialog.Builder {

		public DisplayDialog(Context context) {
			super(context);
		}

		public void showMessage(String title, String content) {

			this.setTitle(title);
			this.setMessage(content);
			this.setNegativeButton("Dismiss",
					new DialogInterface.OnClickListener() {
						public void onClick(DialogInterface dialog, int which) {
							dialog.dismiss();
						}
					});
			this.setPositiveButton(null, null);
			this.setOnKeyListener(new Dialog.OnKeyListener() {
				public boolean onKey(DialogInterface dialog, int keyCode,
						KeyEvent event) {
					dialog.dismiss();
					return true;
				}
			});
			this.create();
			this.show();
		}
	}

	private String getRegistrationId(Context context) {
		String registrationId = mPrefs.getString(REG_ID, "");
		if (registrationId.length() == 0) {
			return "";
		}
		// check if app was updated; if so, it must clear registration id to
		// avoid a race condition if GCM sends a message
		int registeredVersion = mPrefs.getInt(PROPERTY_APP_VERSION,
				Integer.MIN_VALUE);
		int currentVersion = getAppVersion(context);
		if (registeredVersion != currentVersion || isRegistrationExpired()) {
			return "";
		}
		return registrationId;
	}

	private static int getAppVersion(Context context) {
		try {
			PackageInfo packageInfo = context.getPackageManager()
					.getPackageInfo(context.getPackageName(), 0);
			return packageInfo.versionCode;
		} catch (NameNotFoundException e) {
			// should never happen
			throw new RuntimeException("Could not get package name: " + e);
		}
	}

	private boolean isRegistrationExpired() {
		long expirationTime = mPrefs.getLong(
				PROPERTY_ON_SERVER_EXPIRATION_TIME, -1);
		return System.currentTimeMillis() > expirationTime;
	}

	public class RegisterTask extends AsyncTask<String, Integer, String> {
		@SuppressWarnings("unchecked")
		@Override
		protected String doInBackground(String... params) {
			try {
				if (gcm == null) {
					gcm = GoogleCloudMessaging.getInstance(mContext);
				}
				String regid = gcm.register(SENDER_ID);
				Log.d(APP_TAG, "Registration id: " + regid);
				setRegistrationId(mContext, regid);
				String possibleEmail = getAccountEmail();
				if (possibleEmail != null) {
					String serverUrl = SERVER_URL;
					Map<String, String> user = new HashMap<String, String>();
					user.put("regId", regid);
					user.put("email", possibleEmail);
					PostServer mPostServer = new PostServer(serverUrl);
					if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.HONEYCOMB) {
						mPostServer.executeOnExecutor(
								AsyncTask.THREAD_POOL_EXECUTOR, user);
					} else {
						mPostServer.execute(user);
					}
				}
				return regid;
			} catch (IOException ex) {
				Log.d(APP_TAG, "Error :" + ex.getMessage());
			}
			return null;
		}

		@Override
		protected void onPostExecute(String regid) {
			if (regid != null && !regid.equals(null)) {
				submitGcmUser(regid);
			}
		}
	};

	private class PostServer extends
			AsyncTask<Map<String, String>, Integer, String> {

		private String serverUrl;

		PostServer(String serverUrl) {
			this.serverUrl = serverUrl;
		}

		@Override
		protected String doInBackground(Map<String, String>... params) {
			URL url;
			try {
				url = new URL(serverUrl);
			} catch (MalformedURLException e) {
				throw new IllegalArgumentException("invalid url: " + serverUrl);
			}
			StringBuilder bodyBuilder = new StringBuilder();
			Iterator<Entry<String, String>> iterator = params[0].entrySet()
					.iterator();
			while (iterator.hasNext()) {
				Entry<String, String> param = iterator.next();
				bodyBuilder.append(param.getKey()).append('=')
						.append(param.getValue());
				if (iterator.hasNext()) {
					bodyBuilder.append('&');
				}
			}
			String body = bodyBuilder.toString();
			byte[] bytes = body.getBytes();
			HttpURLConnection conn = null;
			try {
				conn = (HttpURLConnection) url.openConnection();
				conn.setDoOutput(true);
				conn.setUseCaches(false);
				conn.setFixedLengthStreamingMode(bytes.length);
				conn.setRequestMethod("POST");
				conn.setRequestProperty("Content-Type",
						"application/x-www-form-urlencoded;charset=UTF-8");
				// post the request
				OutputStream out = conn.getOutputStream();
				out.write(bytes);
				out.close();
				// handle the response
				int status = conn.getResponseCode();
				if (status != 200) {
					throw new IOException("Post failed with error code "
							+ status);
				}
				conn.disconnect();
			} catch (ProtocolException p) {
				p.printStackTrace();
			} catch (IOException e) {
				e.printStackTrace();
			}
			return null;
		}

		@Override
		protected void onPostExecute(String results) {

		}
	}

	private void setRegistrationId(Context context, String regId) {
		int appVersion = getAppVersion(context);
		SharedPreferences.Editor editor = mPrefs.edit();
		editor.putString(REG_ID, regId);
		editor.putInt(PROPERTY_APP_VERSION, appVersion);
		long expirationTime = System.currentTimeMillis()
				+ REGISTRATION_EXPIRY_TIME_MS;
		editor.putLong(PROPERTY_ON_SERVER_EXPIRATION_TIME, expirationTime);
		editor.commit();
	}

	public void submitGcmBroadcast(String message) {
		CloudMessage mCloudMessage = new CloudMessage();
		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.HONEYCOMB) {
			mCloudMessage.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR,
					message);
		} else {
			mCloudMessage.execute(message);
		}
	}

	public class CloudMessage extends AsyncTask<String, Integer, Object> {

		@Override
		protected Object doInBackground(String... params) {
			try {
				ArrayList<NameValuePair> mPairs = new ArrayList<NameValuePair>();
				boolean hasIdentitiy = false;
				String identity = mPrefs.getString(PREF_IDENTITY, "?");
				if (!identity.equals("?")) {
					mPairs.add(new BasicNameValuePair("sender", "reicast tester #"
							+ identity));
					hasIdentitiy = true;
				} else {
					String ip = getSenderIPAddress(true);
					if (ip != null && !ip.equals(null)) {
						mPairs.add(new BasicNameValuePair("sender", ip));
						hasIdentitiy = true;
					}
				}
				if (hasIdentitiy) {
					mPairs.add(new BasicNameValuePair("message", params[0]));
					HttpClient client = new DefaultHttpClient();
					HttpPost post = new HttpPost(cloudUrl);
					post.setEntity(new UrlEncodedFormEntity(mPairs));
					return client.execute(post, new ResponseHandler<Object>() {

						@Override
						public Object handleResponse(HttpResponse response)
								throws ClientProtocolException, IOException {
							return response.toString();
						}
					});
				}
			} catch (MalformedURLException e) {
				Log.d(APP_TAG, "MalformedURLException: " + e);
			} catch (IOException e) {
				Log.d(APP_TAG, "IOException: " + e);
			} catch (Exception e) {
				Log.d(APP_TAG, "Exception: " + e);
			}
			return null;
		}

		@Override
		protected void onPostExecute(Object message) {
			if (message != null && !message.equals(null)) {
				Log.d(APP_TAG, "GCM: " + message.toString());
			}
		}

	}

	public void submitGcmUser(String regid) {
		CloudSelecao mCloudSelecao = new CloudSelecao();
		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.HONEYCOMB) {
			mCloudSelecao.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR,
					regid);
		} else {
			mCloudSelecao.execute(regid);
		}
	}

	public class CloudSelecao extends AsyncTask<String, Integer, Object> {

		@Override
		protected Object doInBackground(String... params) {
			try {
				ArrayList<NameValuePair> mPairs = new ArrayList<NameValuePair>();
				mPairs.add(new BasicNameValuePair("gcm_regid", params[0]));
				HttpClient client = new DefaultHttpClient();
				HttpPost post = new HttpPost(numberUrl);
				post.setEntity(new UrlEncodedFormEntity(mPairs));
				return client.execute(post, new ResponseHandler<Object>() {

					@Override
					public Object handleResponse(HttpResponse response)
							throws ClientProtocolException, IOException {
						HttpEntity resEntitiy = response.getEntity();
						if (resEntitiy.getContentLength() > 0) {
							StringBuilder sb = new StringBuilder();
							try {
								BufferedReader reader = new BufferedReader(
										new InputStreamReader(resEntitiy
												.getContent()), 65728);
								String line = null;

								while ((line = reader.readLine()) != null) {
									sb.append(line);
								}
							} catch (IOException e) {
								e.printStackTrace();
								return null;
							} catch (Exception e) {
								e.printStackTrace();
								return null;
							}
							return sb.toString();
						} else {
							return null;
						}
					}
				});
			} catch (MalformedURLException e) {
				Log.d(APP_TAG, "MalformedURLException: " + e);
			} catch (IOException e) {
				Log.d(APP_TAG, "IOException: " + e);
			} catch (Exception e) {
				Log.d(APP_TAG, "Exception: " + e);
			}
			return null;
		}

		@Override
		protected void onPostExecute(Object response) {
			if (response != null && !response.equals(null)) {
				String identity = response.toString();
				if (!identity.equals("Err")) {
					mPrefs.edit().putString(PREF_IDENTITY, identity).commit();
				}
				Log.d(APP_TAG, "reicast tester #" + identity);
			}
		}

	}

	public String getAccountEmail() {
		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.ECLAIR) {
			Pattern emailPattern = Patterns.EMAIL_ADDRESS; // API level 8+
			Account[] accounts = AccountManager.get(mContext).getAccounts();
			for (Account account : accounts) {
				if (emailPattern.matcher(account.name).matches()) {
					if (account.name.toLowerCase(Locale.getDefault()).endsWith(
							"@gmail.com")) {
						return account.name;
					}
				}
			}
		}
		return null;
	}

	/**
	 * Obtain the current device IP address from mobile interface
	 * 
	 * @param useIPv4
	 *            Specify the infrastructure to return from
	 * 
	 * @return String The IP address associated with this device
	 */
	public static String getSenderIPAddress(boolean useIPv4) {
		try {
			List<NetworkInterface> interfaces = Collections
					.list(NetworkInterface.getNetworkInterfaces());
			for (NetworkInterface intf : interfaces) {
				List<InetAddress> addrs = Collections.list(intf
						.getInetAddresses());
				for (InetAddress addr : addrs) {
					if (!addr.isLoopbackAddress()) {
						String sAddr = addr.getHostAddress().toUpperCase(
								Locale.getDefault());
						boolean isIPv4 = InetAddressUtils.isIPv4Address(sAddr);
						if (useIPv4) {
							if (isIPv4)
								return sAddr;
						} else {
							if (!isIPv4) {
								int delim = sAddr.indexOf('%');
								return delim < 0 ? sAddr : sAddr.substring(0,
										delim);
							}
						}
					}
				}
			}
		} catch (Exception ex) {

		}
		return null;
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
