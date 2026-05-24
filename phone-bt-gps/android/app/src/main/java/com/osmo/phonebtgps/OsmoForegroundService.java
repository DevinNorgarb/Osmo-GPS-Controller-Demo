package com.osmo.phonebtgps;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Intent;
import android.content.pm.ServiceInfo;
import android.os.Build;
import android.os.IBinder;
import androidx.core.app.NotificationCompat;

public class OsmoForegroundService extends Service {

    public static final String CHANNEL_ID = "osmo_gps_remote";
    public static final int NOTIFICATION_ID = 1001;

    public static final String ACTION_START = "com.osmo.phonebtgps.action.START_FOREGROUND";
    public static final String ACTION_UPDATE = "com.osmo.phonebtgps.action.UPDATE_FOREGROUND";
    public static final String ACTION_STOP = "com.osmo.phonebtgps.action.STOP_FOREGROUND";

    public static final String EXTRA_TITLE = "title";
    public static final String EXTRA_BODY = "body";

    private static volatile boolean running = false;

    public static boolean isRunning() {
        return running;
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        if (intent == null) {
            return START_NOT_STICKY;
        }

        String action = intent.getAction();
        if (ACTION_STOP.equals(action)) {
            running = false;
            stopForeground(STOP_FOREGROUND_REMOVE);
            stopSelf();
            return START_NOT_STICKY;
        }

        String title = intent.getStringExtra(EXTRA_TITLE);
        if (title == null || title.isEmpty()) {
            title = "Osmo GPS Remote";
        }
        String body = intent.getStringExtra(EXTRA_BODY);
        if (body == null || body.isEmpty()) {
            body = "Running in background";
        }

        Notification notification = buildNotification(title, body);
        createNotificationChannel();

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            int serviceType = ServiceInfo.FOREGROUND_SERVICE_TYPE_CONNECTED_DEVICE
                | ServiceInfo.FOREGROUND_SERVICE_TYPE_LOCATION;
            startForeground(NOTIFICATION_ID, notification, serviceType);
        } else {
            startForeground(NOTIFICATION_ID, notification);
        }

        running = true;
        return START_STICKY;
    }

    private Notification buildNotification(String title, String body) {
        Intent launchIntent = new Intent(this, MainActivity.class);
        launchIntent.setFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP | Intent.FLAG_ACTIVITY_CLEAR_TOP);
        PendingIntent contentIntent = PendingIntent.getActivity(
            this,
            0,
            launchIntent,
            PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE
        );

        return new NotificationCompat.Builder(this, CHANNEL_ID)
            .setContentTitle(title)
            .setContentText(body)
            .setSmallIcon(R.drawable.ic_stat_gps)
            .setOngoing(true)
            .setOnlyAlertOnce(true)
            .setContentIntent(contentIntent)
            .setCategory(NotificationCompat.CATEGORY_SERVICE)
            .setPriority(NotificationCompat.PRIORITY_LOW)
            .build();
    }

    private void createNotificationChannel() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) {
            return;
        }
        NotificationManager manager = getSystemService(NotificationManager.class);
        if (manager == null) {
            return;
        }
        NotificationChannel channel = new NotificationChannel(
            CHANNEL_ID,
            "Osmo GPS Remote",
            NotificationManager.IMPORTANCE_LOW
        );
        channel.setDescription("Keeps BLE camera connection and GPS push alive in the background");
        manager.createNotificationChannel(channel);
    }
}
