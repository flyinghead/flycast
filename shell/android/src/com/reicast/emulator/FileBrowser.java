package com.reicast.emulator;

import java.io.File;
import java.io.FileOutputStream;
import java.io.FilenameFilter;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Comparator;
import java.util.List;
import java.util.Locale;

import org.apache.commons.io.comparator.CompositeFileComparator;
import org.apache.commons.io.comparator.LastModifiedFileComparator;
import org.apache.commons.io.comparator.SizeFileComparator;
import org.apache.commons.lang3.StringUtils;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.SharedPreferences;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.net.Uri;
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
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;
import android.widget.Toast;

import com.android.util.FileUtils;

public class FileBrowser extends Fragment {

	Vibrator vib;
	Drawable orig_bg;
	Activity parentActivity;
	boolean ImgBrowse;
	private boolean games;
	OnItemSelectedListener mCallback;

	private SharedPreferences mPrefs;
	private File sdcard = Environment.getExternalStorageDirectory();
	private String home_directory = sdcard + "/dc";
	private String game_directory = sdcard + "/";

	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);

		mPrefs = PreferenceManager.getDefaultSharedPreferences(getActivity());
		home_directory = mPrefs.getString("home_directory", home_directory);
		game_directory = mPrefs.getString("game_directory", game_directory);

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
		public void onGameSelected(Uri uri);

		public void onFolderSelected(Uri uri);
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
				InputStream png = parentActivity.getBaseContext().getAssets()
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

		File home = new File(home_directory);
		if (!home.exists() || !home.isDirectory()) {
			Toast.makeText(getActivity(), "Please configure a home directory",
					Toast.LENGTH_LONG).show();
		}

		if (!ImgBrowse) {
			navigate(sdcard);
		} else {
			generate(ExternalFiles(new File(game_directory)));
		}

		File bios = new File(home_directory, "data/dc_boot.bin");
		File flash = new File(home_directory, "data/dc_flash.bin");

		String msg = null;
		if (!bios.exists())
			msg = "BIOS Missing. The Dreamcast BIOS is required for this emulator to work. Place the BIOS file in "
					+ home_directory + "/data/dc_boot.bin";
		else if (!flash.exists())
			msg = "Flash Missing. The Dreamcast Flash is required for this emulator to work. Place the Flash file in "
					+ home_directory + "/data/dc_flash.bin";

		if (msg != null && ImgBrowse) {
			vib.vibrate(50);
			AlertDialog.Builder alertDialogBuilder = new AlertDialog.Builder(
					parentActivity);

			// set title
			alertDialogBuilder.setTitle("You have to provide the BIOS");

			// set dialog message
			alertDialogBuilder
					.setMessage(msg)
					.setCancelable(false)
					.setPositiveButton("Dismiss",
							new DialogInterface.OnClickListener() {
								public void onClick(DialogInterface dialog,
										int id) {
									// if this button is clicked, close
									// current activity
									parentActivity.finish();
								}
							})
					.setNegativeButton("Options",
							new DialogInterface.OnClickListener() {
								public void onClick(DialogInterface dialog,
										int id) {
									FileBrowser firstFragment = new FileBrowser();
									Bundle args = new Bundle();
									args.putBoolean("ImgBrowse", false);
									// specify ImgBrowse option. true = images, false = folders only
									args.putString("browse_entry", sdcard.toString());
									// specify a path for selecting folder options
									args.putBoolean("games_entry", games);
									// specify if the desired path is for games or data

									firstFragment.setArguments(args);
									// In case this activity was started with special instructions from
									// an Intent, pass the Intent's extras to the fragment as arguments
									// firstFragment.setArguments(getIntent().getExtras());

									// Add the fragment to the 'fragment_container' FrameLayout
									getActivity().getSupportFragmentManager()
											.beginTransaction()
											.replace(R.id.fragment_container, firstFragment, "MAIN_BROWSER")
											.addToBackStack(null).commit();
								}
							});

			// create alert dialog
			AlertDialog alertDialog = alertDialogBuilder.create();

			// show it
			alertDialog.show();
		}
	}

	class DirSort implements Comparator<File> {

		// Comparator interface requires defining compare method.
		public int compare(File filea, File fileb) {

			return ((filea.isFile() ? "a" : "b") + filea.getName().toLowerCase(
					Locale.getDefault()))
					.compareTo((fileb.isFile() ? "a" : "b")
							+ fileb.getName().toLowerCase(Locale.getDefault()));
		}
	}
	
	private List<File> ExternalFiles(File baseDirectory) {
		// allows the input of a base directory for storage selection
		final List<File> tFileList = new ArrayList<File>();
		Resources resources = getResources();
		// array of valid image file extensions
		String[] mediaTypes = resources.getStringArray(R.array.images);
		FilenameFilter[] filter = new FilenameFilter[mediaTypes.length];

		int i = 0;
		for (final String type : mediaTypes) {
			filter[i] = new FilenameFilter() {

				public boolean accept(File dir, String name) {
					if (dir.getName().startsWith(".") || name.startsWith(".")) {
						return false;
					} else {
						return StringUtils.endsWithIgnoreCase(name, "." + type);
					}
				}

			};
			i++;
		}

		FileUtils fileUtils = new FileUtils();
		File[] allMatchingFiles = fileUtils.listFilesAsArray(baseDirectory,
				filter, -1);
		for (File mediaFile : allMatchingFiles) {
			tFileList.add(mediaFile);
		}

		@SuppressWarnings("unchecked")
		CompositeFileComparator comparator = new CompositeFileComparator(
				SizeFileComparator.SIZE_REVERSE,
				LastModifiedFileComparator.LASTMODIFIED_REVERSE);
		comparator.sort(tFileList);

		return tFileList;
	}

	void generate(List<File> list) {
		LinearLayout v = (LinearLayout) parentActivity
				.findViewById(R.id.game_list);
		v.removeAllViews();
		
		((TextView) parentActivity.findViewById(R.id.text_cwd)).setText(R.string.games_listing);
		
		bootBiosItem(v);

		for (int i = 0; i < list.size(); i++) {
			final View childview = parentActivity.getLayoutInflater().inflate(
					R.layout.app_list_item, null, false);

			((TextView) childview.findViewById(R.id.item_name)).setText(list
					.get(i).getName());

			((ImageView) childview.findViewById(R.id.item_icon))
					.setImageResource(list.get(i) == null ? R.drawable.config
							: list.get(i).isDirectory() ? R.drawable.open_folder
									: list.get(i).getName()
											.toLowerCase(Locale.getDefault())
											.endsWith(".gdi") ? R.drawable.gdi
											: list.get(i)
													.getName()
													.toLowerCase(
															Locale.getDefault())
													.endsWith(".cdi") ? R.drawable.cdi
													: list.get(i)
															.getName()
															.toLowerCase(
																	Locale.getDefault())
															.endsWith(".chd") ? R.drawable.chd
															: R.drawable.disk_unknown);

			childview.setTag(list.get(i));

			orig_bg = childview.getBackground();

			// vw.findViewById(R.id.childview).setBackgroundColor(0xFFFFFFFF);

			childview.findViewById(R.id.childview).setOnClickListener(
					new OnClickListener() {
						public void onClick(View view) {
							File f = (File) view.getTag();
							vib.vibrate(50);
							mCallback.onGameSelected(f != null ? Uri
									.fromFile(f) : Uri.EMPTY);
							// Intent inte = new
							// Intent(Intent.ACTION_VIEW,f!=null?
							// Uri.fromFile(f):Uri.EMPTY,parentActivity.getBaseContext(),GL2JNIActivity.class);
							// FileBrowser.this.startActivity(inte);
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

			if (i == 0) {
				FrameLayout sepa = new FrameLayout(parentActivity);
				sepa.setBackgroundColor(0xFFA0A0A0);
				sepa.setPadding(0, 0, 0, 1);
				v.addView(sepa);
			}
			v.addView(childview);

			FrameLayout sep = new FrameLayout(parentActivity);
			sep.setBackgroundColor(0xFFA0A0A0);
			sep.setPadding(0, 0, 0, 1);
			v.addView(sep);
		}
	}
	
	private void bootBiosItem(LinearLayout v) {
		final View childview = parentActivity.getLayoutInflater().inflate(
				R.layout.app_list_item, null, false);

		((TextView) childview.findViewById(R.id.item_name))
		.setText("Boot Dreamcast Bios");

		childview.setTag(null);

		orig_bg = childview.getBackground();

		// vw.findViewById(R.id.childview).setBackgroundColor(0xFFFFFFFF);

		childview.findViewById(R.id.childview).setOnClickListener(
				new OnClickListener() {
					public void onClick(View view) {
						File f = (File) view.getTag();
						vib.vibrate(50);
						mCallback.onGameSelected(f != null ? Uri
								.fromFile(f) : Uri.EMPTY);
						// Intent inte = new
						// Intent(Intent.ACTION_VIEW,f!=null?
						// Uri.fromFile(f):Uri.EMPTY,parentActivity.getBaseContext(),GL2JNIActivity.class);
						// FileBrowser.this.startActivity(inte);
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


			FrameLayout sepa = new FrameLayout(parentActivity);
			sepa.setBackgroundColor(0xFFA0A0A0);
			sepa.setPadding(0, 0, 0, 1);
			v.addView(sepa);
		
		v.addView(childview);

		FrameLayout sep = new FrameLayout(parentActivity);
		sep.setBackgroundColor(0xFFA0A0A0);
		sep.setPadding(0, 0, 0, 1);
		v.addView(sep);
	}

	void navigate(final File root_sd) {
		LinearLayout v = (LinearLayout) parentActivity
				.findViewById(R.id.game_list);
		v.removeAllViews();

		ArrayList<File> list = new ArrayList<File>();

		((TextView) parentActivity.findViewById(R.id.text_cwd)).setText(root_sd
				.getAbsolutePath());

		File flist[] = root_sd.listFiles();

		File parent = root_sd.getParentFile();

		list.add(null);

		if (parent != null)
			list.add(parent);

		Arrays.sort(flist, new DirSort());

		for (int i = 0; i < flist.length; i++)
			list.add(flist[i]);

		for (int i = 0; i < list.size(); i++) {
			if (ImgBrowse) {
				if (list.get(i) != null && list.get(i).isFile())
					if (!list.get(i).getName().toLowerCase(Locale.getDefault())
							.endsWith(".gdi")
							&& !list.get(i).getName()
									.toLowerCase(Locale.getDefault())
									.endsWith(".cdi")
							&& !list.get(i).getName()
									.toLowerCase(Locale.getDefault())
									.endsWith(".chd"))
						continue;
			} else {
				if (list.get(i) != null && !list.get(i).isDirectory())
					continue;
			}
			final View childview = parentActivity.getLayoutInflater().inflate(
					R.layout.app_list_item, null, false);

			if (list.get(i) == null) {
				if (ImgBrowse == true)
					((TextView) childview.findViewById(R.id.item_name))
							.setText("BOOT BIOS");
				if (ImgBrowse == false)
					((TextView) childview.findViewById(R.id.item_name))
							.setText("SELECT CURRENT FOLDER");
			} else if (list.get(i) == parent)
				((TextView) childview.findViewById(R.id.item_name))
						.setText("..");
			else
				((TextView) childview.findViewById(R.id.item_name))
						.setText(list.get(i).getName());

			((ImageView) childview.findViewById(R.id.item_icon))
					.setImageResource(list.get(i) == null ? R.drawable.config
							: list.get(i).isDirectory() ? R.drawable.open_folder
									: list.get(i).getName()
											.toLowerCase(Locale.getDefault())
											.endsWith(".gdi") ? R.drawable.gdi
											: list.get(i)
													.getName()
													.toLowerCase(
															Locale.getDefault())
													.endsWith(".cdi") ? R.drawable.cdi
													: list.get(i)
															.getName()
															.toLowerCase(
																	Locale.getDefault())
															.endsWith(".chd") ? R.drawable.chd
															: R.drawable.disk_unknown);

			childview.setTag(list.get(i));

			orig_bg = childview.getBackground();

			// vw.findViewById(R.id.childview).setBackgroundColor(0xFFFFFFFF);

			childview.findViewById(R.id.childview).setOnClickListener(
					new OnClickListener() {
						public void onClick(View view) {
							File f = (File) view.getTag();

							if (f != null && f.isDirectory()) {
								navigate(f);
								ScrollView sv = (ScrollView) parentActivity
										.findViewById(R.id.game_scroller);
								sv.scrollTo(0, 0);
								vib.vibrate(50);
							} else if (f == null && !ImgBrowse) {
								vib.vibrate(50);

								mCallback.onFolderSelected(Uri
										.fromFile(new File(root_sd
												.getAbsolutePath())));
								vib.vibrate(250);
								
								if (games) {
									game_directory = root_sd.getAbsolutePath();
									mPrefs.edit()
											.putString("game_directory",
													game_directory).commit();
								} else {
									home_directory = root_sd.getAbsolutePath();
									mPrefs.edit()
											.putString("home_directory",
													home_directory).commit();
									File data_directory = new File(home_directory,
											"data");
									if (!data_directory.exists()
											|| !data_directory.isDirectory()) {
										data_directory.mkdirs();
									}
									JNIdc.config(home_directory);
								}

							} else if (ImgBrowse) {
								vib.vibrate(50);
								mCallback.onGameSelected(f != null ? Uri
										.fromFile(f) : Uri.EMPTY);
								// Intent inte = new
								// Intent(Intent.ACTION_VIEW,f!=null?
								// Uri.fromFile(f):Uri.EMPTY,parentActivity.getBaseContext(),GL2JNIActivity.class);
								// FileBrowser.this.startActivity(inte);
								vib.vibrate(250);
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

			if (i == 0) {
				FrameLayout sepa = new FrameLayout(parentActivity);
				sepa.setBackgroundColor(0xFFA0A0A0);
				sepa.setPadding(0, 0, 0, 1);
				v.addView(sepa);
			}
			v.addView(childview);

			FrameLayout sep = new FrameLayout(parentActivity);
			sep.setBackgroundColor(0xFFA0A0A0);
			sep.setPadding(0, 0, 0, 1);
			v.addView(sep);
		}
	}

}
