package com.reicast.emulator;

import java.io.File;

import com.example.newdc.JNIdc;

import android.app.AlertDialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.SharedPreferences;
import android.net.Uri;
import android.os.Bundle;
import android.os.Environment;
import android.preference.PreferenceManager;
import android.support.v4.app.FragmentActivity;
import android.util.Log;
import android.view.Menu;
import android.view.MotionEvent;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.View.OnTouchListener;

import com.example.newdc.JNIdc;

public class MainActivity extends FragmentActivity implements
			FileBrowser.OnItemSelectedListener, 
			OptionsFragment.OnClickListener{
	
	private SharedPreferences mPrefs;
	private File sdcard = Environment.getExternalStorageDirectory();
	private String home_directory = sdcard + "/Dreamcast";

	@Override
    public void onCreate(Bundle savedInstanceState) {
            super.onCreate(savedInstanceState);
            setContentView(R.layout.mainuilayout_fragment);

            // Check that the activity is using the layout version with
            // the fragment_container FrameLayout
            if (findViewById(R.id.fragment_container) != null) {

                    // However, if we're being restored from a previous state,
                    // then we don't need to do anything and should return or else
                    // we could end up with overlapping fragments.
                    if (savedInstanceState != null) {
                            return;
                    }

                    // Create a new Fragment to be placed in the activity layout
                    FileBrowser firstFragment = new FileBrowser();
                    Bundle args = new Bundle();
                    args.putBoolean("ImgBrowse", true);				// specify ImgBrowse option. true = images, false = folders only
                    firstFragment.setArguments(args);
                    // In case this activity was started with special instructions from
                    // an
                    // Intent, pass the Intent's extras to the fragment as arguments
                    // firstFragment.setArguments(getIntent().getExtras());

                    // Add the fragment to the 'fragment_container' FrameLayout
                    getSupportFragmentManager().beginTransaction()
                                    .add(R.id.fragment_container, firstFragment).commit();
            }
            
            findViewById(R.id.config).setOnClickListener(
                	new OnClickListener() {
                		public void onClick(View view) {
                			OptionsFragment optsFrag = (OptionsFragment)getSupportFragmentManager().findFragmentByTag("OPTIONS_FRAG");
                			if(optsFrag != null){
	                			if(optsFrag.isVisible()){
	                				return;	                				
	                			}
                			}
                			optsFrag = new OptionsFragment();
            				getSupportFragmentManager().beginTransaction()
            				.replace(R.id.fragment_container,optsFrag, "OPTIONS_FRAG").addToBackStack(null).commit();
                			/*AlertDialog.Builder alertDialogBuilder = new AlertDialog.Builder(
                				MainActivity.this);
                 
                			// set title
                			alertDialogBuilder.setTitle("Configure");
                 
                			// set dialog message
                			alertDialogBuilder
                				.setMessage("No configuration for now :D")
                				.setCancelable(false)
                				.setPositiveButton("Oh well",new DialogInterface.OnClickListener() {
                					public void onClick(DialogInterface dialog,int id) {
                						//FileBrowser.this.finish();
                					}
                				  });
                 
                				// create alert dialog
                				AlertDialog alertDialog = alertDialogBuilder.create();
                 
                				// show it
                				alertDialog.show();*/
                		}

                	});
            
            findViewById(R.id.about).setOnTouchListener(
                	new OnTouchListener() 
                	{
                		public boolean onTouch(View v, MotionEvent event)
                		{
                			if (event.getActionMasked()==MotionEvent.ACTION_DOWN)
                			{
    	            			//vib.vibrate(50);
    	            			AlertDialog.Builder alertDialogBuilder = new AlertDialog.Builder(
    	            					MainActivity.this);
    	             
    	            			// set title
    	            			alertDialogBuilder.setTitle("About reicast");
    	             
    	            			// set dialog message
    	            			alertDialogBuilder
    	            				.setMessage("reicast is a dreamcast emulator")
    	            				.setCancelable(false)
    	            				.setPositiveButton("Dismiss",new DialogInterface.OnClickListener() {
    	            					public void onClick(DialogInterface dialog,int id) {
    	            						// if this button is clicked, close
    	            						// current activity
    	            						//FileBrowser.this.finish();
    	            					}
    	            				  });
    	             
    	            				// create alert dialog
    	            				AlertDialog alertDialog = alertDialogBuilder.create();
    	             
    	            				// show it
    	            				alertDialog.show();
    	            				return true;
                				}
                				else
                        			return false;
                			}
                	});
            
            mPrefs = PreferenceManager.getDefaultSharedPreferences(this);
            home_directory = mPrefs.getString("home_directory", home_directory);
            JNIdc.config(home_directory);

    }
	
	
	
	public void onGameSelected(Uri uri){
		Intent inte = new Intent(Intent.ACTION_VIEW,uri,getBaseContext(),GL2JNIActivity.class);
		startActivity(inte);
	}
	
	public void onFolderSelected(Uri uri){
		FileBrowser browserFrag = (FileBrowser)getSupportFragmentManager().findFragmentByTag("MAIN_BROWSER");
		if(browserFrag != null){
			if(browserFrag.isVisible()){
				
				Log.d("reicast", "Main folder: "+uri.toString());
				//return;	                				
			}
		}
		
		OptionsFragment optsFrag = new OptionsFragment();
		getSupportFragmentManager().beginTransaction()
		.replace(R.id.fragment_container,optsFrag, "OPTIONS_FRAG").commit();
		return;
	}
	
	public void onMainBrowseSelected(String browse_entry){
		FileBrowser firstFragment = new FileBrowser();
        Bundle args = new Bundle();
        args.putBoolean("ImgBrowse", false);
        args.putString("browse_entry", browse_entry);
        // specify ImgBrowse option. true = images, false = folders only
        firstFragment.setArguments(args);
        // In case this activity was started with special instructions from
        // an Intent, pass the Intent's extras to the fragment as arguments
        // firstFragment.setArguments(getIntent().getExtras());

        // Add the fragment to the 'fragment_container' FrameLayout
        getSupportFragmentManager().beginTransaction()
                        .replace(R.id.fragment_container, firstFragment, "MAIN_BROWSER").addToBackStack(null).commit();
	}
	
	@Override
    public boolean onCreateOptionsMenu(Menu menu) {
    	getMenuInflater().inflate(R.menu.activity_main, menu);
        return true;
    }
}
