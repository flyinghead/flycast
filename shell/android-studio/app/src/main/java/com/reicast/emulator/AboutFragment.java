package com.reicast.emulator;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.util.ArrayList;
import java.util.HashMap;

import org.apache.http.HttpEntity;
import org.apache.http.HttpResponse;
import org.apache.http.StatusLine;
import org.apache.http.client.ClientProtocolException;
import org.apache.http.client.HttpClient;
import org.apache.http.client.methods.HttpGet;
import org.apache.http.impl.client.DefaultHttpClient;
import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import android.annotation.TargetApi;
import android.content.pm.PackageManager.NameNotFoundException;
import android.os.AsyncTask;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.support.v4.app.Fragment;
import android.text.util.Linkify;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemClickListener;
import android.widget.ListView;
import android.widget.SlidingDrawer;
import android.widget.SlidingDrawer.OnDrawerOpenListener;
import android.widget.TextView;
import android.widget.Toast;

import com.reicast.emulator.config.Config;
import com.reicast.emulator.debug.GitAdapter;

public class AboutFragment extends Fragment {

	String buildId = "";
	SlidingDrawer slidingGithub;
	private ListView list;
	private GitAdapter adapter;
	private Handler handler;

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
				revision = getActivity().getString(R.string.revision_text,
						versionName, buildId.substring(0,7));
			}
			version.setText(revision);
		} catch (NameNotFoundException e) {
			e.printStackTrace();
		}

		TextView website = (TextView) getView().findViewById(
				R.id.site_text);
		Linkify.addLinks(website, Linkify.ALL);

		handler = new Handler();

		slidingGithub = (SlidingDrawer) getView().findViewById(
				R.id.slidingGithub);
		slidingGithub.setOnDrawerOpenListener(new OnDrawerOpenListener() {
			@TargetApi(Build.VERSION_CODES.HONEYCOMB)
			public void onDrawerOpened() {
				new retrieveGitTask().execute(Config.git_api);
			}
		});

	}

	public class retrieveGitTask extends
			AsyncTask<String, Integer, ArrayList<HashMap<String, String>>> {

		@Override
		protected void onPreExecute() {

		}

		@Override
		protected ArrayList<HashMap<String, String>> doInBackground(
				String... paths) {
			ArrayList<HashMap<String, String>> commitList = new ArrayList<HashMap<String, String>>();
			try {
				JSONArray gitObject = getContent(paths[0]);
				for (int i = 0; i < gitObject.length(); i++) {
					JSONObject jsonObject = gitObject.getJSONObject(i);

					JSONObject commitArray = jsonObject.getJSONObject("commit");

					String date = commitArray.getJSONObject("committer")
							.getString("date").replace("T", " ")
							.replace("Z", "");
					String author = commitArray.getJSONObject("author")
							.getString("name");
					String committer = commitArray.getJSONObject("committer")
							.getString("name");

					String avatar = null;
					if (!jsonObject.getString("committer").equals("null")) {
						avatar = jsonObject.getJSONObject("committer")
								.getString("avatar_url");
						committer = committer
								+ " ("
								+ jsonObject.getJSONObject("committer")
								.getString("login") + ")";
						if (avatar.equals("null")) {
							avatar = "https://github.com/apple-touch-icon-144.png";
						}
					} else {
						avatar = "https://github.com/apple-touch-icon-144.png";
					}
					if (!jsonObject.getString("author").equals("null")) {
						author = author
								+ " ("
								+ jsonObject.getJSONObject("author").getString(
										"login") + ")";
					}
					String sha = jsonObject.getString("sha");
					String curl = jsonObject
							.getString("url")
							.replace("https://api.github.com/repos",
									"https://github.com")
									.replace("commits", "commit");

					String title = "No commit heading attached";
					String message = "No commit message attached";

					if (commitArray.getString("message").contains("\n\n")) {
						String fullOutput = commitArray.getString("message");
						title = fullOutput.substring(0,
								fullOutput.indexOf("\n\n"));
						message = fullOutput.substring(
								fullOutput.indexOf("\n\n") + 1,
								fullOutput.length());
					} else {
						title = commitArray.getString("message");
					}

					HashMap<String, String> map = new HashMap<String, String>();
					map.put("Date", date);
					map.put("Committer", committer);
					map.put("Title", title);
					map.put("Message", message);
					map.put("Sha", sha);
					map.put("Url", curl);
					map.put("Author", author);
					map.put("Avatar", avatar);
					map.put("Build", buildId);
					commitList.add(map);
				}

			} catch (JSONException e) {
				handler.post(new Runnable() {
					public void run() {
						MainActivity.showToastMessage(getActivity(),
								getActivity().getString(R.string.git_broken),
								R.drawable.ic_github, Toast.LENGTH_SHORT);
						slidingGithub.close();
					}
				});
				e.printStackTrace();
			} catch (Exception e) {
				handler.post(new Runnable() {
					public void run() {
						MainActivity.showToastMessage(getActivity(),
								getActivity().getString(R.string.git_broken),
								R.drawable.ic_github, Toast.LENGTH_SHORT);
						slidingGithub.close();
					}
				});
				e.printStackTrace();
			}

			return commitList;
		}

		@Override
		protected void onPostExecute(
				ArrayList<HashMap<String, String>> commitList) {
			if (commitList != null && commitList.size() > 0) {
				list = (ListView) getView().findViewById(R.id.list);
				list.setSelector(R.drawable.list_selector);
				list.setChoiceMode(ListView.CHOICE_MODE_SINGLE);
				adapter = new GitAdapter(getActivity(), commitList);
				// Set adapter as specified collection
				list.setAdapter(adapter);

				list.setOnItemClickListener(new OnItemClickListener() {
					public void onItemClick(AdapterView<?> parent, View view,
							int position, long id) {
						slidingGithub.open();
					}
				});
			}

		}
		
		private JSONArray getContent(String urlString) 
				throws IOException, JSONException {
			StringBuilder builder = new StringBuilder();
			HttpClient client = new DefaultHttpClient();
			HttpGet httpGet = new HttpGet(urlString);
			try {
				HttpResponse response = client.execute(httpGet);
				StatusLine statusLine = response.getStatusLine();
				int statusCode = statusLine.getStatusCode();
				if (statusCode == 200) {
					HttpEntity entity = response.getEntity();
					InputStream content = entity.getContent();
					BufferedReader reader = new BufferedReader(
							new InputStreamReader(content));
					String line;
					while ((line = reader.readLine()) != null) {
						builder.append(line);
					}
				} else {
				}
			} catch (ClientProtocolException e) {
				e.printStackTrace();
			} catch (IOException e) {
				e.printStackTrace();
			}
			return new JSONArray(builder.toString());
		}
	}
}
