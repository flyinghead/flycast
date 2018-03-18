package com.reicast.emulator;

import java.io.BufferedInputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.StringReader;
import java.io.UnsupportedEncodingException;
import java.net.URL;
import java.net.URLConnection;
import java.net.URLEncoder;
import java.util.Locale;

import javax.xml.parsers.DocumentBuilder;
import javax.xml.parsers.DocumentBuilderFactory;
import javax.xml.parsers.ParserConfigurationException;

import org.apache.http.HttpEntity;
import org.apache.http.HttpResponse;
import org.apache.http.client.ClientProtocolException;
import org.apache.http.client.methods.HttpPost;
import org.apache.http.impl.client.DefaultHttpClient;
import org.apache.http.util.EntityUtils;
import org.w3c.dom.Document;
import org.w3c.dom.Element;
import org.w3c.dom.Node;
import org.w3c.dom.NodeList;
import org.xml.sax.InputSource;
import org.xml.sax.SAXException;

import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.SharedPreferences;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;
import android.net.Uri;
import android.os.AsyncTask;
import android.os.Build;
import android.os.StrictMode;
import android.os.Vibrator;
import android.view.View;
import android.view.View.OnLongClickListener;
import android.widget.ImageView;
import android.widget.TextView;

import com.reicast.emulator.FileBrowser.OnItemSelectedListener;
import com.reicast.emulator.config.Config;

public class XMLParser extends AsyncTask<String, Integer, String> {

	private SharedPreferences mPrefs;
	private File game;
	private int index;
	private View childview;
	private OnItemSelectedListener mCallback;
	private Context mContext;
	private String game_name;
	private Bitmap coverart;
	private Drawable game_icon;
	private String gameId;
	private String game_details;

	private static final String game_index = "http://thegamesdb.net/api/GetGamesList.php?platform=sega+dreamcast&name=";
	private static final String game_id = "http://thegamesdb.net/api/GetGame.php?platform=sega+dreamcast&id=";

	public XMLParser(File game, int index, SharedPreferences mPrefs) {
		this.mPrefs = mPrefs;
		this.game = game;
		this.index = index;
	}

	public void setViewParent(Context mContext, View childview, OnItemSelectedListener mCallback) {
		this.mContext = mContext;
		this.childview = childview;
		this.mCallback = mCallback;
	}
	
	public void setGameID(String id) {
		this.gameId = id;
		initializeDefaults();
	}

	protected void onPreExecute() {
		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.GINGERBREAD) {
			StrictMode.ThreadPolicy policy = new StrictMode.ThreadPolicy.Builder()
					.permitAll().build();
			StrictMode.setThreadPolicy(policy);
		}
	}

	public Bitmap decodeBitmapIcon(String filename) throws IOException {
		String index = filename.substring(filename.lastIndexOf("/") + 1, filename.lastIndexOf("."));
		File file = new File(mContext.getExternalFilesDir(null) + "/images", index + ".png");
		if (file.exists()) {
			return BitmapFactory.decodeFile(file.getAbsolutePath());
		} else {
			URL updateURL = new URL(filename);
			URLConnection conn1 = updateURL.openConnection();
			InputStream im = conn1.getInputStream();
			BufferedInputStream bis = new BufferedInputStream(im, 512);
			BitmapFactory.Options options = new BitmapFactory.Options();
			options.inJustDecodeBounds = true;
			Bitmap bitmap = BitmapFactory.decodeStream(bis, null, options);
			int heightRatio = (int) Math.ceil(options.outHeight / (float) 72);
			int widthRatio = (int) Math.ceil(options.outWidth / (float) 72);
			if (heightRatio > 1 || widthRatio > 1) {
				if (heightRatio > widthRatio) {
					options.inSampleSize = heightRatio;
				} else {
					options.inSampleSize = widthRatio;
				}
			}
			options.inJustDecodeBounds = false;
			bis.close();
			im.close();
			conn1 = updateURL.openConnection();
			im = conn1.getInputStream();
			bis = new BufferedInputStream(im, 512);
			bitmap = BitmapFactory.decodeStream(bis, null, options);
			bis.close();
			im.close();
			bis = null;
			im = null;
			OutputStream fOut = null;
			if (!file.getParentFile().exists()) {
				file.getParentFile().mkdir();
			}
			try {
				fOut = new FileOutputStream(file, false);
				bitmap.compress(Bitmap.CompressFormat.PNG, 100, fOut);
				fOut.flush();
				fOut.close();
			} catch (Exception ex) {
				
			}
			return bitmap;
		}
	}

	@Override
	protected String doInBackground(String... params) {
		String filename = game_name = params[0];
		if (isNetworkAvailable() && mPrefs.getBoolean(Config.pref_gamedetails, false)) {
			DefaultHttpClient httpClient = new DefaultHttpClient();
			HttpPost httpPost;
			if (gameId != null) {
				httpPost = new HttpPost(game_id + gameId);
			} else {
				filename = filename.substring(0, filename.lastIndexOf("."));
				try {
					filename = URLEncoder.encode(filename, "UTF-8");
				} catch (UnsupportedEncodingException e) {
					filename = filename.replace(" ", "+");
				}
				httpPost = new HttpPost(game_index + filename);
			}
			try {
				HttpResponse httpResponse = httpClient.execute(httpPost);
				HttpEntity httpEntity = httpResponse.getEntity();
				return EntityUtils.toString(httpEntity);
			} catch (UnsupportedEncodingException e) {

			} catch (ClientProtocolException e) {

			} catch (IOException e) {

			}
		}
		return null;
	}

	@Override
	protected void onPostExecute(String gameData) {
		if (gameData != null) {
			try {
				Document doc = getDomElement(gameData);
				if (doc.getElementsByTagName("Game") != null) {
					Element root = (Element) doc.getElementsByTagName("Game").item(0);
					if (gameId == null) {
						XMLParser xmlParser = new XMLParser(game, index, mPrefs);
						xmlParser.setViewParent(mContext, childview, mCallback);
						xmlParser.setGameID(getValue(root, "id"));
						xmlParser.execute(game_name);
					} else {
						game_name = getValue(root, "GameTitle");
						game_details = getValue(root, "Overview");
						Element images = (Element) root.getElementsByTagName("Images").item(0);
						Element boxart = null;
						if (images.getElementsByTagName("boxart").getLength() > 1) {
							boxart = (Element) images.getElementsByTagName("boxart").item(1);
						} else if (images.getElementsByTagName("boxart").getLength() == 1) {
							boxart = (Element) images.getElementsByTagName("boxart").item(0);
						}
						if (boxart != null) {
							coverart = decodeBitmapIcon("http://thegamesdb.net/banners/" + getElementValue(boxart));
							game_icon = new BitmapDrawable(coverart);
						}
					}
				}
			} catch (Exception e) {
				
			}
		}

		((TextView) childview.findViewById(R.id.item_name)).setText(game_name);

		if (Build.VERSION.SDK_INT < 21) {
			((ImageView) childview.findViewById(R.id.item_icon)).setImageDrawable(game_icon);
		} else {
			((ImageView) childview.findViewById(R.id.item_icon)).setImageBitmap(coverart);
		}
		
		if (mPrefs.getBoolean(Config.pref_gamedetails, false)) {
			childview.findViewById(R.id.childview).setOnLongClickListener(
					new OnLongClickListener() {
						public boolean onLongClick(View view) {
							final AlertDialog.Builder builder = new AlertDialog.Builder(mContext);
							builder.setCancelable(true);
							builder.setTitle(mContext.getString(R.string.game_details, game_name));
							builder.setMessage(game_details);
							builder.setIcon(game_icon);
							builder.setNegativeButton("Close",
									new DialogInterface.OnClickListener() {
										public void onClick(DialogInterface dialog, int which) {
											dialog.dismiss();
											return;
										}
									});
							builder.setPositiveButton("Launch",
									new DialogInterface.OnClickListener() {
										public void onClick(DialogInterface dialog, int which) {
											dialog.dismiss();
											mCallback.onGameSelected(game != null ? Uri.fromFile(game) : Uri.EMPTY);
											((Vibrator) mContext.getSystemService(Context.VIBRATOR_SERVICE)).vibrate(250);
											return;
										}
									});
							builder.create().show();
							return true;
						}
					});
		}

		childview.setTag(game_name);
	}
	
	private void initializeDefaults() {
		game_details = mContext.getString(R.string.info_unavailable);
		final String nameLower = game.getName().toLowerCase(
				Locale.getDefault());
		if (Build.VERSION.SDK_INT < 21) {
			game_icon = mContext.getResources().getDrawable(
					game.isDirectory() ? R.drawable.open_folder : nameLower
							.endsWith(".gdi") ? R.drawable.gdi : nameLower
							.endsWith(".chd") ? R.drawable.chd
							: R.drawable.disk_unknown);
		}
	}

	public boolean isNetworkAvailable() {
		ConnectivityManager connectivityManager = (ConnectivityManager) mContext
				.getSystemService(Context.CONNECTIVITY_SERVICE);		
		NetworkInfo mWifi = connectivityManager.getNetworkInfo(ConnectivityManager.TYPE_WIFI);
		NetworkInfo mMobile = connectivityManager.getNetworkInfo(ConnectivityManager.TYPE_MOBILE);
		NetworkInfo activeNetworkInfo = connectivityManager.getActiveNetworkInfo();
		if (mMobile != null && mWifi != null) {
			return mMobile.isAvailable() || mWifi.isAvailable();
		} else {
			return activeNetworkInfo != null && activeNetworkInfo.isConnected();
		}
	}

	public Drawable getGameIcon() {
		return game_icon;
	}
	
	public Bitmap getGameCover() {
		return coverart;
	}

	public String getGameTitle() {
		return game_name;
	}
	
	public String getGameDetails() {
		return game_details;
	}

	public Document getDomElement(String xml) {
		Document doc = null;
		DocumentBuilderFactory dbf = DocumentBuilderFactory.newInstance();
		try {

			DocumentBuilder db = dbf.newDocumentBuilder();

			InputSource is = new InputSource();
			is.setCharacterStream(new StringReader(xml));
			doc = db.parse(is);

		} catch (ParserConfigurationException e) {

			return null;
		} catch (SAXException e) {

			return null;
		} catch (IOException e) {

			return null;
		}

		return doc;
	}

	public String getValue(Element item, String str) {
		NodeList n = item.getElementsByTagName(str);
		return this.getElementValue(n.item(0));
	}

	public final String getElementValue(Node elem) {
		Node child;
		if (elem != null) {
			if (elem.hasChildNodes()) {
				for (child = elem.getFirstChild(); child != null; child = child
						.getNextSibling()) {
					if (child.getNodeType() == Node.TEXT_NODE) {
						return child.getNodeValue();
					}
				}
			}
		}
		return "";
	}
}
