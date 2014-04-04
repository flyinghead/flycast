package com.reicast.emulator.config;

import java.io.File;

import android.app.Activity;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.os.Environment;
import android.preference.PreferenceManager;
import android.support.v4.app.Fragment;
import android.text.Editable;
import android.text.TextWatcher;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.EditText;
import android.widget.Toast;

import com.reicast.emulator.R;

public class OptionsFragment extends Fragment {

	private Button mainBrowse;
	private Button gameBrowse;
	private OnClickListener mCallback;

	private SharedPreferences mPrefs;
	private File sdcard = Environment.getExternalStorageDirectory();
	private String home_directory = sdcard + "/dc";
	private String game_directory = sdcard + "/dc";

	// Container Activity must implement this interface
	public interface OnClickListener {
		public void onMainBrowseSelected(String path_entry, boolean games);
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
	}

	@Override
	public View onCreateView(LayoutInflater inflater, ViewGroup container,
			Bundle savedInstanceState) {
		// Inflate the layout for this fragment
		return inflater.inflate(R.layout.options_fragment, container, false);
	}

	@Override
	public void onViewCreated(View view, Bundle savedInstanceState) {
		// setContentView(R.layout.activity_main);
		
		mPrefs = PreferenceManager.getDefaultSharedPreferences(getActivity());
		home_directory = mPrefs.getString("home_directory", home_directory);

		mainBrowse = (Button) getView().findViewById(R.id.browse_main_path);

		final EditText editBrowse = (EditText) getView().findViewById(
				R.id.main_path);
		editBrowse.setText(home_directory);

		mainBrowse.setOnClickListener(new View.OnClickListener() {
			public void onClick(View view) {
				if (editBrowse.getText() != null) {
					home_directory = editBrowse.getText().toString();
					//mPrefs.edit().putString("home_directory", home_directory).commit();
				}
				mCallback.onMainBrowseSelected(home_directory, false);
			}
		});

		editBrowse.addTextChangedListener(new TextWatcher() {
			public void afterTextChanged(Editable s) {
				if (editBrowse.getText() != null) {
					home_directory = editBrowse.getText().toString();
					if (home_directory.endsWith("/data")) {
						home_directory.replace("/data", "");
						Toast.makeText(getActivity(), R.string.data_folder,
								Toast.LENGTH_SHORT).show();
					}
					mPrefs.edit().putString("home_directory", home_directory)
							.commit();
				}
			}

			public void beforeTextChanged(CharSequence s, int start, int count, int after) {
			}

			public void onTextChanged(CharSequence s, int start, int before, int count) {
			}
		});

		gameBrowse = (Button) getView().findViewById(R.id.browse_game_path);

		final EditText editGames = (EditText) getView().findViewById(
				R.id.game_path);
		game_directory = mPrefs.getString("game_directory", game_directory);
		editGames.setText(game_directory);

		gameBrowse.setOnClickListener(new View.OnClickListener() {
			public void onClick(View view) {
				if (editBrowse.getText() != null) {
					game_directory = editGames.getText().toString();
					//mPrefs.edit().putString("game_directory", game_directory).commit();
				}
				mCallback.onMainBrowseSelected(game_directory, true);
			}
		});

		editGames.addTextChangedListener(new TextWatcher() {
			public void afterTextChanged(Editable s) {
				if (editBrowse.getText() != null) {
					game_directory = editGames.getText().toString();
					mPrefs.edit().putString("game_directory", game_directory)
					.commit();
				}
			}

			public void beforeTextChanged(CharSequence s, int start, int count, int after) {
			}

			public void onTextChanged(CharSequence s, int start, int before, int count) {
			}
		});
	}
}
