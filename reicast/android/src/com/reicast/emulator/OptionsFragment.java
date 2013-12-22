package com.reicast.emulator;

import android.annotation.TargetApi;
import android.app.Activity;
import android.os.Build;
import android.os.Bundle;
import android.support.v4.app.Fragment;
import android.util.Log;
import android.view.InputDevice;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;

@TargetApi(Build.VERSION_CODES.JELLY_BEAN)
public class OptionsFragment extends Fragment{
	
	Activity parentActivity;
	Button mainBrowse;
	OnClickListener mCallback;
	
	 // Container Activity must implement this interface
    public interface OnClickListener {            
            public void onMainBrowseSelected();
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
	    	mainBrowse.setOnClickListener(new View.OnClickListener() {
        		public void onClick(View view) {
        			mCallback.onMainBrowseSelected();
        		}
	    	});
	 }
}
