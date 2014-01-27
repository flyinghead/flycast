package com.reicast.emulator;

import java.io.File;
import java.util.ArrayList;

import android.annotation.SuppressLint;
import android.app.AlertDialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager.NameNotFoundException;
import android.content.res.Configuration;
import android.content.res.TypedArray;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.preference.PreferenceManager;
import android.support.v4.app.ActionBarDrawerToggle;
import android.support.v4.app.Fragment;
import android.support.v4.app.FragmentActivity;
import android.support.v4.app.FragmentManager;
import android.support.v4.widget.DrawerLayout;
import android.util.Log;
import android.view.KeyEvent;
import android.view.Menu;
import android.view.MenuItem;
import android.view.MotionEvent;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.View.OnTouchListener;
import android.widget.AdapterView;
import android.widget.ListView;

public class MainActivity extends FragmentActivity implements
		FileBrowser.OnItemSelectedListener, OptionsFragment.OnClickListener {

	private SharedPreferences mPrefs;
	private static File sdcard = Environment.getExternalStorageDirectory();
	public static String home_directory = sdcard + "/dc";

	private DrawerLayout mDrawerLayout;
	private ListView mDrawerList;
	private ActionBarDrawerToggle mDrawerToggle;

	// nav drawer title
	private CharSequence mDrawerTitle;

	// used to store app title
	private CharSequence mTitle;

	// slide menu items
	private String[] navMenuTitles;
	private TypedArray navMenuIcons;

	private ArrayList<NavDrawerItem> navDrawerItems;
	private NavDrawerListAdapter adapter;

	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		setContentView(R.layout.mainuilayout_fragment);

		mPrefs = PreferenceManager.getDefaultSharedPreferences(this);
		home_directory = mPrefs.getString("home_directory", home_directory);
		JNIdc.config(home_directory);

		// Check that the activity is using the layout version with
		// the fragment_container FrameLayout
		if (findViewById(R.id.fragment_container) != null) {

			// However, if we're being restored from a previous state,
			// then we don't need to do anything and should return or else
			// we could end up with overlapping fragments.
			if (Build.VERSION.SDK_INT < Build.VERSION_CODES.HONEYCOMB_MR1) {
				if (savedInstanceState != null) {
					return;
				}
			}

			// Create a new Fragment to be placed in the activity layout
			FileBrowser firstFragment = new FileBrowser();
			Bundle args = new Bundle();
			args.putBoolean("ImgBrowse", true);
			args.putString("browse_entry", null);
			// specify a path for selecting folder options
			args.putBoolean("games_entry", false);
			// specify if the desired path is for games or data
			firstFragment.setArguments(args);
			// In case this activity was started with special instructions from
			// an
			// Intent, pass the Intent's extras to the fragment as arguments
			// firstFragment.setArguments(getIntent().getExtras());

			// Add the fragment to the 'fragment_container' FrameLayout
			getSupportFragmentManager()
					.beginTransaction()
					.replace(R.id.fragment_container, firstFragment,
							"MAIN_BROWSER").commit();
		}

		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.HONEYCOMB_MR1) {

			navMenuTitles = getResources().getStringArray(
					R.array.nav_drawer_items);

			// nav drawer icons from resources
			navMenuIcons = getResources().obtainTypedArray(
					R.array.nav_drawer_icons);

			mDrawerLayout = (DrawerLayout) findViewById(R.id.drawer_layout);
			mDrawerList = (ListView) findViewById(R.id.list_slidermenu);

			navDrawerItems = new ArrayList<NavDrawerItem>();

			// adding nav drawer items to array
			// Browser
			navDrawerItems.add(new NavDrawerItem(navMenuTitles[0], navMenuIcons
					.getResourceId(0, 0)));
			// Settings
			navDrawerItems.add(new NavDrawerItem(navMenuTitles[1], navMenuIcons
					.getResourceId(1, 0)));
			// Paths
			navDrawerItems.add(new NavDrawerItem(navMenuTitles[2], navMenuIcons
					.getResourceId(2, 0)));
			// Input
			navDrawerItems.add(new NavDrawerItem(navMenuTitles[3], navMenuIcons
					.getResourceId(3, 0)));
			// About
			navDrawerItems.add(new NavDrawerItem(navMenuTitles[4], navMenuIcons
					.getResourceId(4, 0)));
			// Rate
			navDrawerItems.add(new NavDrawerItem(navMenuTitles[5], navMenuIcons
					.getResourceId(5, 0)));

			// Recycle the typed array
			navMenuIcons.recycle();

			mDrawerList.setOnItemClickListener(new SlideMenuClickListener());

			// setting the nav drawer list adapter
			adapter = new NavDrawerListAdapter(getApplicationContext(),
					navDrawerItems);
			mDrawerList.setAdapter(adapter);

			// enabling action bar app icon and behaving it as toggle button
			getActionBar().setDisplayHomeAsUpEnabled(true);
			getActionBar().setHomeButtonEnabled(true);

			mDrawerToggle = new ActionBarDrawerToggle(this, mDrawerLayout,
					R.drawable.ic_drawer, // nav menu toggle icon
					R.string.app_name, // nav drawer open title
					R.string.app_name // nav drawer close title
			) {
				@SuppressLint("NewApi")
				public void onDrawerClosed(View view) {
					getActionBar().setTitle(mTitle);
					// calling onPrepareOptionsMenu() to show action bar
					// icons
					invalidateOptionsMenu();
				}

				@SuppressLint("NewApi")
				public void onDrawerOpened(View drawerView) {
					getActionBar().setTitle(mDrawerTitle);
					// calling onPrepareOptionsMenu() to hide action bar
					// icons
					invalidateOptionsMenu();
				}
			};
			mDrawerLayout.setDrawerListener(mDrawerToggle);

			if (savedInstanceState == null) {
				displayView(0);
			}
		} else {

			findViewById(R.id.config).setOnClickListener(new OnClickListener() {
				public void onClick(View view) {
					ConfigureFragment configFrag = (ConfigureFragment) getSupportFragmentManager()
							.findFragmentByTag("CONFIG_FRAG");
					if (configFrag != null) {
						if (configFrag.isVisible()) {
							return;
						}
					}
					configFrag = new ConfigureFragment();
					getSupportFragmentManager()
							.beginTransaction()
							.replace(R.id.fragment_container, configFrag,
									"CONFIG_FRAG").addToBackStack(null)
							.commit();
				}

			});

			findViewById(R.id.options).setOnClickListener(
					new OnClickListener() {
						public void onClick(View view) {
							OptionsFragment optionsFrag = (OptionsFragment) getSupportFragmentManager()
									.findFragmentByTag("OPTIONS_FRAG");
							if (optionsFrag != null) {
								if (optionsFrag.isVisible()) {
									return;
								}
							}
							optionsFrag = new OptionsFragment();
							getSupportFragmentManager()
									.beginTransaction()
									.replace(R.id.fragment_container,
											optionsFrag, "OPTIONS_FRAG")
									.addToBackStack(null).commit();
						}

					});

			findViewById(R.id.input).setOnClickListener(new OnClickListener() {
				public void onClick(View view) {
					InputFragment inputFrag = (InputFragment) getSupportFragmentManager()
							.findFragmentByTag("INPUT_FRAG");
					if (inputFrag != null) {
						if (inputFrag.isVisible()) {
							return;
						}
					}
					inputFrag = new InputFragment();
					getSupportFragmentManager()
							.beginTransaction()
							.replace(R.id.fragment_container, inputFrag,
									"INPUT_FRAG").addToBackStack(null).commit();
				}

			});

			findViewById(R.id.about).setOnTouchListener(new OnTouchListener() {
				public boolean onTouch(View v, MotionEvent event) {
					if (event.getActionMasked() == MotionEvent.ACTION_DOWN) {
						// vib.vibrate(50);
						AlertDialog.Builder alertDialogBuilder = new AlertDialog.Builder(
								MainActivity.this);

						// set title
						alertDialogBuilder.setTitle(getString(R.string.about_title));

						String versionName = "";
						try {
							PackageInfo pInfo = getPackageManager()
									.getPackageInfo(getPackageName(), 0);
							versionName = pInfo.versionName;
						} catch (NameNotFoundException e) {
							e.printStackTrace();
						}

						alertDialogBuilder
							.setMessage(getString(R.string.about_text, versionName))
							.setCancelable(false)
							.setPositiveButton("Dismiss",
									new DialogInterface.OnClickListener() {
										public void onClick(DialogInterface dialog, int id) {
											dialog.dismiss();
										}
									});

						// create alert dialog
						AlertDialog alertDialog = alertDialogBuilder.create();

						// show it
						alertDialog.show();
						return true;
					} else
						return false;
				}
			});
			findViewById(R.id.rate).setOnTouchListener(new OnTouchListener() {
				public boolean onTouch(View v, MotionEvent event) {
					if (event.getActionMasked() == MotionEvent.ACTION_DOWN) {
						// vib.vibrate(50);
						startActivity(new Intent(Intent.ACTION_VIEW, Uri
								.parse("market://details?id="
										+ getPackageName())));
						return true;
					} else
						return false;
				}
			});
		}

	}

	public static boolean isBiosExisting() {
		File bios = new File(home_directory, "data/dc_boot.bin");
		return bios.exists();
	}

	public static boolean isFlashExisting() {
		File flash = new File(home_directory, "data/dc_flash.bin");
		return flash.exists();
	}

	public void onGameSelected(Uri uri) {
		String msg = null;
		if (!isBiosExisting())
			msg = getString(R.string.missing_bios, home_directory);
		else if (!isFlashExisting())
			msg = getString(R.string.missing_flash, home_directory);

		if (msg != null) {
			AlertDialog.Builder alertDialogBuilder = new AlertDialog.Builder(
					this);

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
									// MainActivity.this.finish();
								}
							})
					.setNegativeButton("Options",
							new DialogInterface.OnClickListener() {
								public void onClick(DialogInterface dialog,
										int id) {
									FileBrowser firstFragment = new FileBrowser();
									Bundle args = new Bundle();
									// args.putBoolean("ImgBrowse", false);
									// specify ImgBrowse option. true = images,
									// false = folders only
									args.putString("browse_entry",
											sdcard.toString());
									// specify a path for selecting folder
									// options
									args.putBoolean("games_entry", false);
									// selecting a BIOS folder, so this is not
									// games

									firstFragment.setArguments(args);
									// In case this activity was started with
									// special instructions from
									// an Intent, pass the Intent's extras to
									// the fragment as arguments
									// firstFragment.setArguments(getIntent().getExtras());

									// Add the fragment to the
									// 'fragment_container' FrameLayout
									getSupportFragmentManager()
											.beginTransaction()
											.replace(R.id.fragment_container,
													firstFragment,
													"MAIN_BROWSER")
											.addToBackStack(null).commit();
								}
							});

			// create alert dialog
			AlertDialog alertDialog = alertDialogBuilder.create();

			// show it
			alertDialog.show();
		} else {
			Intent inte = new Intent(Intent.ACTION_VIEW, uri, getBaseContext(),
					GL2JNIActivity.class);
			startActivity(inte);
		}
	}

	public void onFolderSelected(Uri uri) {
		FileBrowser browserFrag = (FileBrowser) getSupportFragmentManager()
				.findFragmentByTag("MAIN_BROWSER");
		if (browserFrag != null) {
			if (browserFrag.isVisible()) {

				Log.d("reicast", "Main folder: " + uri.toString());
				// return;
			}
		}

		OptionsFragment optsFrag = new OptionsFragment();
		getSupportFragmentManager().beginTransaction()
				.replace(R.id.fragment_container, optsFrag, "OPTIONS_FRAG")
				.commit();
		return;
	}

	public void onMainBrowseSelected(String path_entry, boolean games) {
		FileBrowser firstFragment = new FileBrowser();
		Bundle args = new Bundle();
		args.putBoolean("ImgBrowse", false);
		// specify ImgBrowse option. true = images, false = folders only
		args.putString("browse_entry", path_entry);
		// specify a path for selecting folder options
		args.putBoolean("games_entry", games);
		// specify if the desired path is for games or data

		firstFragment.setArguments(args);
		// In case this activity was started with special instructions from
		// an Intent, pass the Intent's extras to the fragment as arguments
		// firstFragment.setArguments(getIntent().getExtras());

		// Add the fragment to the 'fragment_container' FrameLayout
		getSupportFragmentManager()
				.beginTransaction()
				.replace(R.id.fragment_container, firstFragment, "MAIN_BROWSER")
				.addToBackStack(null).commit();
	}

	/**
	 * Slide menu item click listener
	 * */
	private class SlideMenuClickListener implements
			ListView.OnItemClickListener {

		public void onItemClick(AdapterView<?> parent, View view, int position,
				long id) {
			// TODO Auto-generated method stub
			displayView(position);
		}

	}

	/**
	 * Diplaying fragment view for selected nav drawer list item
	 * */
	private void displayView(int position) {
		// update the main content by replacing fragments
		Fragment fragment = null;
		String frag_tag = "";
		switch (position) {
		case 0:
			fragment = new FileBrowser();
			Bundle args = new Bundle();
			args.putBoolean("ImgBrowse", true);
			args.putString("browse_entry", null);
			// specify a path for selecting folder options
			args.putBoolean("games_entry", false);
			// specify if the desired path is for games or data
			fragment.setArguments(args);
			// In case this activity was started with special instructions from
			// an
			// Intent, pass the Intent's extras to the fragment as arguments
			// firstFragment.setArguments(getIntent().getExtras());
			frag_tag = "MAIN_BROWSER";
			break;
		case 1:
			fragment = (ConfigureFragment) getSupportFragmentManager()
					.findFragmentByTag("CONFIG_FRAG");
			if (fragment != null) {
				if (fragment.isVisible()) {
					return;
				}
			}
			fragment = new ConfigureFragment();
			frag_tag = "CONFIG_FRAG";
			break;
		case 2:
			fragment = (OptionsFragment) getSupportFragmentManager()
					.findFragmentByTag("OPTIONS_FRAG");
			if (fragment != null) {
				if (fragment.isVisible()) {
					return;
				}
			}
			fragment = new OptionsFragment();
			frag_tag = "OPTIONS_FRAG";
			break;
		case 3:
			fragment = (InputFragment) getSupportFragmentManager()
					.findFragmentByTag("INPUT_FRAG");
			if (fragment != null) {
				if (fragment.isVisible()) {
					return;
				}
			}
			fragment = new InputFragment();
			frag_tag = "INPUT_FRAG";
			break;
		case 4:
			fragment = null;
			// vib.vibrate(50);
			AlertDialog.Builder alertDialogBuilder = new AlertDialog.Builder(
					MainActivity.this);

			// set title
			alertDialogBuilder.setTitle(getString(R.string.about_title));

			String versionName = "";
			try {
				PackageInfo pInfo = getPackageManager().getPackageInfo(
						getPackageName(), 0);
				versionName = pInfo.versionName;
			} catch (NameNotFoundException e) {
				e.printStackTrace();
			}

			alertDialogBuilder
					.setMessage(getString(R.string.about_text, versionName))
					.setCancelable(false)
					.setPositiveButton("Dismiss",
							new DialogInterface.OnClickListener() {
								public void onClick(DialogInterface dialog,
										int id) {
									dialog.dismiss();
								}
							});

			// create alert dialog
			AlertDialog alertDialog = alertDialogBuilder.create();

			// show it
			alertDialog.show();
			break;

		case 5:
			startActivity(new Intent(Intent.ACTION_VIEW,
				Uri.parse("market://details?id=" + getPackageName())));
		default:
			break;
		}

		if (fragment != null) {
			FragmentManager fragmentManager = getSupportFragmentManager();
			fragmentManager.beginTransaction()
					.replace(R.id.fragment_container, fragment, frag_tag)
					.commit();

			// update selected item and title, then close the drawer
			mDrawerList.setItemChecked(position, true);
			mDrawerList.setSelection(position);
			setTitle(navMenuTitles[position]);
			mDrawerLayout.closeDrawer(mDrawerList);
		} else {
			mDrawerLayout.closeDrawer(mDrawerList);
			// error in creating fragment
			Log.e("MainActivity",
					"Error in creating fragment - possibly a popup");
		}
	}

	@Override
	public boolean onCreateOptionsMenu(Menu menu) {
		getMenuInflater().inflate(R.menu.activity_main, menu);
		return true;
	}

	@Override
	public boolean onOptionsItemSelected(MenuItem item) {
		// toggle nav drawer on selecting action bar app icon/title
		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.HONEYCOMB_MR1) {
			if (mDrawerToggle.onOptionsItemSelected(item)) {
				return true;
			}
			// Handle action bar actions click
		}
		switch (item.getItemId()) {
		default:
			return super.onOptionsItemSelected(item);
		}
	}

	/***
	 * Called when invalidateOptionsMenu() is triggered
	 */
	@Override
	public boolean onPrepareOptionsMenu(Menu menu) {
		return super.onPrepareOptionsMenu(menu);
	}

	@SuppressLint("NewApi")
	@Override
	public void setTitle(CharSequence title) {
		mTitle = title;
		getActionBar().setTitle(mTitle);
	}

	/**
	 * When using the ActionBarDrawerToggle, you must call it during
	 * onPostCreate() and onConfigurationChanged()...
	 */

	@Override
	protected void onPostCreate(Bundle savedInstanceState) {
		super.onPostCreate(savedInstanceState);
		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.HONEYCOMB_MR1) {
			if (mDrawerToggle != null) {
				mDrawerToggle.syncState();
			}
		}
	}

	@Override
	public void onConfigurationChanged(Configuration newConfig) {
		super.onConfigurationChanged(newConfig);
		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.HONEYCOMB_MR1) {
			mDrawerToggle.onConfigurationChanged(newConfig);
		}

	}

	@Override
	public boolean onKeyDown(int keyCode, KeyEvent event) {
		if (keyCode == KeyEvent.KEYCODE_BACK) {
			Fragment fragment = (FileBrowser) getSupportFragmentManager()
					.findFragmentByTag("MAIN_BROWSER");
			if (fragment != null && fragment.isVisible()) {
				MainActivity.this.finish();
				return true;
			} else {
				if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.HONEYCOMB_MR1) {
					displayView(0);
				} else {
					fragment = new FileBrowser();
					Bundle args = new Bundle();
					args.putBoolean("ImgBrowse", true);
					args.putString("browse_entry", null);
					args.putBoolean("games_entry", false);
					fragment.setArguments(args);
					getSupportFragmentManager()
							.beginTransaction()
							.replace(R.id.fragment_container, fragment,
									"MAIN_BROWSER").commit();
				}
				return true;
			}

		}

		return super.onKeyDown(keyCode, event);
	}

	@Override
	protected void onPause() {
		super.onPause();
		InputFragment fragment = (InputFragment) getSupportFragmentManager()
				.findFragmentByTag("INPUT_FRAG");
		if (fragment != null && fragment.isVisible()) {
			if (fragment.moga != null) {
				fragment.moga.onPause();
			}
		}
	}

	@Override
	protected void onDestroy() {
		super.onDestroy();
		InputFragment fragment = (InputFragment) getSupportFragmentManager()
				.findFragmentByTag("INPUT_FRAG");
		if (fragment != null && fragment.isVisible()) {
			if (fragment.moga != null) {
				fragment.moga.onDestroy();
			}
		}
	}

	@Override
	protected void onResume() {
		super.onResume();
		InputFragment fragment = (InputFragment) getSupportFragmentManager()
				.findFragmentByTag("INPUT_FRAG");
		if (fragment != null && fragment.isVisible()) {
			if (fragment.moga != null) {
				fragment.moga.onResume();
			}
		}
	}
}
