import { Capacitor, registerPlugin } from '@capacitor/core';

const NOTIFICATION_TITLE = 'Osmo GPS Remote';

interface OsmoBackgroundPlugin {
  startService(options: { title: string; body: string }): Promise<void>;
  updateService(options: { title: string; body: string }): Promise<void>;
  stopService(): Promise<void>;
  checkNotificationPermission(): Promise<{ granted: boolean }>;
  requestNotificationPermission(): Promise<{ granted: boolean }>;
  openBatteryOptimizationSettings(): Promise<void>;
}

const OsmoBackground = registerPlugin<OsmoBackgroundPlugin>('OsmoBackground');

let bleActive = false;
let gpsActive = false;
let serviceRunning = false;
let notificationPermissionRequested = false;

function isAndroidNative(): boolean {
  return Capacitor.isNativePlatform() && Capacitor.getPlatform() === 'android';
}

function buildNotificationBody(): string {
  if (bleActive && gpsActive) {
    return 'Connected to camera — pushing GPS';
  }
  if (bleActive) {
    return 'Connected to camera';
  }
  if (gpsActive) {
    return 'Pushing GPS to camera';
  }
  return 'Running in background';
}

function shouldRunService(): boolean {
  return bleActive || gpsActive;
}

async function ensureNotificationPermission(): Promise<void> {
  if (!isAndroidNative()) {
    return;
  }
  const status = await OsmoBackground.checkNotificationPermission();
  if (status.granted || notificationPermissionRequested) {
    return;
  }
  notificationPermissionRequested = true;
  await OsmoBackground.requestNotificationPermission();
}

/** Sync foreground service with current BLE / GPS activity (Android only). */
export async function syncBackgroundService(): Promise<void> {
  if (!isAndroidNative()) {
    return;
  }

  if (!shouldRunService()) {
    if (serviceRunning) {
      await OsmoBackground.stopService();
      serviceRunning = false;
    }
    return;
  }

  await ensureNotificationPermission();

  const options = {
    title: NOTIFICATION_TITLE,
    body: buildNotificationBody(),
  };

  if (!serviceRunning) {
    await OsmoBackground.startService(options);
    serviceRunning = true;
    return;
  }

  await OsmoBackground.updateService(options);
}

export function setBleBackgroundActive(active: boolean): void {
  bleActive = active;
  void syncBackgroundService();
}

export function setGpsBackgroundActive(active: boolean): void {
  gpsActive = active;
  void syncBackgroundService();
}

export function isBackgroundServiceRunning(): boolean {
  return serviceRunning;
}

/** Android 13+ notification permission for the persistent foreground notification. */
export async function requestBackgroundNotificationPermission(): Promise<boolean> {
  if (!isAndroidNative()) {
    return true;
  }
  const status = await OsmoBackground.checkNotificationPermission();
  if (status.granted) {
    return true;
  }
  notificationPermissionRequested = true;
  const result = await OsmoBackground.requestNotificationPermission();
  return result.granted;
}

/** Optional: prompt user to disable battery optimization for reliable background BLE/GPS. */
export async function openBatteryOptimizationSettings(): Promise<void> {
  if (!isAndroidNative()) {
    return;
  }
  await OsmoBackground.openBatteryOptimizationSettings();
}
