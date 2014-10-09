package com.reicast.emulator;

import java.io.File;
import java.io.FileOutputStream;
import java.io.FilenameFilter;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;
import java.util.Locale;

import org.apache.commons.lang3.StringUtils;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.SharedPreferences;
import android.graphics.Typeface;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.os.AsyncTask;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.os.Vibrator;
import android.preference.PreferenceManager;
import android.support.v4.app.Fragment;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.View.OnTouchListener;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;
import android.widget.Toast;

import com.android.util.FileUtils;
import com.reicast.emulator.config.Config;
import com.reicast.emulator.emu.JNIdc;

public class FileBrowser extends Fragment {

	private Vibrator vib;
	private Drawable orig_bg;
	private Activity parentActivity;
	private boolean ImgBrowse;
	private boolean games;
	private OnItemSelectedListener mCallback;

	private SharedPreferences mPrefs;
	private File sdcard = Environment.getExternalStorageDirectory();
	private String home_directory = sdcard + "/dc";
	private String game_directory = sdcard + "/dc";

	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);

		mPrefs = PreferenceManager.getDefaultSharedPreferences(getActivity());
		home_directory = mPrefs.getString(Config.pref_home, home_directory);
		game_directory = mPrefs.getString(Config.pref_games, game_directory);

		Bundle b = getArguments();
		if (b != null) {
			ImgBrowse = b.getBoolean("ImgBrowse", true);
			if (games = b.getBoolean("games_entry", false)) {
				if (b.getString("path_entry") != null) {
					home_directory = b.getString("path_entry");
				}
			} else {
				if (b.getString("path_entry") != null) {
					game_directory = b.getString("path_entry");
				}
			}
		}

	}

	// Container Activity must implement this interface
	public interface OnItemSelectedListener {
		void onGameSelected(Uri uri);
		void onFolderSelected(Uri uri);
	}

	@Override
	public void onAttach(Activity activity) {
		super.onAttach(activity);

		// This makes sure that the container activity has implemented
		// the callback interface. If not, it throws an exception
		try {
			mCallback = (OnItemSelectedListener) activity;
		} catch (ClassCastException e) {
			throw new ClassCastException(activity.toString()
					+ " must implement OnItemSelectedListener");
		}
	}

	@Override
	public View onCreateView(LayoutInflater inflater, ViewGroup container,
			Bundle savedInstanceState) {
		return inflater.inflate(R.layout.activity_main, container, false);
	}

	@Override
	public void onViewCreated(View view, Bundle savedInstanceState) {
		// setContentView(R.layout.activity_main);
		parentActivity = getActivity();
		try {
			File file = new File(home_directory, "data/buttons.png");
			if (!file.exists()) {
				file.createNewFile();
				OutputStream fo = new FileOutputStream(file);
				InputStream png = parentActivity.getAssets()
						.open("buttons.png");

				byte[] buffer = new byte[4096];
				int len = 0;
				while ((len = png.read(buffer)) != -1) {
					fo.write(buffer, 0, len);
				}
				fo.close();
				png.close();
			}
		} catch (IOException ioe) {
			ioe.printStackTrace();
		}

		vib = (Vibrator) parentActivity
				.getSystemService(Context.VIBRATOR_SERVICE);

		/*
		 * OnTouchListener viblist=new OnTouchListener() {
		 * 
		 * public boolean onTouch(View v, MotionEvent event) { if
		 * (event.getActionMasked()==MotionEvent.ACTION_DOWN) vib.vibrate(50);
		 * return false; } };
		 * 
		 * findViewById(R.id.config).setOnTouchListener(viblist);
		 * findViewById(R.id.about).setOnTouchListener(viblist);
		 */

		File home = new File(mPrefs.getString(Config.pref_home, home_directory));
		if (!home.exists() || !home.isDirectory()) {
			Toast.makeText(getActivity(), R.string.config_home,
					Toast.LENGTH_LONG).show();
		}

		if (!ImgBrowse) {
//			navigate(sdcard);
			LocateGames mLocateGames = new LocateGames(R.array.flash);
			if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.HONEYCOMB) {
				mLocateGames
						.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR, home_directory);
			} else {
				mLocateGames.execute(home_directory);
			}
		} else {
			LocateGames mLocateGames = new LocateGames(R.array.images);
			if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.HONEYCOMB) {
				mLocateGames
						.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR, game_directory);
			} else {
				mLocateGames.execute(game_directory);
			}
		}
	}

	private final class LocateGames extends AsyncTask<String, Integer, List<File>> {
		
		private int array;
		
		public LocateGames(int arrayType) {
			this.array = arrayType;
		}

		@Override
		protected List<File> doInBackground(String... paths) {
			File storage = new File(paths[0]);

			// array of valid image file extensions
			String[] mediaTypes = parentActivity.getResources().getStringArray(array);
			FilenameFilter[] filter = new FilenameFilter[mediaTypes.length];

			int i = 0;
			for (final String type : mediaTypes) {
				filter[i] = new FilenameFilter() {

					public boolean accept(File dir, String name) {
						if (dir.getName().startsWith(".")
								|| name.startsWith(".")) {
							return false;
						} else {
							return StringUtils.endsWithIgnoreCase(name, "."
									+ type);
						}
					}

				};
				i++;
			}

			FileUtils fileUtils = new FileUtils();
			Collection<File> files = fileUtils.listFiles(storage, filter, 1);
			return (List<File>) files;
		}

		@Override
		protected void onPostExecute(List<File> items) {
			final LinearLayout list = (LinearLayout) parentActivity.findViewById(R.id.game_list);
			if (list != null) {
				list.removeAllViews();
			}

			String heading = parentActivity.getString(R.string.games_listing);
			createListHeader(heading, list, array == R.array.images);
			if (items != null && !items.isEmpty()) {
				for (int i = 0; i < items.size(); i++) {
					createListItem(list, items.get(i), i, array == R.array.images);
				}
			} else {
				Toast.makeText(parentActivity, R.string.config_game, Toast.LENGTH_LONG).show();
			}
			list.invalidate();
		}
	}

	private static final class DirSort implements Comparator<File> {

		@Override
		public int compare(File filea, File fileb) {

			return ((filea.isFile() ? "a" : "b") + filea.getName().toLowerCase(
					Locale.getDefault()))
					.compareTo((fileb.isFile() ? "a" : "b")
							+ fileb.getName().toLowerCase(Locale.getDefault()));
		}
	}

	private void createListHeader(String header_text, View view, boolean hasBios) {
		if (hasBios) {
			final View childview = parentActivity.getLayoutInflater().inflate(
					R.layout.bios_list_item, null, false);

			((TextView) childview.findViewById(R.id.item_name))
					.setText(R.string.boot_bios);

			childview.setTag(null);

			orig_bg = childview.getBackground();

			childview.findViewById(R.id.childview).setOnClickListener(
					new OnClickListener() {
						public void onClick(View view) {
							File f = (File) view.getTag();
							vib.vibrate(50);
							mCallback.onGameSelected(f != null ? Uri
									.fromFile(f) : Uri.EMPTY);
							vib.vibrate(250);
						}
					});

			childview.findViewById(R.id.childview).setOnTouchListener(
					new OnTouchListener() {
						@SuppressWarnings("deprecation")
						public boolean onTouch(View view, MotionEvent arg1) {
							if (arg1.getActionMasked() == MotionEvent.ACTION_DOWN) {
								view.setBackgroundColor(0xFF4F3FFF);
							} else if (arg1.getActionMasked() == MotionEvent.ACTION_CANCEL
									|| arg1.getActionMasked() == MotionEvent.ACTION_UP) {
								view.setBackgroundDrawable(orig_bg);
							}

							return false;

						}
					});
			((ViewGroup) view).addView(childview);
		}

		final View headerView = parentActivity.getLayoutInflater().inflate(
				R.layout.app_list_item, null, false);
		((ImageView) headerView.findViewById(R.id.item_icon))
				.setImageResource(R.drawable.open_folder);
		((TextView) headerView.findViewById(R.id.item_name))
				.setText(header_text);
		((TextView) headerView.findViewById(R.id.item_name))
				.setTypeface(Typeface.DEFAULT_BOLD);
		((ViewGroup) view).addView(headerView);

	}

	private void createListItem(LinearLayout list, final File game, final int index, final boolean isGame) {				
		final View childview = parentActivity.getLayoutInflater().inflate(
				R.layout.app_list_item, null, false);
		
		final XMLParser xmlParser = new XMLParser(game, index, mPrefs);
		xmlParser.setViewParent(parentActivity, childview);
		xmlParser.execute(game.getName());

		orig_bg = childview.getBackground();

		// vw.findViewById(R.id.childview).setBackgroundColor(0xFFFFFFFF);

		childview.findViewById(R.id.childview).setOnClickListener(
				new OnClickListener() {
					public void onClick(View view) {
						if (isGame) {
						vib.vibrate(50);
						if (mPrefs.getBoolean(Config.pref_gamedetails, false)) {
							final AlertDialog.Builder builder = new AlertDialog.Builder(parentActivity);
							builder.setCancelable(true);
							builder.setTitle(getString(R.string.game_details,
									xmlParser.getGameTitle()));
							builder.setMessage(xmlParser.game_details.get(index));
							builder.setIcon(xmlParser.getGameIcon());
							builder.setPositiveButton("Close",
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
											mCallback.onGameSelected(game != null ? Uri
													.fromFile(game) : Uri.EMPTY);
											vib.vibrate(250);
											return;
										}
									});
							builder.create().show();
						} else {
							mCallback.onGameSelected(game != null ? Uri
									.fromFile(game) : Uri.EMPTY);
							vib.vibrate(250);
						}
						} else {
							vib.vibrate(50);
							mCallback.onFolderSelected(game != null ? Uri
									.fromFile(game) : Uri.EMPTY);
							home_directory = game.getAbsolutePath().substring(0,
									game.getAbsolutePath().lastIndexOf(File.separator));
							mPrefs.edit().putString("home_directory",
									home_directory.replace("/data", "")).commit();
							JNIdc.config(home_directory.replace("/data", ""));
						}
					}
				});

		childview.findViewById(R.id.childview).setOnTouchListener(
				new OnTouchListener() {
					@SuppressWarnings("deprecation")
					public boolean onTouch(View view, MotionEvent arg1) {
						if (arg1.getActionMasked() == MotionEvent.ACTION_DOWN) {
							view.setBackgroundColor(0xFF4F3FFF);
						} else if (arg1.getActionMasked() == MotionEvent.ACTION_CANCEL
								|| arg1.getActionMasked() == MotionEvent.ACTION_UP) {
							view.setBackgroundDrawable(orig_bg);
						}
						return false;

					}
				});
		list.addView(childview);
	}

	void navigate(final File root_sd) {
		LinearLayout v = (LinearLayout) parentActivity
				.findViewById(R.id.game_list);
		v.removeAllViews();

		ArrayList<File> list = new ArrayList<File>();

		final String heading = root_sd.getAbsolutePath();
		createListHeader(heading, v, false);

		File flist[] = root_sd.listFiles();
		File parent = root_sd.getParentFile();

		list.add(null);

		if (parent != null)
			list.add(parent);

		Arrays.sort(flist, new DirSort());

		Collections.addAll(list, flist);

		for (final File file : list) {
			if (file != null && !file.isDirectory())
				continue;
			final View childview = parentActivity.getLayoutInflater().inflate(
					R.layout.app_list_item, null, false);

			if (file == null) {
				((TextView) childview.findViewById(R.id.item_name)).setText(R.string.folder_select);
			} else if (file == parent)
				((TextView) childview.findViewById(R.id.item_name)).setText("..");
			else
				((TextView) childview.findViewById(R.id.item_name)).setText(file.getName());

			((ImageView) childview.findViewById(R.id.item_icon))
					.setImageResource(file == null ? R.drawable.config
							: file.isDirectory() ? R.drawable.open_folder
									: R.drawable.disk_unknown);

			childview.setTag(file);

			orig_bg = childview.getBackground();

			// vw.findViewById(R.id.childview).setBackgroundColor(0xFFFFFFFF);

			childview.findViewById(R.id.childview).setOnClickListener(
					new OnClickListener() {
						public void onClick(View view) {
							if (file != null && file.isDirectory()) {
								navigate(file);
								ScrollView sv = (ScrollView) parentActivity
										.findViewById(R.id.game_scroller);
								sv.scrollTo(0, 0);
								vib.vibrate(50);
							} else if (view.getTag() == null) {
								vib.vibrate(50);

								mCallback.onFolderSelected(Uri
										.fromFile(new File(heading)));
								vib.vibrate(250);

								if (games) {
									game_directory = heading;
									mPrefs.edit()
											.putString(Config.pref_games,
													heading).commit();
								} else {
									home_directory = heading;
									mPrefs.edit()
											.putString(Config.pref_home,
													heading).commit();
									File data_directory = new File(heading,
											"data");
									if (!data_directory.exists()
											|| !data_directory.isDirectory()) {
										data_directory.mkdirs();
									}
									JNIdc.config(heading);
								}
							}
						}
					});

			childview.findViewById(R.id.childview).setOnTouchListener(
					new OnTouchListener() {
						@SuppressWarnings("deprecation")
						public boolean onTouch(View view, MotionEvent event) {
							if (event.getActionMasked() == MotionEvent.ACTION_DOWN) {
								view.setBackgroundColor(0xFF4F3FFF);
							} else if (event.getActionMasked() == MotionEvent.ACTION_CANCEL
									|| event.getActionMasked() == MotionEvent.ACTION_UP) {
								view.setBackgroundDrawable(orig_bg);
							}

							return false;

						}
					});
			v.addView(childview);
		}
		v.invalidate();
	}
}
