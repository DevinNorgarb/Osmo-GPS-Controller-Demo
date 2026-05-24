package com.osmo.phonebtgps;

import android.Manifest;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.provider.Settings;
import androidx.core.app.ActivityCompat;
import com.getcapacitor.JSObject;
import com.getcapacitor.Plugin;
import com.getcapacitor.PluginCall;
import com.getcapacitor.PluginMethod;
import com.getcapacitor.annotation.CapacitorPlugin;
import com.getcapacitor.annotation.Permission;
import com.getcapacitor.annotation.PermissionCallback;

@CapacitorPlugin(
    name = "OsmoBackground",
    permissions = {
        @Permission(strings = { Manifest.permission.POST_NOTIFICATIONS }, alias = OsmoBackgroundPlugin.PERM_NOTIFICATIONS)
    }
)
public class OsmoBackgroundPlugin extends Plugin {

    static final String PERM_NOTIFICATIONS = "notifications";

    private static final int REQUEST_BATTERY_OPTIMIZATION = 9001;

    @PluginMethod
    public void startService(PluginCall call) {
        String title = call.getString("title", "Osmo GPS Remote");
        String body = call.getString("body", "Running in background");
        startOrUpdateService(OsmoForegroundService.ACTION_START, title, body);
        call.resolve();
    }

    @PluginMethod
    public void updateService(PluginCall call) {
        String title = call.getString("title", "Osmo GPS Remote");
        String body = call.getString("body", "Running in background");
        if (OsmoForegroundService.isRunning()) {
            startOrUpdateService(OsmoForegroundService.ACTION_UPDATE, title, body);
        } else {
            startOrUpdateService(OsmoForegroundService.ACTION_START, title, body);
        }
        call.resolve();
    }

    @PluginMethod
    public void stopService(PluginCall call) {
        Intent intent = new Intent(getContext(), OsmoForegroundService.class);
        intent.setAction(OsmoForegroundService.ACTION_STOP);
        getContext().startService(intent);
        call.resolve();
    }

    @PluginMethod
    public void checkNotificationPermission(PluginCall call) {
        JSObject result = new JSObject();
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU) {
            result.put("granted", true);
        } else {
            result.put(
                "granted",
                ActivityCompat.checkSelfPermission(getContext(), Manifest.permission.POST_NOTIFICATIONS)
                    == android.content.pm.PackageManager.PERMISSION_GRANTED
            );
        }
        call.resolve(result);
    }

    @PluginMethod
    public void requestNotificationPermission(PluginCall call) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU) {
            JSObject result = new JSObject();
            result.put("granted", true);
            call.resolve(result);
            return;
        }
        if (getPermissionState(PERM_NOTIFICATIONS) == com.getcapacitor.PermissionState.GRANTED) {
            JSObject result = new JSObject();
            result.put("granted", true);
            call.resolve(result);
            return;
        }
        requestPermissionForAlias(PERM_NOTIFICATIONS, call, "notificationPermissionCallback");
    }

    @PermissionCallback
    private void notificationPermissionCallback(PluginCall call) {
        JSObject result = new JSObject();
        result.put("granted", getPermissionState(PERM_NOTIFICATIONS) == com.getcapacitor.PermissionState.GRANTED);
        call.resolve(result);
    }

    @PluginMethod
    public void openBatteryOptimizationSettings(PluginCall call) {
        Intent intent = new Intent(Settings.ACTION_REQUEST_IGNORE_BATTERY_OPTIMIZATIONS);
        intent.setData(Uri.parse("package:" + getContext().getPackageName()));
        try {
            startActivityForResult(call, intent, REQUEST_BATTERY_OPTIMIZATION);
        } catch (Exception e) {
            Intent fallback = new Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS);
            fallback.setData(Uri.parse("package:" + getContext().getPackageName()));
            getActivity().startActivity(fallback);
            call.resolve();
        }
    }

    @Override
    protected void handleOnActivityResult(int requestCode, int resultCode, Intent data) {
        super.handleOnActivityResult(requestCode, resultCode, data);
        if (requestCode == REQUEST_BATTERY_OPTIMIZATION) {
            PluginCall call = getSavedCall();
            if (call != null) {
                call.resolve();
            }
        }
    }

    private void startOrUpdateService(String action, String title, String body) {
        Intent intent = new Intent(getContext(), OsmoForegroundService.class);
        intent.setAction(action);
        intent.putExtra(OsmoForegroundService.EXTRA_TITLE, title);
        intent.putExtra(OsmoForegroundService.EXTRA_BODY, body);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            getContext().startForegroundService(intent);
        } else {
            getContext().startService(intent);
        }
    }
}
