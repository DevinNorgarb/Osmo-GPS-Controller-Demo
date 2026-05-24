import { BleClient, ScanMode, type ScanResult } from '@capacitor-community/bluetooth-le';
import { setBleBackgroundActive } from '@/services/backgroundService';
import { requestBleAndLocationPermissions } from '@/services/permissions';
import { buildCameraSleepFrame, CMD_ID_CAMERA_POWER, CMD_SET_CAMERA_POWER } from '@/protocol/cameraPower';
import {
  dispatchNotifyChunk,
  resetSeq,
  runProtocolConnect,
  type FrameHandler,
  type ParsedFrame,
} from '@/protocol/connect';
import {
  buildQsKeyReportFrame,
  buildRecordKeyReportFrame,
  CMD_ID_KEY_REPORT,
  CMD_SET_KEY_REPORT,
} from '@/protocol/keyReport';
import { parseCommandRetCode, retCodeLabel } from '@/protocol/payloads';
import {
  buildStartRecordFrame,
  buildStopRecordFrame,
  CMD_ID_RECORD_CONTROL,
  CMD_SET_RECORD_CONTROL,
} from '@/protocol/recordingControl';
import { DEFAULT_REMOTE_IDENTITY, type RemoteIdentity } from '@/protocol/types';

export const DJI_SERVICE_UUID = '0000fff0-0000-1000-8000-00805f9b34fb';
export const DJI_NOTIFY_UUID = '0000fff4-0000-1000-8000-00805f9b34fb';
export const DJI_WRITE_UUID = '0000fff5-0000-1000-8000-00805f9b34fb';

export interface DjiCameraDevice {
  deviceId: string;
  name: string;
  rssi?: number;
  /** Present when debug “show all BLE” is on and the device is not a DJI match. */
  debugOnly?: boolean;
}

/** DJI manufacturer company ID (little-endian 0xAA, 0x08 in AD payload). */
export const DJI_MANUFACTURER_COMPANY_ID = 0x08aa;

export interface DjiScanOptions {
  /** List every advertiser (labelled debug-only when not DJI). */
  showAllDevices?: boolean;
  onScanStatus?: (message: string) => void;
}

export type BleCameraState =
  | 'idle'
  | 'scanning'
  | 'connecting'
  | 'ble_connected'
  | 'protocol_connected'
  | 'error';

let connectedDeviceId: string | null = null;
let state: BleCameraState = 'idle';
let stateListeners: Array<(s: BleCameraState) => void> = [];
let frameHandlers: FrameHandler[] = [];
let scanListener: ((devices: DjiCameraDevice[]) => void) | null = null;
const seenDevices = new Map<string, DjiCameraDevice>();

function setState(next: BleCameraState): void {
  state = next;
  for (const fn of stateListeners) {
    fn(next);
  }
}

export function getBleCameraState(): BleCameraState {
  return state;
}

export function onBleCameraStateChange(listener: (s: BleCameraState) => void): () => void {
  stateListeners.push(listener);
  listener(state);
  return () => {
    stateListeners = stateListeners.filter((l) => l !== listener);
  };
}

function subscribeFrames(handler: FrameHandler): () => void {
  frameHandlers.push(handler);
  return () => {
    frameHandlers = frameHandlers.filter((h) => h !== handler);
  };
}

function emitFrames(chunk: Uint8Array): void {
  dispatchNotifyChunk(chunk, (frame) => {
    for (const h of frameHandlers) {
      h(frame);
    }
  });
}

function dataViewToBytes(dv: DataView): Uint8Array {
  return new Uint8Array(dv.buffer, dv.byteOffset, dv.byteLength);
}

function manufacturerValueToBytes(raw: DataView | Uint8Array | string | undefined): Uint8Array | null {
  if (raw == null) {
    return null;
  }
  if (raw instanceof DataView) {
    return dataViewToBytes(raw);
  }
  if (raw instanceof Uint8Array) {
    return raw;
  }
  if (typeof raw === 'string') {
    const hex = raw.replace(/[^0-9a-fA-F]/g, '');
    if (hex.length < 2 || hex.length % 2 !== 0) {
      return null;
    }
    const out = new Uint8Array(hex.length / 2);
    for (let i = 0; i < out.length; i++) {
      out[i] = parseInt(hex.slice(i * 2, i * 2 + 2), 16);
    }
    return out;
  }
  return null;
}

/**
 * Match DJI camera manufacturer data — same rule as ESP32 `bsp_link_is_dji_camera_adv`.
 * Native BLE plugins expose payload *without* the 2-byte company ID; web/raw may include it.
 */
export function matchesDjiManufacturerPayload(bytes: Uint8Array): boolean {
  if (bytes.length >= 5 && bytes[0] === 0xaa && bytes[1] === 0x08 && bytes[4] === 0xfa) {
    return true;
  }
  if (bytes.length >= 3 && bytes[2] === 0xfa) {
    return true;
  }
  return false;
}

function matchesDjiManufacturerEntry(companyKey: string, bytes: Uint8Array): boolean {
  const companyId = Number.parseInt(companyKey, 10);
  if (!Number.isNaN(companyId) && companyId === DJI_MANUFACTURER_COMPANY_ID) {
    return matchesDjiManufacturerPayload(bytes);
  }
  return matchesDjiManufacturerPayload(bytes);
}

/** Manufacturer bytes 0=0xAA, 1=0x08, 4=0xFA — ble.c bsp_link_is_dji_camera_adv */
export function isDjiCameraAdvertisement(result: ScanResult): boolean {
  const mfg = result.manufacturerData;
  if (mfg) {
    for (const key of Object.keys(mfg)) {
      const bytes = manufacturerValueToBytes(mfg[key] as DataView | string);
      if (bytes && matchesDjiManufacturerEntry(key, bytes)) {
        return true;
      }
    }
  }

  const uuids = result.uuids ?? [];
  if (uuids.some((u) => u.toLowerCase().includes('fff0'))) {
    return true;
  }

  const name = (result.localName || result.device.name || '').toLowerCase();
  return name.includes('osmo') || name.includes('dji');
}

function mapBleError(e: unknown, context: 'scan' | 'connect'): Error {
  const raw = e instanceof Error ? e.message : String(e);
  const lower = raw.toLowerCase();
  if (lower.includes('permission') || lower.includes('denied') || lower.includes('not authorized')) {
    return new Error(
      'Bluetooth permission denied. On Android 12+, grant Nearby devices / Bluetooth ' +
        '(BLUETOOTH_SCAN, BLUETOOTH_CONNECT) and Location in app settings, then retry.',
    );
  }
  if (lower.includes('location') && lower.includes('disabled')) {
    return new Error(
      'Location services are off. Enable Location for BLE scan and phone GPS, then retry.',
    );
  }
  if (lower.includes('not enabled') || lower.includes('powered off')) {
    return new Error('Bluetooth is off. Turn on Bluetooth in system settings, then retry.');
  }
  if (context === 'scan' && lower.includes('not available')) {
    return new Error(
      'BLE scan unavailable. Install the native APK on a physical phone (not browser preview).',
    );
  }
  return e instanceof Error ? e : new Error(raw);
}

export async function requestBlePermissions(): Promise<void> {
  try {
    await requestBleAndLocationPermissions();
  } catch (e) {
    throw mapBleError(e, 'scan');
  }
}

function upsertScanDevice(result: ScanResult, djiMatch: boolean, debugOnly: boolean): void {
  const baseName = result.localName || result.device.name || 'Unknown BLE';
  const name = debugOnly
    ? `${baseName} (debug — not DJI)`
    : djiMatch
      ? baseName || 'DJI Camera'
      : baseName;
  const entry: DjiCameraDevice = {
    deviceId: result.device.deviceId,
    name,
    rssi: result.rssi,
    debugOnly: debugOnly || undefined,
  };
  seenDevices.set(entry.deviceId, entry);
  scanListener?.([...seenDevices.values()]);
}

export async function startDjiScan(
  onDevices: (devices: DjiCameraDevice[]) => void,
  options: DjiScanOptions = {},
): Promise<void> {
  const { showAllDevices = false, onScanStatus } = options;

  onScanStatus?.('Checking Bluetooth permissions…');
  await requestBlePermissions();
  await stopDjiScan();

  seenDevices.clear();
  scanListener = onDevices;
  setState('scanning');
  onScanStatus?.('Scanning for BLE devices…');

  console.info('[bleCamera] startDjiScan', { showAllDevices });

  try {
    await BleClient.requestLEScan(
      {
        allowDuplicates: true,
        scanMode: ScanMode.SCAN_MODE_LOW_LATENCY,
      },
      (result: ScanResult) => {
        const djiMatch = isDjiCameraAdvertisement(result);
        if (!djiMatch && !showAllDevices) {
          return;
        }
        upsertScanDevice(result, djiMatch, !djiMatch && showAllDevices);
        if (djiMatch) {
          onScanStatus?.(`Found ${seenDevices.size} camera(s)…`);
        }
      },
    );
    onScanStatus?.('Scan active — ensure the camera is on and Bluetooth discoverable.');
  } catch (e) {
    setState('idle');
    throw mapBleError(e, 'scan');
  }
}

export async function stopDjiScan(): Promise<void> {
  try {
    await BleClient.stopLEScan();
  } catch {
    // ignore
  }
  if (state === 'scanning') {
    setState('idle');
  }
}

async function writeFrame(frame: Uint8Array, withoutResponse = false): Promise<void> {
  if (!connectedDeviceId) {
    throw new Error('Not connected');
  }
  const view = new DataView(frame.buffer, frame.byteOffset, frame.byteLength);
  if (withoutResponse) {
    await BleClient.writeWithoutResponse(
      connectedDeviceId,
      DJI_SERVICE_UUID,
      DJI_WRITE_UUID,
      view,
    );
  } else {
    await BleClient.write(connectedDeviceId, DJI_SERVICE_UUID, DJI_WRITE_UUID, view);
  }
}

export async function connectDjiCamera(
  device: DjiCameraDevice,
  identity: RemoteIdentity = DEFAULT_REMOTE_IDENTITY,
): Promise<void> {
  await requestBlePermissions();
  await stopDjiScan();
  setState('connecting');
  resetSeq();

  const pairingIdentity: RemoteIdentity = {
    ...identity,
    verifyData:
      identity.verifyMode === 1
        ? identity.verifyData || (Math.floor(Math.random() * 10000) & 0xffff)
        : identity.verifyData,
  };

  try {
    await BleClient.connect(device.deviceId, () => {
      connectedDeviceId = null;
      setBleBackgroundActive(false);
      setState('idle');
    });

    connectedDeviceId = device.deviceId;
    await BleClient.discoverServices(device.deviceId);
    setState('ble_connected');
    setBleBackgroundActive(true);

    await BleClient.startNotifications(
      device.deviceId,
      DJI_SERVICE_UUID,
      DJI_NOTIFY_UUID,
      (value) => {
        emitFrames(new Uint8Array(value.buffer, value.byteOffset, value.byteLength));
      },
    );

    await runProtocolConnect(
      pairingIdentity,
      (f) => writeFrame(f, false),
      subscribeFrames,
    );
    setState('protocol_connected');
  } catch (e) {
    connectedDeviceId = null;
    setBleBackgroundActive(false);
    setState('error');
    throw mapBleError(e, 'connect');
  }
}

export async function disconnectDjiCamera(): Promise<void> {
  const id = connectedDeviceId;
  connectedDeviceId = null;
  frameHandlers = [];
  setBleBackgroundActive(false);
  if (id) {
    try {
      await BleClient.stopNotifications(id, DJI_SERVICE_UUID, DJI_NOTIFY_UUID);
      await BleClient.disconnect(id);
    } catch {
      // ignore
    }
  }
  setState('idle');
}

export async function writeRawFrame(frame: Uint8Array, withoutResponse = true): Promise<void> {
  await writeFrame(frame, withoutResponse);
}

export interface DjiCommandResult {
  ok: boolean;
  retCode: number | null;
  message: string;
}

const DEFAULT_COMMAND_TIMEOUT_MS = 5000;

function waitForCommandResponse(
  cmdSet: number,
  cmdId: number,
  timeoutMs: number,
): Promise<ParsedFrame> {
  return new Promise((resolve, reject) => {
    const timer = setTimeout(() => {
      unsubscribe();
      reject(
        new Error(
          `Timeout waiting for response ${cmdSet.toString(16)}/${cmdId.toString(16)}`,
        ),
      );
    }, timeoutMs);

    const unsubscribe = subscribeFrames((frame) => {
      const isAck = (frame.cmdType & 0x20) !== 0;
      if (!isAck || frame.cmdSet !== cmdSet || frame.cmdId !== cmdId) {
        return;
      }
      clearTimeout(timer);
      unsubscribe();
      resolve(frame);
    });
  });
}

/**
 * Write a DJI R SDK frame to FFF5 and optionally wait for the matching ACK.
 */
export async function sendDjiCommand(
  frame: Uint8Array,
  cmdSet: number,
  cmdId: number,
  options: { waitResponse?: boolean; timeoutMs?: number } = {},
): Promise<DjiCommandResult> {
  if (state !== 'protocol_connected') {
    return {
      ok: false,
      retCode: null,
      message: 'Protocol not connected — connect and pair first.',
    };
  }

  const { waitResponse = true, timeoutMs = DEFAULT_COMMAND_TIMEOUT_MS } = options;

  try {
    if (waitResponse) {
      const responsePromise = waitForCommandResponse(cmdSet, cmdId, timeoutMs);
      await writeFrame(frame, false);
      const response = await responsePromise;
      const retCode = parseCommandRetCode(response.data);
      if (retCode == null) {
        return { ok: true, retCode: null, message: 'Command sent (no return code in response).' };
      }
      const ok = retCode === 0;
      return {
        ok,
        retCode,
        message: ok ? 'OK' : retCodeLabel(retCode),
      };
    }

    await writeFrame(frame, false);
    return { ok: true, retCode: null, message: 'Command sent.' };
  } catch (e) {
    const msg = e instanceof Error ? e.message : String(e);
    return { ok: false, retCode: null, message: msg };
  }
}

/** Shutter short press (0011) — toggles record/capture like the camera button. */
export function sendRecordKeyReport(): Promise<DjiCommandResult> {
  return sendDjiCommand(
    buildRecordKeyReportFrame(),
    CMD_SET_KEY_REPORT,
    CMD_ID_KEY_REPORT,
  );
}

/** QS quick-switch mode (0011, key 0x02). */
export function sendQsKeyReport(): Promise<DjiCommandResult> {
  return sendDjiCommand(buildQsKeyReportFrame(), CMD_SET_KEY_REPORT, CMD_ID_KEY_REPORT);
}

/** Explicit start record via 1D03 (fallback when status is known). */
export function sendStartRecord(): Promise<DjiCommandResult> {
  return sendDjiCommand(
    buildStartRecordFrame(),
    CMD_SET_RECORD_CONTROL,
    CMD_ID_RECORD_CONTROL,
  );
}

/** Explicit stop record via 1D03. */
export function sendStopRecord(): Promise<DjiCommandResult> {
  return sendDjiCommand(
    buildStopRecordFrame(),
    CMD_SET_RECORD_CONTROL,
    CMD_ID_RECORD_CONTROL,
  );
}

/** Put camera in sleep mode (001A, power_mode 0x03). */
export function sendCameraSleep(): Promise<DjiCommandResult> {
  return sendDjiCommand(
    buildCameraSleepFrame(),
    CMD_SET_CAMERA_POWER,
    CMD_ID_CAMERA_POWER,
  );
}

export function getConnectedDeviceId(): string | null {
  return connectedDeviceId;
}
