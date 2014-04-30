package com.reicast.emulator;

import java.io.BufferedInputStream;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.io.StringReader;
import java.io.UnsupportedEncodingException;
import java.net.URL;
import java.net.URLConnection;
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

import com.reicast.emulator.config.Config;

import android.content.Context;
import android.content.SharedPreferences;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;
import android.os.AsyncTask;
import android.os.Build;
import android.os.StrictMode;
import android.util.SparseArray;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

public class XMLParser extends AsyncTask<String, Integer, String> {

	private SharedPreferences mPrefs;
	private File game;
	private int index;
	private View childview;
	private Context mContext;
	private String game_name;
	private Drawable game_icon;

	private static final String game_index = "http://thegamesdb.net/api/GetGame.php?platform=sega+dreamcast&name=";
	public SparseArray<String> game_details = new SparseArray<String>();
	public SparseArray<Bitmap> game_preview = new SparseArray<Bitmap>();

	public XMLParser(File game, int index, SharedPreferences mPrefs) {
		this.mPrefs = mPrefs;
		this.game = game;
		this.index = index;
	}

	public void setViewParent(Context mContext, View childview) {
		this.mContext = mContext;
		this.childview = childview;
	}

	protected void onPreExecute() {
		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.GINGERBREAD) {
			StrictMode.ThreadPolicy policy = new StrictMode.ThreadPolicy.Builder()
					.permitAll().build();
			StrictMode.setThreadPolicy(policy);
		}
	}

	public Bitmap decodeBitmapIcon(String filename) throws IOException {
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
		return bitmap;
	}

	@Override
	protected String doInBackground(String... params) {
		String filename = game_name = params[0];
		if (isNetworkAvailable() && mPrefs.getBoolean(Config.pref_gamedetails, false)) {
			if (params[0].contains("[")) {
				filename = params[0].substring(0, params[0].lastIndexOf("["));
			} else {
				filename = params[0].substring(0, params[0].lastIndexOf("."));
			}
			filename = filename.replaceAll("[^\\p{L}\\p{Nd}]", " ");
			filename = filename.replace(" ", "+");
			if (filename.endsWith("+")) {
				filename = filename.substring(0, filename.length() - 1);
			}
			try {
				DefaultHttpClient httpClient = new DefaultHttpClient();
				HttpPost httpPost = new HttpPost(game_index + filename);

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
			Document doc = getDomElement(gameData);
			if (doc != null && doc.getElementsByTagName("Game") != null) {
				Element root = (Element) doc.getElementsByTagName("Game").item(
						0);
				game_name = getValue(root, "GameTitle");
				String details = getValue(root, "Overview");
				game_details.put(index, details);
				Element boxart = (Element) root.getElementsByTagName("Images")
						.item(0);
				String image = "http://thegamesdb.net/banners/"
						+ getValue(boxart, "boxart");
				try {
					game_preview.put(index, decodeBitmapIcon(image));
					game_icon = new BitmapDrawable(decodeBitmapIcon(image));
				} catch (IOException e) {
					// TODO Auto-generated catch block
					e.printStackTrace();
				}
			}
		} else {
			game_details.put(index,
					mContext.getString(R.string.info_unavailable));
			final String nameLower = game.getName().toLowerCase(
					Locale.getDefault());
			game_icon = mContext.getResources().getDrawable(
					game.isDirectory() ? R.drawable.open_folder : nameLower
							.endsWith(".gdi") ? R.drawable.gdi : nameLower
							.endsWith(".cdi") ? R.drawable.cdi : nameLower
							.endsWith(".chd") ? R.drawable.chd
							: R.drawable.disk_unknown);

		}

		((TextView) childview.findViewById(R.id.item_name)).setText(game_name);

		((ImageView) childview.findViewById(R.id.item_icon))
				.setImageDrawable(game_icon);

		childview.setTag(game_name);
	}

	private boolean isNetworkAvailable() {
		ConnectivityManager connectivityManager = (ConnectivityManager) mContext
				.getSystemService(Context.CONNECTIVITY_SERVICE);
		NetworkInfo activeNetworkInfo = connectivityManager
				.getActiveNetworkInfo();
		return activeNetworkInfo != null && activeNetworkInfo.isConnected();
	}

	public Drawable getGameIcon() {
		return game_icon;
	}

	public String getGameTitle() {
		return game_name;
	}

	@Override
	protected void onPostExecute(String gameData) {
		if (gameData != null) {
			Document doc = getDomElement(gameData);
			if (doc != null && doc.getElementsByTagName("Game") != null) {
				Element root = (Element) doc.getElementsByTagName("Game").item(
						0);
				game_name = getValue(root, "GameTitle");
				String details = getValue(root, "Overview");
				game_details.put(index, details);
				Element boxart = (Element) root.getElementsByTagName("Images")
						.item(0);
				String image = "http://thegamesdb.net/banners/"
						+ getValue(boxart, "boxart");
				try {
					game_preview.put(index, decodeBitmapIcon(image));
					game_icon = new BitmapDrawable(decodeBitmapIcon(image));
				} catch (IOException e) {
					// TODO Auto-generated catch block
					e.printStackTrace();
				}
			}
		} else {
			game_details.put(index, mContext.getString(R.string.info_unavailable));
			final String nameLower = game.getName().toLowerCase(Locale.getDefault());
			game_icon = mContext.getResources().getDrawable(
					game.isDirectory() ? R.drawable.open_folder : nameLower
							.endsWith(".gdi") ? R.drawable.gdi : nameLower
							.endsWith(".cdi") ? R.drawable.cdi : nameLower
							.endsWith(".chd") ? R.drawable.chd
							: R.drawable.disk_unknown);
			
		}

		((TextView) childview.findViewById(R.id.item_name)).setText(game_name);

		((ImageView) childview.findViewById(R.id.item_icon))
				.setImageDrawable(game_icon);

		childview.setTag(game_name);
	}

	private boolean isNetworkAvailable() {
		ConnectivityManager connectivityManager = (ConnectivityManager) mContext
				.getSystemService(Context.CONNECTIVITY_SERVICE);
		NetworkInfo activeNetworkInfo = connectivityManager
				.getActiveNetworkInfo();
		return activeNetworkInfo != null && activeNetworkInfo.isConnected();
	}

	public Drawable getGameIcon() {
		return game_icon;
	}

	public String getGameTitle() {
		return game_name;
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
