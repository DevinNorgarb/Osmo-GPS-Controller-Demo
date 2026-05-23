import { Capacitor } from '@capacitor/core';

export type AppPlatform = 'android' | 'ios' | 'web';

/** Capacitor platform: `android`, `ios`, or `web` (browser / dev server). */
export function getAppPlatform(): AppPlatform {
  const platform = Capacitor.getPlatform();
  if (platform === 'android' || platform === 'ios') {
    return platform;
  }
  return 'web';
}

/** Short label for UI chip / logs: `android`, `ios`, or `web`. */
export function getPlatformChipLabel(): string {
  return getAppPlatform();
}

export function logPlatformDiagnostics(): void {
  console.info('[Osmo BT GPS] platform', {
    getPlatform: Capacitor.getPlatform(),
    isNativePlatform: Capacitor.isNativePlatform(),
    appPlatform: getAppPlatform(),
  });
}

/** Legacy HC-05 SPP path — kept for `bluetooth.ts` only; not exposed in app UI. */
export function isClassicSppSupported(): boolean {
  return Capacitor.isNativePlatform() && Capacitor.getPlatform() === 'android';
}

export function getClassicSppUnavailableMessage(): string {
  return 'HC-05 Bluetooth Classic mode was removed from this app. Use BLE camera mode.';
}
