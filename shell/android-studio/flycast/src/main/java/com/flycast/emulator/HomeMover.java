/*
	Copyright 2024 flyinghead

	This file is part of Flycast.

    Flycast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    Flycast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Flycast.  If not, see <https://www.gnu.org/licenses/>.
*/
package com.flycast.emulator;

import android.app.Activity;
import android.app.ProgressDialog;
import android.content.DialogInterface;
import android.net.Uri;
import android.util.Log;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.List;

public class HomeMover {
    private Activity activity;
    private AndroidStorage storage;
    private StorageWrapper wrapper;
    private boolean migrationThreadCancelled = false;

    private boolean reloadConfigOnCompletion = false;

    private class StorageWrapper
    {
        private File getFile(String path) {
            Uri uri = Uri.parse(path);
            if (uri.getScheme().equals("file"))
                return new File(uri.getPath());
            else
                return null;
        }
        public String getSubPath(String parent, String kid)
        {
            File f = getFile(parent);
            if (f != null)
                return new File(f, kid).toURI().toString();
            else {
                try {
                    return storage.getSubPath(parent, kid);
                } catch (RuntimeException e) {
                    return null;
                }
            }
        }

        public FileInfo[] listContent(String folder)
        {
            File dir = getFile(folder);
            if (dir != null)
            {
                File[] files = dir.listFiles();
                List<FileInfo> ret = new ArrayList<>(files.length);
                for (File f : files) {
                    FileInfo info = new FileInfo();
                    info.setName(f.getName());
                    info.setDirectory(f.isDirectory());
                    info.setPath(f.toURI().toString());
                    ret.add(info);
                }
                return ret.toArray(new FileInfo[ret.size()]);
            }
            else {
                return storage.listContent(folder);
            }
        }

        public InputStream openInputStream(String path) throws FileNotFoundException {
            File file = getFile(path);
            if (file != null)
                return new FileInputStream(file);
            else
                return storage.openInputStream(path);
        }

        public OutputStream openOutputStream(String parent, String name) throws FileNotFoundException {
            File file = getFile(parent);
            if (file != null)
                return new FileOutputStream(new File(file, name));
            else
                return storage.openOutputStream(parent, name);
        }

        public boolean exists(String path) {
            if (path == null)
                return false;
            File file = getFile(path);
            if (file != null)
                return file.exists();
            else
                return storage.exists(path);
        }

        public String mkdir(String parent, String name) throws FileNotFoundException
        {
            File dir = getFile(parent);
            if (dir != null)
            {
                File subfolder = new File(dir, name);
                subfolder.mkdir();
                return subfolder.toURI().toString();
            }
            else {
                return storage.mkdir(parent, name);
            }
        }
    }

    public HomeMover(Activity activity, AndroidStorage storage) {
        this.activity = activity;
        this.storage = storage;
        this.wrapper = new StorageWrapper();
    }

    public void copyHome(String source, String dest, String message)
    {
        migrationThreadCancelled = false;
        ProgressDialog progress = new ProgressDialog(activity);
        progress.setTitle("Copying");
        progress.setMessage(message);
        progress.setProgressStyle(ProgressDialog.STYLE_HORIZONTAL);
        progress.setMax(1);
        progress.setOnCancelListener(dialogInterface -> migrationThreadCancelled = true);
        progress.show();

        Thread thread = new Thread(new Runnable() {
            private void copyFile(String path, String name, String toDir)
            {
                if (path == null)
                    return;
                //Log.d("flycast", "Copying " + path + " to " + toDir);
                try {
                    InputStream in = wrapper.openInputStream(path);
                    OutputStream out = wrapper.openOutputStream(toDir, name);
                    byte[] buf = new byte[8192];
                    while (true) {
                        int len = in.read(buf);
                        if (len == -1)
                            break;
                        out.write(buf, 0, len);
                    }
                    out.close();
                    in.close();
                } catch (Exception e) {
                    Log.e("flycast", "Error copying " + path, e);
                }
            }

            private void copyDir(String from, String toParent, String name)
            {
                //Log.d("flycast", "Copying folder " + from + " to " + toParent + " / " + name);
                if (!wrapper.exists(from))
                    return;
                try {
                    String to = wrapper.getSubPath(toParent, name);
                    if (!wrapper.exists(to))
                        to = wrapper.mkdir(toParent, name);

                    FileInfo[] files = wrapper.listContent(from);
                    incrementMaxProgress(files.length);
                    for (FileInfo file : files)
                    {
                        if (migrationThreadCancelled)
                            break;
                        if (!file.isDirectory())
                            copyFile(file.path, file.name, to);
                        else
                            copyDir(file.path, to, file.getName());
                        incrementProgress(1);
                    }
                } catch (Exception e) {
                    Log.e("flycast", "Error copying folder " + from, e);
                }
            }

            private void migrate()
            {
                incrementMaxProgress(3);
                String path = wrapper.getSubPath(source, "emu.cfg");
                copyFile(path, "emu.cfg", dest);
                if (migrationThreadCancelled)
                    return;
                incrementProgress(1);

                String srcMappings = wrapper.getSubPath(source, "mappings");
                copyDir(srcMappings, dest, "mappings");
                if (migrationThreadCancelled)
                    return;
                incrementProgress(1);

                String srcData = wrapper.getSubPath(source, "data");
                copyDir(srcData, dest, "data");
                incrementProgress(1);
            }

            private void incrementMaxProgress(int max) {
                activity.runOnUiThread(() -> {
                    progress.setMax(progress.getMax() + max);
                });
            }
            private void incrementProgress(int i) {
                activity.runOnUiThread(() -> {
                    progress.incrementProgressBy(i);
                });
            }

            @Override
            public void run()
            {
                migrate();
                activity.runOnUiThread(() -> {
                    progress.dismiss();
                    if (reloadConfigOnCompletion)
                        storage.reloadConfig();
                });
            }
        });
        thread.start();
    }

    public void setReloadConfigOnCompletion(boolean reloadConfigOnCompletion) {
        this.reloadConfigOnCompletion = reloadConfigOnCompletion;
    }
}
