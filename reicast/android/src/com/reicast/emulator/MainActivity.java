package com.reicast.emulator;

import android.os.Bundle;
import android.support.v4.app.FragmentActivity;
import android.view.Menu;

public class MainActivity extends FragmentActivity {

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

                    // In case this activity was started with special instructions from
                    // an
                    // Intent, pass the Intent's extras to the fragment as arguments
                    // firstFragment.setArguments(getIntent().getExtras());

                    // Add the fragment to the 'fragment_container' FrameLayout
                    getSupportFragmentManager().beginTransaction()
                                    .add(R.id.fragment_container, firstFragment).commit();
            }

    }
	
	
	@Override
    public boolean onCreateOptionsMenu(Menu menu) {
    	getMenuInflater().inflate(R.menu.activity_main, menu);
        return true;
    }
}
