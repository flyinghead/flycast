package com.reicast.emulator;

import android.app.Activity;
import android.content.Context;
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
import android.support.constraint.ConstraintLayout;
import android.support.design.widget.Snackbar;
import android.support.graphics.drawable.VectorDrawableCompat;
import android.support.v4.app.Fragment;
import android.view.Gravity;
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

import com.android.util.FileUtils;
import com.reicast.emulator.config.Config;
import com.reicast.emulator.emu.JNIdc;

import org.apache.commons.lang3.StringUtils;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
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
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.Locale;

public class FileBrowser extends Fragment {

	private Vibrator vib;
	private Drawable orig_bg;
	private boolean ImgBrowse;
	private boolean games;
	private String searchQuery = null;
	private OnItemSelectedListener mCallback;

	private SharedPreferences mPrefs;
	private File sdcard = Environment.getExternalStorageDirectory();
	private String home_directory = sdcard.getAbsolutePath();
	private String game_directory = sdcard.getAbsolutePath();

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
			if (b.getString("search_params") != null) {
				searchQuery = b.getString("search_params");
			}
		}
	}

	public static HashSet<String> getExternalMounts() {
		final HashSet<String> out = new HashSet<String>();
		String reg = "(?i).*vold.*(vfat|ntfs|exfat|fat32|ext3|ext4|fuse).*rw.*";
		StringBuilder s = new StringBuilder();
		try {
			final Process process = new ProcessBuilder().command("mount")
					.redirectErrorStream(true).start();
			process.waitFor();
			InputStream is = process.getInputStream();
			byte[] buffer = new byte[1024];
			while (is.read(buffer) != -1) {
				s.append(new String(buffer));
			}
			is.close();

			String[] lines = s.toString().split("\n");
			for (String line : lines) {
				if (StringUtils.containsIgnoreCase(line, "secure"))
					continue;
				if (StringUtils.containsIgnoreCase(line, "asec"))
					continue;
				if (line.matches(reg)) {
					String[] parts = line.split(" ");
					for (String part : parts) {
						if (part.startsWith("/"))
							if (!StringUtils.containsIgnoreCase(part, "vold"))
								out.add(part);
					}
				}
			}
		} catch (final Exception e) {
			e.printStackTrace();
		}
		return out;
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
		return inflater.inflate(R.layout.browser_fragment, container, false);
	}

	@Override
	public void onViewCreated(View view, Bundle savedInstanceState) {
		// setContentView(R.layout.activity_main);

		vib = (Vibrator) getActivity().getSystemService(Context.VIBRATOR_SERVICE);

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
			showToastMessage(getActivity().getString(R.string.config_home), Snackbar.LENGTH_LONG);
		} else {
			(new installGraphics()).execute();
		}

		if (!ImgBrowse && !games) {
			new LocateGames(R.array.flash).execute(home_directory);
		} else {
			new LocateGames(R.array.images).execute(game_directory);
		}
	}

	private class installGraphics extends AsyncTask<String, Integer, String> {
		@Override
		protected String doInBackground(String... params) {
			try {
				File buttons = null;
				String theme = mPrefs.getString(Config.pref_theme, null);
				if (theme != null) {
					buttons = new File(theme);
				}
				File file = new File(home_directory, "data/buttons.png");
				if (buttons != null && buttons.exists()) {
					InputStream in = new FileInputStream(buttons);
					OutputStream out = new FileOutputStream(file);

					// Transfer bytes from in to out
					byte[] buf = new byte[1024];
					int len;
					while ((len = in.read(buf)) > 0) {
						out.write(buf, 0, len);
					}
					in.close();
					out.close();
				} else if (!file.exists()) {
					org.apache.commons.io.FileUtils.touch(file);
					InputStream png = getActivity().getAssets().open("buttons.png");
					OutputStream fo = new FileOutputStream(file);
					byte[] buffer = new byte[4096];
					int read;
					while ((read = png.read(buffer)) != -1) {
						fo.write(buffer, 0, read);
					}
					png.close();
					fo.flush();
					fo.close();
				}
			} catch (FileNotFoundException fnf) {
				fnf.printStackTrace();
			} catch (IOException ioe) {
				ioe.printStackTrace();
			}
			return null;
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
			String[] mediaTypes = getActivity().getResources().getStringArray(array);
			FilenameFilter[] filter = new FilenameFilter[mediaTypes.length];

			int i = 0;
			for (final String type : mediaTypes) {
				filter[i] = new FilenameFilter() {

					public boolean accept(File dir, String name) {
						if (dir.getName().equals("obb") || dir.getName().equals("cache") 
							|| dir.getName().startsWith(".") || name.startsWith(".")) {
							return false;
						} else if (array == R.array.flash && !name.startsWith("dc_")) {
							return false;
						} else if (searchQuery == null || name.toLowerCase(Locale.getDefault())
								.contains(searchQuery.toLowerCase(Locale.getDefault())))
							return StringUtils.endsWithIgnoreCase(name, "." + type);
						else
							return false;
					}

				};
				i++;
			}

			FileUtils fileUtils = new FileUtils();
			int recurse = 2;
			if (array == R.array.flash) {
				recurse = 4;
			}
			Collection<File> files = fileUtils.listFiles(storage, filter, recurse);
			return (List<File>) files;
		}

		@Override
		protected void onPostExecute(List<File> items) {
			if (items != null && !items.isEmpty()) {
				LinearLayout list = (LinearLayout) getActivity().findViewById(R.id.game_list);
				if (list.getChildCount() > 0) {
					list.removeAllViews();
				}

				String heading = getActivity().getString(R.string.games_listing);
				createListHeader(heading, list, array == R.array.images);
				for (int i = 0; i < items.size(); i++) {
					createListItem(list, items.get(i), i, array == R.array.images);
				}
				list.invalidate();
			} else {
				browseStorage(array == R.array.images);
			}
		}
	}
	
	private void browseStorage(boolean images) {
		if (images) {
			(new navigate()).executeOnExecutor(
					AsyncTask.THREAD_POOL_EXECUTOR, new File(home_directory));
		} else {
			if (game_directory.equals(sdcard.getAbsolutePath())) {
				HashSet<String> extStorage = FileBrowser.getExternalMounts();
				if (extStorage != null && !extStorage.isEmpty()) {
					for (Iterator<String> sd = extStorage.iterator(); sd.hasNext();) {
						String sdCardPath = sd.next().replace("mnt/media_rw", "storage");
						if (!sdCardPath.equals(sdcard.getAbsolutePath())) {
							if (new File(sdCardPath).canRead()) {
								(new navigate()).execute(new File(sdCardPath));
								return;
							}
						}
					}
				}
			}
			(new navigate()).executeOnExecutor(
					AsyncTask.THREAD_POOL_EXECUTOR, new File(game_directory));
		}
	}

	private void createListHeader(String header_text, View view, boolean hasBios) {
		if (hasBios) {
			final View childview = getActivity().getLayoutInflater().inflate(
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

		final View headerView = getActivity().getLayoutInflater().inflate(
				R.layout.browser_fragment_header, null, false);
		((ImageView) headerView.findViewById(R.id.item_icon))
				.setImageResource(R.drawable.open_folder);
		((TextView) headerView.findViewById(R.id.item_name))
				.setText(header_text);
		((TextView) headerView.findViewById(R.id.item_name))
				.setTypeface(Typeface.DEFAULT_BOLD);
		((ViewGroup) view).addView(headerView);

	}

	private void createListItem(LinearLayout list, final File game, final int index, final boolean isGame) {				
		final View childview = getActivity().getLayoutInflater().inflate(
				R.layout.browser_fragment_item, null, false);
		
		XMLParser xmlParser = new XMLParser(game, index, mPrefs);
		xmlParser.setViewParent(getActivity(), childview, mCallback);
		orig_bg = childview.getBackground();

		childview.findViewById(R.id.childview).setOnClickListener(
				new OnClickListener() {
					public void onClick(View view) {
						if (isGame) {
							vib.vibrate(50);
							mCallback.onGameSelected(game != null ? Uri
									.fromFile(game) : Uri.EMPTY);
							vib.vibrate(250);
						} else {
							vib.vibrate(50);
							home_directory = game.getAbsolutePath().substring(0,
									game.getAbsolutePath().lastIndexOf(File.separator)).replace("/data", "");
							if (!DataDirectoryBIOS()) {
								showToastMessage(getActivity().getString(R.string.config_data, home_directory),
                                        Snackbar.LENGTH_LONG);
							}
							mPrefs.edit().putString(Config.pref_home, home_directory).apply();
                            mCallback.onFolderSelected(Uri.fromFile(new File(home_directory)));
							JNIdc.config(home_directory);
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
		xmlParser.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR, game.getName());
	}

	private final class navigate extends AsyncTask<File, Integer, List<File>> {

		private LinearLayout listView;
		private String heading;
		private File parent;

		private final class DirSort implements Comparator<File> {
			@Override
			public int compare(File filea, File fileb) {
				return ((filea.isFile() ? "a" : "b") + filea.getName().toLowerCase(
						Locale.getDefault()))
						.compareTo((fileb.isFile() ? "a" : "b")
								+ fileb.getName().toLowerCase(Locale.getDefault()));
			}
		}

		@Override
		protected void onPreExecute() {
			listView = (LinearLayout) getActivity().findViewById(R.id.game_list);
			if (listView.getChildCount() > 0)
				listView.removeAllViews();
		}

		@Override
		protected List<File> doInBackground(File... paths) {
			heading = paths[0].getAbsolutePath();

			ArrayList<File> list = new ArrayList<File>();

			File flist[] = paths[0].listFiles();
			parent = paths[0].getParentFile();

			list.add(null);

			if (parent != null) {
				list.add(parent);
			}

			if (flist != null) {
				Arrays.sort(flist, new DirSort());
				Collections.addAll(list, flist);
			}
			return list;
		}

		@Override
		protected void onPostExecute(List<File> list) {
			if (list != null && !list.isEmpty()) {
				createListHeader(heading, listView, false);
				for (final File file : list) {
					if (file != null && !file.isDirectory() && !file.getAbsolutePath().equals("/data"))
						continue;
					final View childview = getActivity().getLayoutInflater().inflate(
							R.layout.browser_fragment_item, null, false);

					if (file == null) {
						((TextView) childview.findViewById(R.id.item_name)).setText(R.string.folder_select);
					} else if (file == parent)
						((TextView) childview.findViewById(R.id.item_name)).setText("..");
					else
						((TextView) childview.findViewById(R.id.item_name)).setText(file.getName());

					((ImageView) childview.findViewById(R.id.item_icon)).setImageResource(file == null
							? R.drawable.ic_settings: file.isDirectory()
							? R.drawable.ic_folder_black_24dp : R.drawable.disk_unknown);;

					childview.setTag(file);

					orig_bg = childview.getBackground();

					// vw.findViewById(R.id.childview).setBackgroundColor(0xFFFFFFFF);

					childview.findViewById(R.id.childview).setOnClickListener(
							new OnClickListener() {
								public void onClick(View view) {
									if (file != null && file.isDirectory()) {
										(new navigate()).executeOnExecutor(
												AsyncTask.THREAD_POOL_EXECUTOR, file);
										ScrollView sv = (ScrollView) getActivity()
												.findViewById(R.id.game_scroller);
										sv.scrollTo(0, 0);
										vib.vibrate(50);
									} else if (view.getTag() == null) {
										vib.vibrate(250);

										if (games) {
											game_directory = heading;
											mPrefs.edit().putString(Config.pref_games, heading).apply();
											mCallback.onFolderSelected(Uri.fromFile(new File(game_directory)));
										} else {
											home_directory = heading.replace("/data", "");
											mPrefs.edit().putString(Config.pref_home, home_directory).apply();
											if (!DataDirectoryBIOS()) {
												showToastMessage(getActivity().getString(R.string.config_data, home_directory),
														Snackbar.LENGTH_LONG
												);
											}
											mCallback.onFolderSelected(Uri.fromFile(new File(home_directory)));
											JNIdc.config(home_directory);
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
					listView.addView(childview);
				}
				listView.invalidate();
			}
		}
	}
	
	private boolean DataDirectoryBIOS() {
		File data_directory = new File(home_directory, "data");
		if (!data_directory.exists() || !data_directory.isDirectory()) {
			data_directory.mkdirs();
			File bios = new File(home_directory, "dc_boot.bin");
			boolean success = bios.renameTo(new File(home_directory, "data/dc_boot.bin"));
			if (success) {
				File flash = new File(home_directory, "dc_flash.bin");
				success = flash.renameTo(new File(home_directory, "data/dc_flash.bin"));
			}
			return success;
		} else {
			File bios = new File(home_directory, "data/dc_boot.bin");
			File flash = new File(home_directory, "data/dc_flash.bin");
			if (bios.exists() && flash.exists()) {
				return true;
			} else {
				return false;
			}
		}
	}

	private void showToastMessage(String message, int duration) {
		ConstraintLayout layout = (ConstraintLayout) getActivity().findViewById(R.id.mainui_layout);
		Snackbar snackbar = Snackbar.make(layout, message, duration);
		View snackbarLayout = snackbar.getView();
		TextView textView = (TextView) snackbarLayout.findViewById(
				android.support.design.R.id.snackbar_text);
		textView.setGravity(Gravity.CENTER_VERTICAL);
		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR1)
			textView.setTextAlignment(View.TEXT_ALIGNMENT_GRAVITY);
		Drawable drawable;
		if (android.os.Build.VERSION.SDK_INT > Build.VERSION_CODES.M) {
			drawable = getResources().getDrawable(
					R.drawable.ic_subdirectory_arrow_right, getActivity().getTheme());
		} else {
			drawable = VectorDrawableCompat.create(getResources(),
					R.drawable.ic_subdirectory_arrow_right, getActivity().getTheme());
		}
		textView.setCompoundDrawablesWithIntrinsicBounds(drawable, null, null, null);
		textView.setCompoundDrawablePadding(getResources()
				.getDimensionPixelOffset(R.dimen.snackbar_icon_padding));
		snackbar.show();
	}
}
