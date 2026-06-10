package com.flycast.emulator;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.preference.PreferenceManager;
import android.util.Log;

import com.flycast.emulator.config.Config;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

/**
 * Receives host override broadcasts from RAOfflineProxy (and similar tools) so they can
 * redirect RetroAchievements API traffic to a local proxy without modifying emu.cfg manually.
 *
 * SET action: com.flycast.emulator.action.SET_RETROACHIEVEMENTS_HOST_OVERRIDE
 *   Extra "host" (String): the proxy base URL, e.g. "http://127.0.0.1:8080"
 *
 * CLEAR action: com.flycast.emulator.action.CLEAR_RETROACHIEVEMENTS_HOST_OVERRIDE
 *   No extras required.
 */
public class RetroAchievementsHostOverrideReceiver extends BroadcastReceiver {
    private static final String TAG = "flycast/RAHostOverride";
    static final String ACTION_SET = "com.flycast.emulator.action.SET_RETROACHIEVEMENTS_HOST_OVERRIDE";
    static final String ACTION_CLEAR = "com.flycast.emulator.action.CLEAR_RETROACHIEVEMENTS_HOST_OVERRIDE";
    static final String EXTRA_HOST = "host";

    @Override
    public void onReceive(Context context, Intent intent) {
        if (intent == null || intent.getAction() == null) return;

        String newHost = null;
        if (ACTION_SET.equals(intent.getAction())) {
            newHost = intent.getStringExtra(EXTRA_HOST);
            if (newHost == null || newHost.isEmpty()) {
                Log.w(TAG, "SET broadcast received but 'host' extra is missing or empty");
                return;
            }
            Log.i(TAG, "SET host override: " + newHost);
        } else if (ACTION_CLEAR.equals(intent.getAction())) {
            Log.i(TAG, "CLEAR host override");
        } else {
            return;
        }

        File cfgFile = resolveConfigFile(context);
        if (cfgFile == null) {
            Log.w(TAG, "Could not resolve emu.cfg path");
            return;
        }

        try {
            writeHostUrl(cfgFile, newHost != null ? newHost : "");
        } catch (IOException e) {
            Log.e(TAG, "Failed to update emu.cfg: " + e.getMessage());
        }

        // Also update the in-memory config so the next game load picks it up immediately
        // without requiring a full restart of Flycast.
        try {
            nativeSetHostOverride(newHost);
            Log.i(TAG, "Updated in-memory host override via JNI");
        } catch (UnsatisfiedLinkError e) {
            Log.d(TAG, "Native library not yet loaded; emu.cfg change will take effect on next Flycast start");
        }
    }

    private static native void nativeSetHostOverride(String host);

    private File resolveConfigFile(Context context) {
        String homeDir = PreferenceManager.getDefaultSharedPreferences(context)
                .getString(Config.pref_home, "");
        if (homeDir.isEmpty()) {
            File dir = context.getExternalFilesDir(null);
            if (dir == null) dir = context.getFilesDir();
            homeDir = dir.getAbsolutePath();
        }
        return new File(homeDir, "emu.cfg");
    }

    private void writeHostUrl(File cfgFile, String hostUrl) throws IOException {
        List<String> lines = new ArrayList<>();
        boolean inAchievements = false;
        boolean hostUrlWritten = false;
        boolean achievementsSectionFound = false;

        if (cfgFile.exists()) {
            try (BufferedReader reader = new BufferedReader(new FileReader(cfgFile))) {
                String line;
                while ((line = reader.readLine()) != null) {
                    String trimmed = line.trim();
                    if (trimmed.startsWith("[") && trimmed.endsWith("]")) {
                        if (inAchievements && !hostUrlWritten) {
                            // End of [achievements] section — append before next section
                            if (!hostUrl.isEmpty()) {
                                lines.add("HostUrl = " + hostUrl);
                            }
                            hostUrlWritten = true;
                        }
                        inAchievements = trimmed.equalsIgnoreCase("[achievements]");
                        if (inAchievements) achievementsSectionFound = true;
                    } else if (inAchievements) {
                        int eq = trimmed.indexOf('=');
                        if (eq != -1 && trimmed.substring(0, eq).trim().equalsIgnoreCase("HostUrl")) {
                            if (!hostUrl.isEmpty()) {
                                lines.add(line.substring(0, line.indexOf('=') + 1) + " " + hostUrl);
                            }
                            hostUrlWritten = true;
                            continue;
                        }
                    }
                    lines.add(line);
                }
            }
        }

        if (!achievementsSectionFound) {
            if (!hostUrl.isEmpty()) {
                lines.add("[achievements]");
                lines.add("HostUrl = " + hostUrl);
            }
        } else if (inAchievements && !hostUrlWritten && !hostUrl.isEmpty()) {
            lines.add("HostUrl = " + hostUrl);
        }

        try (FileWriter writer = new FileWriter(cfgFile)) {
            for (String line : lines) {
                writer.write(line);
                writer.write('\n');
            }
        }

        Log.i(TAG, "Updated HostUrl in " + cfgFile.getAbsolutePath()
                + " -> " + (hostUrl.isEmpty() ? "(cleared)" : hostUrl));
    }
}
