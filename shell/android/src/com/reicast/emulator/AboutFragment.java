package com.reicast.emulator;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;

import android.content.pm.PackageManager.NameNotFoundException;
import android.os.Bundle;
import android.support.v4.app.Fragment;
import android.text.util.Linkify;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

public class AboutFragment extends Fragment {

	String buildId = "";

	@Override
	public View onCreateView(LayoutInflater inflater, ViewGroup container,
			Bundle savedInstanceState) {
		// Inflate the layout for this fragment
		return inflater.inflate(R.layout.about_fragment, container, false);
	}

	@Override
	public void onViewCreated(View view, Bundle savedInstanceState) {

		try {
			InputStream file = getResources().getAssets().open("build");
			if (file != null) {
				BufferedReader reader = new BufferedReader(
						new InputStreamReader(file));
				buildId = reader.readLine();
				file.close();
			}
		} catch (IOException ioe) {
			ioe.printStackTrace();
		}

		try {
			String versionName = getActivity().getPackageManager()
					.getPackageInfo(getActivity().getPackageName(), 0).versionName;
			int versionCode = getActivity().getPackageManager()
					.getPackageInfo(getActivity().getPackageName(), 0).versionCode;
			TextView version = (TextView) getView().findViewById(
					R.id.revision_text);
			String revision = getString(R.string.revision_text,
					versionName, String.valueOf(versionCode));
			if (!buildId.equals("")) {
				revision = getString(R.string.revision_text,
						versionName, buildId.substring(0,7));
			}
			version.setText(revision);
		} catch (NameNotFoundException e) {
			e.printStackTrace();
		}
		
		TextView website = (TextView) getView().findViewById(R.id.site_text);
		Linkify.addLinks(website, Linkify.ALL);
	}
}
