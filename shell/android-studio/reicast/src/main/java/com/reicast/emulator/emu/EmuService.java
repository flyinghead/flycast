package com.reicast.emulator.emu;

import android.app.PendingIntent;
import android.app.Service;
import android.content.Intent;
import android.os.IBinder;
import android.support.v4.app.NotificationCompat;
import com.reicast.emulator.R;

public class EmuService extends Service {

    private final static int FOREGROUND_ID = 999;

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        if (intent != null && intent.getAction() != null
                && intent.getAction().equals("com.reicast.emulator.KYS")) {
            stopSelf();
        } else {
            Intent intentService = new Intent(this, EmuService.class);
            intentService.setAction("com.reicast.emulator.KYS");
            PendingIntent pendingIntent = PendingIntent.getService(this,
                    (int) System.currentTimeMillis(), intentService,
                    PendingIntent.FLAG_UPDATE_CURRENT);

            NotificationCompat.Builder builder = new NotificationCompat.Builder(this)
                    .setSmallIcon(R.drawable.ic_launcher)
                    .setTicker("Emulator ticker message")
                    .setContentTitle("Emulator notification title")
                    .setContentText("Emulator notification content")
                    .setContentIntent(pendingIntent);
            startForeground(FOREGROUND_ID, builder.build());
            return START_STICKY_COMPATIBILITY;
        }
        return START_NOT_STICKY;
    }

    @Override
    public void onDestroy() {
        stopForeground(true);
    }
}