/*
	Copyright 2026 flyinghead

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

import android.app.backup.BackupAgentHelper;
import android.app.backup.BackupDataInput;
import android.app.backup.BackupDataInputStream;
import android.app.backup.BackupDataOutput;
import android.app.backup.FileBackupHelper;
import android.content.Context;
import android.content.ContextWrapper;
import android.os.ParcelFileDescriptor;
import android.util.Log;

import com.flycast.emulator.emu.JNIdc;

import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

public class FlycastBackupAgent extends BackupAgentHelper {
    private static final String TAG = "FlycastBackupAgent";

    // Hacked FileBackupHelper that will restore all files from a backup, not just the files listed in the constructor.
    // It uses a dummy file list during restore containing only the file being currently restored and updated for each file.
    static class MyFileBackupHelper extends FileBackupHelper {
        private String[] files;

        public MyFileBackupHelper(Context context, String... files) {
            super(context, files);
            this.files = files;
        }

        public void restoreEntity(BackupDataInputStream data)
        {
            // We want to restore all files so make sure it appears in the list
            files[0] = data.getKey();
            super.restoreEntity(data);
        }
    }

    public void addFileHelper(String files[])
    {
        // Our files are located in the external files directory so hack the context to return it
        // instead of the regular internal files directory.
        Context mycontext = new ContextWrapper(this) {
            @Override
            public File getFilesDir() {
                return super.getExternalFilesDir(null);
            }
        };
        FileBackupHelper helper = new MyFileBackupHelper(mycontext, files);
        addHelper("files", helper);
    }

    @Override
    public void onBackup(ParcelFileDescriptor oldState, BackupDataOutput data, ParcelFileDescriptor newState) throws IOException {
        String[] files = listFiles((data.getTransportFlags() & FLAG_DEVICE_TO_DEVICE_TRANSFER) != 0);
        Log.i(TAG, "Starting backup of " + files.length + " files");
        addFileHelper(files);
        super.onBackup(oldState, data, newState);
        Log.i(TAG, "Data backed up successfully");
    }

    @Override
    public void onRestore(BackupDataInput data, int appVersionCode, ParcelFileDescriptor newState) throws IOException {
        Log.i(TAG, "Starting restore");
        addFileHelper(new String[] { "dummy" }); // Will be updated by the helper for each restored file
        super.onRestore(data, appVersionCode, newState);
        Log.i(TAG, "Data restored successfully");
    }

    @Override
    public void onRestoreFinished() {
        super.onRestoreFinished();
        // Reload and clean up the restored configuration.
        JNIdc.postRestore(getExternalFilesDir(null).getAbsolutePath());
        Log.i(TAG, "Restore finished");
    }

    @Override
    public void onQuotaExceeded(long backupDataBytes, long quotaBytes) {
        super.onQuotaExceeded(backupDataBytes, quotaBytes);
        Log.e(TAG, "Backup quota exceeded: backup is " + backupDataBytes + " bytes but quota is " + quotaBytes + " bytes");
    }

    // Return the list of files that need to be backed up as paths relative to the external files directory.
    // If d2dXfer is true, save states are included so they are copied over when copying from device to device.
    private String[] listFiles(boolean d2dXfer)
    {
        List<String> files = new ArrayList<>();
        File root = getApplicationContext().getExternalFilesDir(null);
        // emu.cfg
        File emucfg = new File(root, "emu.cfg");
        if (!emucfg.isFile())
            Log.i(TAG, "backup: can't access 'emu.cfg' file");
        else
            files.add(emucfg.getName());
        // Get files in 'data'
        File datadir = new File(root, "data");
        File[] kids = datadir.listFiles();
        if (kids == null) {
            Log.i(TAG, "backup: can't access 'data' directory");
        }
        else {
            for (File file : kids) {
                if (file.isDirectory())
                    continue;
                if (file.getName().equals("vulkan_pipeline.cache"))
                    continue;
                // Don't backup save states except when copying directly to a device
                if (!d2dXfer && file.getName().endsWith(".state"))
                    continue;
                files.add("data/" + file.getName());
            }
        }
        // Get files in 'mappings'
        File mappings = new File(root, "mappings");
        kids = mappings.listFiles();
        if (kids == null) {
            Log.i(TAG, "backup: can't access 'mappings' directory");
        }
        else
        {
            for (File file : kids) {
                if (file.isDirectory())
                    continue;
                files.add("mappings/" + file.getName());
            }
        }
        return files.toArray(new String[files.size()]);
    }
}
