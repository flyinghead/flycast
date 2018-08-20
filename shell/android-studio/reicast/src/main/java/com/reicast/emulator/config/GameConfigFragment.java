package com.reicast.emulator.config;

import android.app.Activity;
import android.content.Context;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.preference.PreferenceManager;
import android.support.v4.app.Fragment;
import android.text.Editable;
import android.text.TextWatcher;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.CompoundButton;
import android.widget.CompoundButton.OnCheckedChangeListener;
import android.widget.EditText;
import android.widget.SeekBar;
import android.widget.SeekBar.OnSeekBarChangeListener;

import com.reicast.emulator.Emulator;
import com.reicast.emulator.R;

public class GameConfigFragment extends Fragment {

	private SharedPreferences mPrefs;

	// Container Activity must implement this interface
	public interface OnClickListener {
		void onMainBrowseSelected(boolean browse, String path_entry, boolean games, String query);
		void onSettingsReload(Fragment options);
	}

	@Override
	public void onAttach(Activity activity) {
		super.onAttach(activity);
	}

	@Override
	public void onAttach(Context context) {
		super.onAttach(context);
	}

	@Override
	public View onCreateView(LayoutInflater inflater, ViewGroup container,
							 Bundle savedInstanceState) {
		// Inflate the layout for this fragment
		return inflater.inflate(R.layout.game_config_fragment, container, false);
	}

	@Override
	public void onViewCreated(View view, Bundle savedInstanceState) {

		mPrefs = PreferenceManager.getDefaultSharedPreferences(getActivity());

		Emulator app = (Emulator) getActivity().getApplicationContext();
		app.getConfigurationPrefs(mPrefs);

		// Generate the menu options and fill in existing settings

		OnCheckedChangeListener dynarec_options = new OnCheckedChangeListener() {

			public void onCheckedChanged(CompoundButton buttonView,
										 boolean isChecked) {
				mPrefs.edit().putBoolean(Emulator.pref_dynarecopt, isChecked).apply();
				Emulator.dynarecopt = isChecked;
			}
		};
		CompoundButton dynarec_opt = (CompoundButton) getView().findViewById(
				R.id.dynarec_option);
		dynarec_opt.setChecked(Emulator.dynarecopt);
		dynarec_opt.setOnCheckedChangeListener(dynarec_options);

		OnCheckedChangeListener unstable_option = new OnCheckedChangeListener() {

			public void onCheckedChanged(CompoundButton buttonView,
										 boolean isChecked) {
				mPrefs.edit().putBoolean(Emulator.pref_unstable, isChecked).apply();
				Emulator.unstableopt = isChecked;
			}
		};
		CompoundButton unstable_opt = (CompoundButton) getView().findViewById(
				R.id.unstable_option);
		if (Emulator.unstableopt) {
			unstable_opt.setChecked(true);
		} else {
			unstable_opt.setChecked(false);
		}
		unstable_opt.setOnCheckedChangeListener(unstable_option);

		OnCheckedChangeListener safemode_option = new OnCheckedChangeListener() {

			public void onCheckedChanged(CompoundButton buttonView,
										 boolean isChecked) {
				mPrefs.edit().putBoolean(Emulator.pref_dynsafemode, isChecked).apply();
				Emulator.dynsafemode = isChecked;
			}
		};
		CompoundButton safemode_opt = (CompoundButton) getView().findViewById(
				R.id.dynarec_safemode);
		if (Emulator.dynsafemode) {
			safemode_opt.setChecked(true);
		} else {
			safemode_opt.setChecked(false);
		}
		safemode_opt.setOnCheckedChangeListener(safemode_option);

		final EditText mainFrames = (EditText) getView().findViewById(R.id.current_frames);
		mainFrames.setText(String.valueOf(Emulator.frameskip));

		final SeekBar frameSeek = (SeekBar) getView().findViewById(R.id.frame_seekbar);
		frameSeek.setProgress(Emulator.frameskip);
		frameSeek.setIndeterminate(false);
		frameSeek.setOnSeekBarChangeListener(new OnSeekBarChangeListener() {
			public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
				mainFrames.setText(String.valueOf(progress));
			}

			public void onStartTrackingTouch(SeekBar seekBar) {
				// TODO Auto-generated method stub
			}

			public void onStopTrackingTouch(SeekBar seekBar) {
				int progress = seekBar.getProgress();
				mPrefs.edit().putInt(Emulator.pref_frameskip, progress).apply();
				Emulator.frameskip = progress;
			}
		});
		mainFrames.addTextChangedListener(new TextWatcher() {
			public void afterTextChanged(Editable s) {
				Editable frameText = mainFrames.getText();
				if (frameText != null) {
					int frames = Integer.parseInt(frameText.toString());
					frameSeek.setProgress(frames);
					mPrefs.edit().putInt(Emulator.pref_frameskip, frames).apply();
					Emulator.frameskip = frames;
				}
			}

			public void beforeTextChanged(CharSequence s, int start, int count, int after) {
			}

			public void onTextChanged(CharSequence s, int start, int before, int count) {
			}
		});

		OnCheckedChangeListener pvr_rendering = new OnCheckedChangeListener() {

			public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
				mPrefs.edit().putBoolean(Emulator.pref_pvrrender, isChecked).apply();
				Emulator.pvrrender = isChecked;
			}
		};
		CompoundButton pvr_render = (CompoundButton) getView().findViewById(R.id.render_option);
		pvr_render.setChecked(Emulator.pvrrender);
		pvr_render.setOnCheckedChangeListener(pvr_rendering);

		OnCheckedChangeListener synchronous = new OnCheckedChangeListener() {
			public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
				mPrefs.edit().putBoolean(Emulator.pref_syncedrender, isChecked).apply();
				Emulator.syncedrender = isChecked;
			}
		};
		CompoundButton synced_render = (CompoundButton) getView().findViewById(R.id.syncrender_option);
		synced_render.setChecked(Emulator.syncedrender);
		synced_render.setOnCheckedChangeListener(synchronous);

		OnCheckedChangeListener mod_volumes = new OnCheckedChangeListener() {

			public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
				mPrefs.edit().putBoolean(Emulator.pref_modvols, isChecked).apply();
				Emulator.modvols = isChecked;
			}
		};
		CompoundButton modifier_volumes = (CompoundButton) getView().findViewById(R.id.modvols_option);
		modifier_volumes.setChecked(Emulator.modvols);
		modifier_volumes.setOnCheckedChangeListener(mod_volumes);
	}
}
