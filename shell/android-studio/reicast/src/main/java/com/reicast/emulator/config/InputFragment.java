package com.reicast.emulator.config;

import android.app.Activity;
import android.content.Context;
import android.content.SharedPreferences;
import android.net.Uri;
import android.os.Bundle;
import android.os.Environment;
import android.os.Vibrator;
import android.preference.PreferenceManager;
import android.support.v4.app.Fragment;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.CompoundButton;
import android.widget.CompoundButton.OnCheckedChangeListener;
import android.widget.LinearLayout;
import android.widget.SeekBar;
import android.widget.TextView;

import com.reicast.emulator.Emulator;
import com.reicast.emulator.R;

import java.io.File;

public class InputFragment extends Fragment {
    private OnClickListener mCallback;

	private SharedPreferences mPrefs;
	private CompoundButton switchTouchVibrationEnabled;

	Vibrator vib;

	// Container Activity must implement this interface
	public interface OnClickListener {
		void onEditorSelected(Uri uri);
	}

    @Override @SuppressWarnings("deprecation")
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
    }

    @Override
    public void onAttach(Context context) {
        super.onAttach(context);

        // This makes sure that the container activity has implemented
        // the callback interface. If not, it throws an exception
        try {
            mCallback = (OnClickListener) context;
        } catch (ClassCastException e) {
            throw new ClassCastException(context.getClass().toString()
                    + " must implement OnClickListener");
        }
    }

	@Override
	public View onCreateView(LayoutInflater inflater, ViewGroup container,
							 Bundle savedInstanceState) {
		// Inflate the layout for this fragment
		return inflater.inflate(R.layout.input_fragment, container, false);
	}

	@Override
	public void onViewCreated(View view, Bundle savedInstanceState) {
		mPrefs = PreferenceManager.getDefaultSharedPreferences(getActivity());

		Config.vibrationDuration = mPrefs.getInt(Config.pref_vibrationDuration, 20);
		vib = (Vibrator) getActivity().getSystemService(Context.VIBRATOR_SERVICE);

		final Button buttonLaunchEditor = (Button) getView().findViewById(R.id.buttonLaunchEditor);
		buttonLaunchEditor.setOnClickListener(new View.OnClickListener() {
			public void onClick(View v) {
				buttonLaunchEditor.setEnabled(false);
                mCallback.onEditorSelected(Uri.EMPTY);
			}
		});

		buttonLaunchEditor.setEnabled(isBIOSAvailable());

		final TextView duration = (TextView) getView().findViewById(R.id.vibDuration_current);
		final LinearLayout vibLay = (LinearLayout) getView().findViewById(R.id.vibDuration_layout);
		final SeekBar vibSeek = (SeekBar) getView().findViewById(R.id.vib_seekBar);

		if (mPrefs.getBoolean(Config.pref_touchvibe, true)) {
			vibLay.setVisibility(View.VISIBLE);
		} else {
			vibLay.setVisibility(View.GONE);
		}

		duration.setText(String.valueOf(Config.vibrationDuration +  " ms"));
		vibSeek.setProgress(Config.vibrationDuration);

		vibSeek.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
			public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
				duration.setText(String.valueOf(progress + 5 + " ms"));
			}

			public void onStartTrackingTouch(SeekBar seekBar) {

			}

			public void onStopTrackingTouch(SeekBar seekBar) {
				int progress = seekBar.getProgress() + 5;
				mPrefs.edit().putInt(Config.pref_vibrationDuration, progress).apply();
				Config.vibrationDuration = progress;
				vib.vibrate(progress);
			}
		});

		OnCheckedChangeListener touch_vibration = new OnCheckedChangeListener() {
			public void onCheckedChanged(CompoundButton buttonView,
										 boolean isChecked) {
				mPrefs.edit().putBoolean(Config.pref_touchvibe, isChecked).apply();
				vibLay.setVisibility( isChecked ? View.VISIBLE : View.GONE );
			}
		};
		switchTouchVibrationEnabled = (CompoundButton) getView().findViewById(
				R.id.switchTouchVibrationEnabled);
		boolean vibrate = mPrefs.getBoolean(Config.pref_touchvibe, true);
		if (vibrate) {
			switchTouchVibrationEnabled.setChecked(true);
		} else {
			switchTouchVibrationEnabled.setChecked(false);
		}
		switchTouchVibrationEnabled.setOnCheckedChangeListener(touch_vibration);
		updateVibration();
	}

	@Override
	public void onResume() {
		super.onResume();
		Button buttonLaunchEditor = (Button) getView().findViewById(R.id.buttonLaunchEditor);
		buttonLaunchEditor.setEnabled(isBIOSAvailable());
	}

	private boolean isBIOSAvailable() {
		String home_directory = mPrefs.getString(Config.pref_home,
				Environment.getExternalStorageDirectory().getAbsolutePath());
		return new File(home_directory, "data/dc_flash.bin").exists()
				|| mPrefs.getBoolean(Emulator.pref_usereios, false);
	}

	private void updateVibration() {
		boolean touchVibrationEnabled = mPrefs.getBoolean(Config.pref_touchvibe, true);
		switchTouchVibrationEnabled.setChecked(touchVibrationEnabled);
	}

}
