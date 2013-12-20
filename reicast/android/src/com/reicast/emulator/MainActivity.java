package com.reicast.emulator;

import java.io.File;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Comparator;
import java.util.List;

import android.net.Uri;
import android.os.Bundle;
import android.os.Environment;
import android.os.Vibrator;
import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.graphics.drawable.Drawable;
import android.view.Menu;
import android.view.MenuItem;
import android.view.MotionEvent;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.View.OnTouchListener;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;
import android.widget.Toast;
import android.support.v4.app.NavUtils;

import tv.ouya.console.api.OuyaController;
import java.io.*;

public class MainActivity extends Activity {

	Vibrator vib;
	Drawable orig_bg;
	
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        
        try 
        { 
            File file = new File("/sdcard/dc/data/buttons.png");
            if (!file.exists())
            {
                file.createNewFile();
                OutputStream fo = new FileOutputStream(file);
                InputStream png=getBaseContext().getAssets().open("buttons.png");
                
                byte[] buffer = new byte[4096];
                int len = 0;
                while ((len = png.read(buffer)) != -1) {
                    fo.write(buffer, 0, len);
                }
                fo.close();
                png.close();
            }
        }
        catch (IOException ioe) 
        {
            ioe.printStackTrace();
        }
        
        vib=(Vibrator) getSystemService(Context.VIBRATOR_SERVICE);
        
        
        findViewById(R.id.config).setOnClickListener(
            	new OnClickListener() {
            		public void onClick(View view) {
            			AlertDialog.Builder alertDialogBuilder = new AlertDialog.Builder(
            				MainActivity.this);
             
            			// set title
            			alertDialogBuilder.setTitle("Configure");
             
            			// set dialog message
            			alertDialogBuilder
            				.setMessage("No configuration for now :D")
            				.setCancelable(false)
            				.setPositiveButton("Oh well",new DialogInterface.OnClickListener() {
            					public void onClick(DialogInterface dialog,int id) {
            						//MainActivity.this.finish();
            					}
            				  });
             
            				// create alert dialog
            				AlertDialog alertDialog = alertDialogBuilder.create();
             
            				// show it
            				alertDialog.show();
            		}

            	});
        
        findViewById(R.id.about).setOnTouchListener(
            	new OnTouchListener() 
            	{
            		public boolean onTouch(View v, MotionEvent event)
            		{
            			if (event.getActionMasked()==MotionEvent.ACTION_DOWN)
            			{
	            			vib.vibrate(50);
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
	            						//MainActivity.this.finish();
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
        
       /*
        OnTouchListener viblist=new OnTouchListener() {
			
			public boolean onTouch(View v, MotionEvent event) {
				if (event.getActionMasked()==MotionEvent.ACTION_DOWN)
					vib.vibrate(50);
				return false;
			}
		};

        findViewById(R.id.config).setOnTouchListener(viblist);
        findViewById(R.id.about).setOnTouchListener(viblist);
        */
        
        
       
        navigate(Environment.getExternalStorageDirectory());        

        File bios = new File("/sdcard/dc/data/dc_boot.bin");
        File flash = new File("/sdcard/dc/data/dc_flash.bin");

        String msg = null;
        if(!bios.exists()) 
            msg = "Bios Missing. Put bios in /sdcard/dc/data/dc_boot.bin";
        else if (!flash.exists())
            msg = "Flash Missing. Put bios in /sdcard/dc/data/dc_flash.bin";

        if (msg != null ) {
            vib.vibrate(50);
            AlertDialog.Builder alertDialogBuilder = new AlertDialog.Builder(
                    MainActivity.this);

            // set title
            alertDialogBuilder.setTitle("Missing files");

            // set dialog message
            alertDialogBuilder
                .setMessage(msg)
                .setCancelable(false)
                .setPositiveButton("Dismiss",new DialogInterface.OnClickListener() {
                    public void onClick(DialogInterface dialog,int id) {
                        // if this button is clicked, close
                        // current activity
                        MainActivity.this.finish();
                    }
                  });

                // create alert dialog
                AlertDialog alertDialog = alertDialogBuilder.create();

                // show it
                alertDialog.show();
            }
    }
    
    class DirSort implements Comparator<File> {

        // Comparator interface requires defining compare method.
        public int compare(File filea, File fileb) {
        	
        	return ((filea.isFile() ? "a" : "b") + filea.getName().toLowerCase()).compareTo((fileb.isFile() ? "a" : "b")+fileb.getName().toLowerCase());
        }
    }
    
    void navigate(File root_sd)
    {
        LinearLayout v = (LinearLayout)findViewById(R.id.game_list);
        v.removeAllViews();
        
        ArrayList<File> list = new ArrayList<File>();  
        
        ((TextView)findViewById(R.id.text_cwd)).setText(root_sd.getAbsolutePath());
        
        File flist[] = root_sd.listFiles();
        
        
        File parent=root_sd.getParentFile();
        
        list.add(null);
        
        if (parent!=null)
        	list.add(parent);
        
        Arrays.sort(flist, new DirSort());
        
        for (int i=0;i<flist.length;i++)
        	list.add(flist[i]);
        
        
        for (int i=0;i<list.size();i++)
        {
        	if (list.get(i)!=null && list.get(i).isFile())
        		if (!list.get(i).getName().toLowerCase().endsWith(".gdi") && !list.get(i).getName().toLowerCase().endsWith(".cdi") && !list.get(i).getName().toLowerCase().endsWith(".chd"))
        			continue;
        	
        	View childview=getLayoutInflater().inflate(R.layout.app_list_item, null, false);
        	
        	if (list.get(i)==null)
        		((TextView)childview.findViewById(R.id.item_name)).setText("BOOT BIOS");
        	else if (list.get(i)==parent)
        		((TextView)childview.findViewById(R.id.item_name)).setText("..");
        	else
        		((TextView)childview.findViewById(R.id.item_name)).setText(list.get(i).getName());
        	
         	((ImageView)childview.findViewById(R.id.item_icon)).setImageResource(
         			list.get(i)==null ? R.drawable.config :
         			list.get(i).isDirectory() ? R.drawable.open_folder : 
         			list.get(i).getName().toLowerCase().endsWith(".gdi") ? R.drawable.gdi : 
         			list.get(i).getName().toLowerCase().endsWith(".cdi") ? R.drawable.cdi :
         			list.get(i).getName().toLowerCase().endsWith(".chd") ? R.drawable.chd : 
         			R.drawable.disk_unknown);
        	
         	childview.setTag(list.get(i));
        	
         	orig_bg=childview.getBackground();
         	
        	//vw.findViewById(R.id.childview).setBackgroundColor(0xFFFFFFFF);
        	

        	childview.findViewById(R.id.childview).setOnClickListener(
        	new OnClickListener() {
        		public void onClick(View view) {
        			File f = (File)view.getTag();
        			
        			if (f != null && f.isDirectory())
        			{
        				navigate(f);
        				vib.vibrate(50);
        			}
        			else
        			{
        				vib.vibrate(50);
        				Intent inte = new Intent(Intent.ACTION_VIEW,f!=null? Uri.fromFile(f):Uri.EMPTY,MainActivity.this.getBaseContext(),GL2JNIActivity.class);
        				MainActivity.this.startActivity(inte);
        				vib.vibrate(250);
        			}
        		}
        	});
        	

        	childview.findViewById(R.id.childview).setOnTouchListener(
        			new OnTouchListener() {
						public boolean onTouch(View view, MotionEvent arg1) {
							if (arg1.getActionMasked()== MotionEvent.ACTION_DOWN)
							{
								view.setBackgroundColor(0xFF4F3FFF);
							}
							else if (arg1.getActionMasked()== MotionEvent.ACTION_CANCEL || 
									arg1.getActionMasked()== MotionEvent.ACTION_UP)
							{
								view.setBackgroundDrawable(orig_bg);
							}
							
							return false;
							
						}
                	});
        	
        	if (i==0)
        	{
        		FrameLayout sepa= new FrameLayout(this);
        		sepa.setBackgroundColor(0xFFA0A0A0);
        		sepa.setPadding(0, 0, 0, 1);
        		v.addView(sepa);
        	}
        	v.addView(childview);
        	
        	FrameLayout sep= new FrameLayout(this);
    		sep.setBackgroundColor(0xFFA0A0A0);
    		sep.setPadding(0, 0, 0, 1);
    		v.addView(sep);
        }
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        getMenuInflater().inflate(R.menu.activity_main, menu);
        return true;
    }

    
}
