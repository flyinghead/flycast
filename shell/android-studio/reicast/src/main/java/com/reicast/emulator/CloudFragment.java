package com.reicast.emulator;

/*
 *  File: CloudFragment.java
 *  Author: Luca D'Amico (Luca91)
 *  Last Edit: 11 May 2014
 *  
 *  Reference: http://forums.reicast.com/index.php?topic=160.msg422
 */

import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.SharedPreferences;
import android.content.SharedPreferences.Editor;
import android.os.AsyncTask;
import android.os.Bundle;
import android.os.Environment;
import android.preference.PreferenceManager;
import android.support.v4.app.Fragment;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.Toast;

import com.dropbox.client2.DropboxAPI;
import com.dropbox.client2.DropboxAPI.Entry;
import com.dropbox.client2.android.AndroidAuthSession;
import com.dropbox.client2.exception.DropboxException;
import com.dropbox.client2.session.AccessTokenPair;
import com.dropbox.client2.session.AppKeyPair;
import com.dropbox.client2.session.TokenPair;
import com.reicast.emulator.config.Config;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.concurrent.ExecutionException;


public class CloudFragment extends Fragment {
	
	Button uploadBtn;
	Button downloadBtn;
	AlertDialog.Builder confirmDialog = null; 
	boolean actionRequired=false;
	public String task = "";
	DropBoxClient client = null;
	private String home_directory;
	
	String[] vmus = {"vmu_save_A1.bin","vmu_save_A2.bin",
					 "vmu_save_B1.bin","vmu_save_B2.bin",
					 "vmu_save_C1.bin","vmu_save_C2.bin",
					 "vmu_save_D1.bin","vmu_save_D2.bin"};
    
	@Override
	public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
		return inflater.inflate(R.layout.cloud_fragment, container, false);
	}

	@Override
	public void onViewCreated(View view, Bundle savedInstanceState) {
		SharedPreferences mPrefs = PreferenceManager.getDefaultSharedPreferences(getActivity());
		home_directory = mPrefs.getString(Config.pref_home,
				Environment.getExternalStorageDirectory().getAbsolutePath());
        buttonListener();
        confirmDialog = new AlertDialog.Builder(getActivity());
        setClient();
	}
	
	public void setClient(){
		if(client==null)
        	client = new DropBoxClient(getActivity());
	}
	
	
	public void buttonListener() {
		uploadBtn = (Button) getView().findViewById(R.id.uploadBtn);
		uploadBtn.setOnClickListener(new View.OnClickListener() {
			@Override
			public void onClick(View arg0) {
				confirmDialog.setMessage(R.string.uploadWarning);
		        confirmDialog.setPositiveButton(android.R.string.ok, new DialogInterface.OnClickListener() {
		    	    public void onClick(DialogInterface dialog, int which) {					      	
		         	   	setClient();
		                task = "Upload";
		         		client.startLogin();
		         		actionRequired = true;
			    	    }
			    	});
			    confirmDialog.setNegativeButton(R.string.cancel, null);			
		        confirmDialog.show();

            }
          });
            	
		
		downloadBtn = (Button) getView().findViewById(R.id.downloadBtn);
		downloadBtn.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View arg0) {
            	confirmDialog.setMessage(R.string.downloadWarning);
		        confirmDialog.setPositiveButton(android.R.string.ok, new DialogInterface.OnClickListener() {
		    	    public void onClick(DialogInterface dialog, int which) {					      	
		         	   	setClient();
		                task = "Download";
		         		client.startLogin();
		         		actionRequired = true;
			    	    }
			    	});
			    confirmDialog.setNegativeButton(R.string.cancel, null);					
		        confirmDialog.show();
            }
        });
    }
	
	
	@Override
	public void onResume(){
		super.onResume();
		if (client.mDBApi != null) {
			if (client.mDBApi.getSession().authenticationSuccessful()) {
				try {
					client.mDBApi.getSession().finishAuthentication();
					TokenPair tokens = client.mDBApi.getSession(). getAccessTokenPair();
					if(tokens == null)
						Toast.makeText(getActivity(), "Failed to save session token!", Toast.LENGTH_SHORT).show();
					else
						client.storeKeys(tokens.key, tokens.secret);
				} catch (IllegalStateException e) {
					Log.i("Dropbox", "Error authenticating", e);
				}
			}
			if(actionRequired){
				for(int k=0;k<vmus.length;k++){
					String result = "";
					try {
						String vmuPath = home_directory+"/"+vmus[k];
						File vmu = new File(vmuPath);
						if(vmu.exists() || task.equals("Download") ){     
							result = new netOperation(client, home_directory).execute(
									task,vmuPath,vmus[k]).get();
						}
						else{
							result = "Ok"; // The result is still ok, because the vmu bin doesn't exist ;)
							Toast.makeText(getActivity(), vmus[k]+ " doesn't exist, skipping it!",
									Toast.LENGTH_SHORT).show();
						}
					} catch (InterruptedException e) {
						e.printStackTrace();
					} catch (ExecutionException e) {
						e.printStackTrace();
					}
					if(result.equals("Ok"))
						Toast.makeText(getActivity(), "Task Completed!", Toast.LENGTH_SHORT).show();
					else
						Toast.makeText(getActivity(), "Task Failed!", Toast.LENGTH_SHORT).show();
				}
			}
			actionRequired = false;
		}
	}
	
	
}


class DropBoxClient {
	
	Context context;
	
    final static private String APP_KEY = "7d7tw1t57sbzrj5";
    final static private String APP_SECRET = "5xxqa2uctousyi2";
    
    public DropboxAPI<AndroidAuthSession> mDBApi;
    AndroidAuthSession session;
    
    public DropBoxClient(Context context){
    	this.context = context;
        session = buildSession();
        mDBApi = new DropboxAPI<AndroidAuthSession>(session);
    }
    
    public void startLogin(){
    		mDBApi.getSession().startOAuth2Authentication(context);
    }
    
	public String[] getKeys() {
        SharedPreferences prefs = context.getSharedPreferences("ReicastVMUUploader", 0);
        String key = prefs.getString("DBoxKey", null);
        String secret = prefs.getString("DBoxSecret", null);
        if (key != null && secret != null) {
        	String[] ret = new String[2];
        	ret[0] = key;
        	ret[1] = secret;
        	return ret;
        } else {
        	return null;
        }
    }
	

    
    public void storeKeys(String key, String secret) {
        SharedPreferences prefs = context.getSharedPreferences("ReicastVMUUploader", 0);
        Editor edit = prefs.edit();
        edit.putString("DBoxKey", key);
        edit.putString("DBoxSecret", secret);
        edit.commit();
    }
    

    

	
	 private AndroidAuthSession buildSession() {
	        AppKeyPair appKeyPair = new AppKeyPair(APP_KEY, APP_SECRET);
	        AndroidAuthSession session;

	        String[] stored = getKeys();
	        if (stored != null) {
	            AccessTokenPair accessToken = new AccessTokenPair(stored[0], stored[1]);
	            session = new AndroidAuthSession(appKeyPair, accessToken);
	        } else {
	            session = new AndroidAuthSession(appKeyPair);
	        }

	        return session;
	    }

}

class netOperation extends AsyncTask<String, Void, String> {
	
	DropBoxClient client = null;
	private String home_directory;
	
	public netOperation(DropBoxClient client, String home_directory){
		this.client = client;
		this.home_directory = home_directory;
	}

	public boolean uploadFile(String filePath, String fileName) {
        File file = new File(filePath);
        FileInputStream inputStream = null;
        try {
            inputStream = new FileInputStream(file);
        } catch (FileNotFoundException e) {
            e.printStackTrace();
        }

        DropboxAPI.Entry response = null;
        try {
            response = client.mDBApi.putFileOverwrite("/"+fileName, inputStream, file.length(), null);
        } catch (DropboxException e) {
            e.printStackTrace();
        }
        Log.i("FileInfos", "The uploaded file's rev is: "+ response);
        return true;
    }

    public boolean downloadFile(String filePath, String fileName) {
        DropboxAPI.DropboxFileInfo info = null;
        try { 
        	Entry remoteFile = client.mDBApi.metadata("/"+fileName, 1, null, false, null);
        	if((remoteFile.rev != null) && (remoteFile.bytes > 0)){ // Avoid to download 0 bytes vmus!
        		 File file = new File(filePath);
        		 if(file.exists())
        			 createBackupOfVmu(fileName);
        	     FileOutputStream out = null;
        	     try {
        	    	 out = new FileOutputStream(file);
        	     } catch (FileNotFoundException e) {
        	    	 e.printStackTrace();
        	     }
        		info = client.mDBApi.getFile("/"+fileName,null,out,null);
        	}
        } catch (DropboxException e) {
            e.printStackTrace();
        }
        Log.i("FileInfos", "The downloaded file's rev is: "+ info);
        return true;
    }


    @Override
    protected void onPostExecute(String result) {

    }

     @Override
    protected String doInBackground(String... strings) {
        if(strings[0].equals("Upload")){
            if(uploadFile(strings[1],strings[2]))
                return "Ok";
            else
                return "No";
        }
        else if(strings[0].equals("Download")){
            if(downloadFile(strings[1],strings[2]))
                return "Ok";
            else
                return "No";
        }
        else
            return "Unknown";
    }

    @Override
    protected void onPreExecute() {}

    @Override
    protected void onProgressUpdate(Void... values) {}
    
    
    void  createBackupOfVmu(String vmuName){
    	File backupDir = new File(home_directory+"/VmuBackups/");
    	 if(!backupDir.exists()) {
         		backupDir.mkdirs();
         } 
    	
        File source = new File(home_directory+"/"+vmuName);
        File destination = new File(home_directory+"/VmuBackups/"+vmuName);
        if(!destination.exists()) {
        	try {
				destination.createNewFile();
			} catch (IOException e) {
				e.printStackTrace();
			}
        } 
        try {
			InputStream in = new FileInputStream(source);
			OutputStream out = new FileOutputStream(destination);
			byte[] buffer = new byte[1024];
			int length;
			while ((length = in.read(buffer)) > 0){
				out.write(buffer, 0, length);
			}
			in.close();
			out.close();
		} catch (FileNotFoundException e) {
			e.printStackTrace();
		} catch (IOException e) {
			e.printStackTrace();
		}
    }
    
}
