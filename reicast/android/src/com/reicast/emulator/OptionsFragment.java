package com.reicast.emulator;

import java.io.File;

import android.annotation.TargetApi;
import android.app.Activity;
import android.content.SharedPreferences;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.preference.PreferenceManager;
import android.support.v4.app.Fragment;
import android.util.Log;
import android.view.InputDevice;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.EditText;

@TargetApi(Build.VERSION_CODES.JELLY_BEAN)
public class OptionsFragment extends Fragment{
	
	Activity parentActivity;
	Button mainBrowse;
	OnClickListener mCallback;
	
	private SharedPreferences mPrefs;
	private File sdcard = Environment.getExternalStorageDirectory();
	private String home_directory = sdcard + "/Dreamcast";
	private String browse_entry = home_directory;
	
	 // Container Activity must implement this interface
    public interface OnClickListener {            
            public void onMainBrowseSelected(String browse_entry);
    }

    @Override
    public void onAttach(Activity activity) {
            super.onAttach(activity);

            // This makes sure that the container activity has implemented
            // the callback interface. If not, it throws an exception
            try {
                    mCallback = (OnClickListener) activity;
            } catch (ClassCastException e) {
                    throw new ClassCastException(activity.toString()
                                    + " must implement OnClickListener");
            }
            
            int joys[] =InputDevice.getDeviceIds();
            for(int i = 0;i<joys.length; i++){
            	Log.d("reidc", "InputDevice ID: "+joys[i]);
            	Log.d("reidc", "InputDevice Name: "+ InputDevice.getDevice(joys[i]).getName());
            }
    }
	
	 @Override
	    public View onCreateView(LayoutInflater inflater, ViewGroup container,
	                             Bundle savedInstanceState) {
	        // Inflate the layout for this fragment
	        return inflater.inflate(R.layout.options_fragment, container, false);
	    }
	 
	 @Override
	    public void onViewCreated(View view, Bundle savedInstanceState){
	        //setContentView(R.layout.activity_main);
		 
	    	parentActivity = getActivity();
	    	mainBrowse = (Button)getView().findViewById(R.id.browse_main_path);
	    	
	    	final EditText editBrowse = (EditText)getView().findViewById(R.id.main_path);
	    	mPrefs = PreferenceManager.getDefaultSharedPreferences(getActivity());
	    	home_directory = mPrefs.getString("home_directory", home_directory);
	    	editBrowse.setText(home_directory);
	    	
	    	mainBrowse.setOnClickListener(new View.OnClickListener() {
        		public void onClick(View view) {
        			if (editBrowse.getText() != null) {
        				browse_entry = editBrowse.getText().toString();
        			}
        			mCallback.onMainBrowseSelected(browse_entry);
        		}
	    	});
	 }
}
