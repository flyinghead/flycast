package com.reicast.emulator.debug;

import android.app.Activity;
import android.app.Notification;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Build;
import android.os.Bundle;
import android.preference.PreferenceManager;
import android.support.v4.app.NotificationCompat;
import android.util.Log;

import com.google.android.gms.gcm.GoogleCloudMessaging;

public class GcmBroadcastReceiver extends BroadcastReceiver {
	static final String TAG = "GCM::Service";
	Context mContext;

	@Override
	public void onReceive(Context context, Intent intent) {
		GoogleCloudMessaging gcm = GoogleCloudMessaging.getInstance(context);
		mContext = context.getApplicationContext();
		String messageType = gcm.getMessageType(intent);
		if (GoogleCloudMessaging.MESSAGE_TYPE_SEND_ERROR.equals(messageType)) {
			// sendNotification(intent.getExtras().toString());
		} else if (GoogleCloudMessaging.MESSAGE_TYPE_DELETED
				.equals(messageType)) {
			// sendNotification(intent.getExtras().toString());
		} else {
			Bundle extras = intent.getExtras();
			if (extras.getString("message") != null) {
				String sender = extras.getString("sender");
				Log.d(TAG, "Sender: " + sender);
				String message = extras.getString("message");
				if (!message.equals("")) {
					if (sender.contains("reicast tester #")) {
						sendNotification(sender, message);
					}
				}
			}
		}
		setResultCode(Activity.RESULT_OK);
	}

	// Put the GCM message into a notification and post it.
	private void sendNotification(String sender, String message) {
		SharedPreferences prefs = PreferenceManager
				.getDefaultSharedPreferences(mContext);
		final String number = sender.replace("reicast tester #", "");
		long timestamp = System.currentTimeMillis();
		int reference = (int) timestamp;
		if (prefs.getBoolean("enable_messaging", true)) {
			NotificationManager notificationManager = (NotificationManager) mContext
					.getSystemService(Context.NOTIFICATION_SERVICE);
			String title = "New reicast tester message!";
			Notification notification;
			Intent notificationIntent = new Intent(mContext, Debug.class);
			// notificationIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
			notificationIntent.setAction("reicast.emulator.NOTIFY");
			notificationIntent.putExtra("sender", sender);
			notificationIntent.putExtra("message", message);
			notificationIntent.putExtra("context", "GCM");
			PendingIntent intent = PendingIntent.getActivity(mContext, 0,
					notificationIntent, PendingIntent.FLAG_ONE_SHOT);
			NotificationCompat.Builder builder = new NotificationCompat.Builder(
					mContext);
			builder.setTicker(sender).setWhen(timestamp).setContentTitle(title)
					.setContentText(message).setAutoCancel(true)
					.setContentIntent(intent)
					.setSmallIcon(R.drawable.ic_launcher)
					.setLights(0xff00ff00, 200, 800)
					.setPriority(Notification.PRIORITY_HIGH);

			if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR1) {
				notification = new NotificationCompat.BigTextStyle(builder)
						.bigText(message).build();
			} else {
				notification = builder.build();
			}
			notification.flags |= Notification.FLAG_AUTO_CANCEL;

			notificationManager.notify(reference, notification);
		}
	}
}
