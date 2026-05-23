import { BleClient } from '@capacitor-community/bluetooth-le';
import { Geolocation } from '@capacitor/geolocation';
import { Capacitor, registerPlugin, type PermissionState } from '@capacitor/core';
import { getAppPlatform } from '@/utils/platform';

/** Capacitor native plugin id for @capacitor-community/bluetooth-le (permission aliases). */
type BlePermissionAlias =
  | 'BLUETOOTH_SCAN'
  | 'BLUETOOTH_CONNECT'
  | 'ACCESS_FINE_LOCATION'
  | 'ACCESS_COARSE_LOCATION'
  | 'BLUETOOTH'
  | 'BLUETOOTH_ADMIN';

type BlePermissionMap = Partial<Record<BlePermissionAlias, PermissionState>>;

interface BluetoothLePermissionsPlugin {
  checkPermissions(): Promise<BlePermissionMap>;
  requestPermissions(options: { permissions: BlePermissionAlias[] }): Promise<BlePermissionMap>;
}

const BluetoothLeNative = registerPlugin<BluetoothLePermissionsPlugin>('BluetoothLe');

let bleStackReady = false;

export type PermissionUiStatus = 'granted' | 'denied' | 'prompt' | 'unavailable';

export interface PermissionRow {
  id: string;
  label: string;
  detail: string;
  status: PermissionUiStatus;
  /** User must open system settings (e.g. permanently denied). */
  needsSettings: boolean;
}

function mapCapacitorState(state: PermissionState | undefined): PermissionUiStatus {
  if (!state || state === 'prompt' || state === 'prompt-with-rationale') {
    return 'prompt';
  }
  if (state === 'granted') {
    return 'granted';
  }
  if (state === 'denied') {
    return 'denied';
  }
  return 'unavailable';
}

function worstStatus(states: PermissionUiStatus[]): PermissionUiStatus {
  if (states.includes('denied')) {
    return 'denied';
  }
  if (states.includes('prompt')) {
    return 'prompt';
  }
  if (states.every((s) => s === 'granted')) {
    return 'granted';
  }
  return 'unavailable';
}

const ANDROID_12_ALIASES: BlePermissionAlias[] = [
  'BLUETOOTH_SCAN',
  'BLUETOOTH_CONNECT',
  'ACCESS_FINE_LOCATION',
  'ACCESS_COARSE_LOCATION',
];

const LEGACY_ANDROID_BLE_ALIASES: BlePermissionAlias[] = [
  'ACCESS_COARSE_LOCATION',
  'ACCESS_FINE_LOCATION',
  'BLUETOOTH',
  'BLUETOOTH_ADMIN',
];

/** Aliases required on this device (inferred from native checkPermissions). */
export function getBlePermissionAliases(map?: BlePermissionMap): BlePermissionAlias[] {
  if (!Capacitor.isNativePlatform() || Capacitor.getPlatform() !== 'android') {
    return [];
  }
  if (map && (map.BLUETOOTH_SCAN !== undefined || map.BLUETOOTH_CONNECT !== undefined)) {
    return ANDROID_12_ALIASES;
  }
  if (map && (map.BLUETOOTH !== undefined || map.BLUETOOTH_ADMIN !== undefined)) {
    return LEGACY_ANDROID_BLE_ALIASES;
  }
  return ANDROID_12_ALIASES;
}

async function readBlePermissionMap(): Promise<BlePermissionMap> {
  if (!Capacitor.isNativePlatform()) {
    return {};
  }
  try {
    return await BluetoothLeNative.checkPermissions();
  } catch {
    return {};
  }
}

async function readLocationPermissionMap(): Promise<{
  location: PermissionUiStatus;
  coarseLocation: PermissionUiStatus;
}> {
  if (!Capacitor.isNativePlatform()) {
    return { location: 'unavailable', coarseLocation: 'unavailable' };
  }
  try {
    const perm = await Geolocation.checkPermissions();
    return {
      location: mapCapacitorState(perm.location),
      coarseLocation: mapCapacitorState(perm.coarseLocation),
    };
  } catch {
    return { location: 'unavailable', coarseLocation: 'unavailable' };
  }
}

/** Read current permission rows for the permissions card (no system dialogs). */
export async function getPermissionRows(): Promise<PermissionRow[]> {
  const platform = getAppPlatform();
  if (platform === 'web') {
    return [
      {
        id: 'native',
        label: 'Native app',
        detail: 'Install the APK/IPA — browser cannot use BLE or GPS.',
        status: 'unavailable',
        needsSettings: false,
      },
    ];
  }

  const bleMap = await readBlePermissionMap();
  const loc = await readLocationPermissionMap();
  const aliases = getBlePermissionAliases(bleMap);
  const rows: PermissionRow[] = [];

  if (platform === 'android') {
    const scan = mapCapacitorState(bleMap.BLUETOOTH_SCAN);
    const connect = mapCapacitorState(bleMap.BLUETOOTH_CONNECT);
    const legacyBt = worstStatus([
      mapCapacitorState(bleMap.BLUETOOTH),
      mapCapacitorState(bleMap.BLUETOOTH_ADMIN),
    ]);
    const nearby = aliases.includes('BLUETOOTH_SCAN')
      ? worstStatus([scan, connect])
      : legacyBt;

    rows.push({
      id: 'bluetooth_nearby',
      label: 'Nearby devices / Bluetooth',
      detail: 'BLUETOOTH_SCAN & BLUETOOTH_CONNECT (Android 12+)',
      status: nearby,
      needsSettings: nearby === 'denied',
    });

    const fine = mapCapacitorState(bleMap.ACCESS_FINE_LOCATION);
    const coarseBle = mapCapacitorState(bleMap.ACCESS_COARSE_LOCATION);
    const locFine = loc.location;
    const locCoarse = loc.coarseLocation;
    const location = worstStatus([fine, coarseBle, locFine, locCoarse]);

    rows.push({
      id: 'location',
      label: 'Location',
      detail: 'Phone GPS + BLE scan on many Android devices',
      status: location,
      needsSettings: location === 'denied',
    });
  } else {
    rows.push({
      id: 'bluetooth',
      label: 'Bluetooth',
      detail: 'Granted on first BLE initialize (iOS system prompt)',
      status: mapCapacitorState(bleMap.BLUETOOTH) === 'unavailable' ? 'prompt' : mapCapacitorState(bleMap.BLUETOOTH),
      needsSettings: false,
    });
    rows.push({
      id: 'location',
      label: 'Location',
      detail: 'While using the app — phone GPS for camera push',
      status: loc.location,
      needsSettings: loc.location === 'denied',
    });
  }

  return rows;
}

function assertAllGranted(map: BlePermissionMap, aliases: BlePermissionAlias[], context: string): void {
  const denied = aliases.filter((a) => map[a] !== 'granted');
  if (denied.length === 0) {
    return;
  }
  throw new Error(
    `${context}: grant ${denied.join(', ')} in app settings (Nearby devices, Bluetooth, Location), then tap Grant permissions again.`,
  );
}

/**
 * Show Android/iOS permission dialogs for BLE (and location on Android), then initialize the BLE stack.
 */
export async function requestBleAndLocationPermissions(): Promise<void> {
  if (!Capacitor.isNativePlatform()) {
    throw new Error(
      'BLE requires a native app on your phone (Android or iOS). Browser preview cannot scan or connect.',
    );
  }

  if (Capacitor.getPlatform() === 'android') {
    let map = await readBlePermissionMap();
    const aliases = getBlePermissionAliases(map);
    const missing = aliases.filter((a) => map[a] !== 'granted');
    if (missing.length > 0) {
      map = await BluetoothLeNative.requestPermissions({ permissions: aliases });
      assertAllGranted(map, aliases, 'Bluetooth');
    }
  }

  const loc = await Geolocation.checkPermissions();
  if (loc.location !== 'granted') {
    const result = await Geolocation.requestPermissions();
    if (result.location !== 'granted') {
      throw new Error(
        'Location permission required for phone GPS and BLE scan on many Android devices. ' +
          'Grant Location in app settings, then retry.',
      );
    }
  }

  try {
    await BleClient.initialize({ androidNeverForLocation: false });
    bleStackReady = true;
  } catch (e) {
    const raw = e instanceof Error ? e.message : String(e);
    if (raw.toLowerCase().includes('permission')) {
      throw new Error(
        'Bluetooth permission denied. On Android 12+, grant Nearby devices / Bluetooth and Location, then retry.',
      );
    }
    throw e instanceof Error ? e : new Error(raw);
  }

  if (Capacitor.getPlatform() === 'android') {
    const enabled = await BleClient.isEnabled();
    if (!enabled) {
      await BleClient.requestEnable();
    }
    const locationOn = await BleClient.isLocationEnabled();
    if (!locationOn) {
      throw new Error(
        'Location services are off. Enable Location in system settings for BLE scan and GPS, then retry.',
      );
    }
  }
}

/** Location only — call before GPS push to camera. */
export async function requestGpsPermissions(): Promise<void> {
  if (!Capacitor.isNativePlatform()) {
    return;
  }
  const perm = await Geolocation.checkPermissions();
  if (perm.location === 'granted') {
    return;
  }
  const result = await Geolocation.requestPermissions();
  if (result.location !== 'granted') {
    throw new Error(
      'Location permission required for phone GPS. Grant Location in app settings, then retry.',
    );
  }
}

export async function openAppPermissionSettings(): Promise<void> {
  if (!Capacitor.isNativePlatform()) {
    return;
  }
  await BleClient.openAppSettings();
}

/** True when tracked rows are granted and BLE initialize succeeded this session. */
export function isReadyForBleScan(rows: PermissionRow[]): boolean {
  if (getAppPlatform() === 'web') {
    return false;
  }
  if (!bleStackReady) {
    return false;
  }
  return !rows.some(
    (r) =>
      r.id !== 'native' &&
      r.id !== 'bluetooth' &&
      (r.status === 'prompt' || r.status === 'denied'),
  );
}
