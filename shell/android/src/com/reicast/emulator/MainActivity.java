package com.reicast.emulator;

import java.io.File;
import java.util.ArrayList;

import android.app.AlertDialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.SharedPreferences;
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
	private File sdcard = Environment.getExternalStorageDirectory();
	private String home_directory = sdcard + "/dc";

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
			getSupportFragmentManager().beginTransaction()
					.add(R.id.fragment_container, firstFragment).commit();
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
			// About
			navDrawerItems.add(new NavDrawerItem(navMenuTitles[3], navMenuIcons
					.getResourceId(3, 0)));

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
					R.string.app_name, // nav drawer open - description for
										// accessibility
					R.string.app_name // nav drawer close - description for
										// accessibility
			) {
				public void onDrawerClosed(View view) {
					getActionBar().setTitle(mTitle);
					// calling onPrepareOptionsMenu() to show action bar
					// icons
					invalidateOptionsMenu();
				}

				public void onDrawerOpened(View drawerView) {
					getActionBar().setTitle(mDrawerTitle);
					// calling onPrepareOptionsMenu() to hide action bar
					// icons
					invalidateOptionsMenu();
				}
			};
			mDrawerLayout.setDrawerListener(mDrawerToggle);

			// if (savedInstanceState == null) {
			// displayView(0);
			//
			// }
		} else {

			findViewById(R.id.options).setOnClickListener(new OnClickListener() {
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
							.replace(R.id.fragment_container, optionsFrag,
									"OPTIONS_FRAG").addToBackStack(null)
							.commit();
					/*
					 * AlertDialog.Builder alertDialogBuilder = new
					 * AlertDialog.Builder( MainActivity.this);
					 * 
					 * // set title alertDialogBuilder.setTitle("Configure");
					 * 
					 * // set dialog message alertDialogBuilder
					 * .setMessage("No configuration for now :D")
					 * .setCancelable(false) .setPositiveButton("Oh well",new
					 * DialogInterface.OnClickListener() { public void
					 * onClick(DialogInterface dialog,int id) {
					 * //FileBrowser.this.finish(); } });
					 * 
					 * // create alert dialog AlertDialog alertDialog =
					 * alertDialogBuilder.create();
					 * 
					 * // show it alertDialog.show();
					 */
				}

			});
			
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

			findViewById(R.id.about).setOnTouchListener(new OnTouchListener() {
				public boolean onTouch(View v, MotionEvent event) {
					if (event.getActionMasked() == MotionEvent.ACTION_DOWN) {
						// vib.vibrate(50);
						AlertDialog.Builder alertDialogBuilder = new AlertDialog.Builder(
								MainActivity.this);

						// set title
						alertDialogBuilder.setTitle("About reicast");

						// set dialog message
						alertDialogBuilder
								.setMessage("reicast is a dreamcast emulator")
								.setCancelable(false)
								.setPositiveButton("Dismiss",
										new DialogInterface.OnClickListener() {
											public void onClick(
													DialogInterface dialog,
													int id) {
												// if this button is clicked,
												// close
												// current activity
												// FileBrowser.this.finish();
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
		}

	}

	public void onGameSelected(Uri uri) {
		Intent inte = new Intent(Intent.ACTION_VIEW, uri, getBaseContext(),
				GL2JNIActivity.class);
		startActivity(inte);
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
			fragment = null;
			// vib.vibrate(50);
			AlertDialog.Builder alertDialogBuilder = new AlertDialog.Builder(
					MainActivity.this);

			// set title
			alertDialogBuilder.setTitle("About reicast");

			// set dialog message
			alertDialogBuilder
					.setMessage("reicast is a dreamcast emulator")
					.setCancelable(false)
					.setPositiveButton("Dismiss",
							new DialogInterface.OnClickListener() {
								public void onClick(DialogInterface dialog,
										int id) {
									// if this button is clicked, close
									// current activity
									// FileBrowser.this.finish();
								}
							});

			// create alert dialog
			AlertDialog alertDialog = alertDialogBuilder.create();

			// show it
			alertDialog.show();
			break;

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
}
