import { BleClient, type ScanResult } from '@capacitor-community/bluetooth-le';
import { Capacitor } from '@capacitor/core';
import {
  dispatchNotifyChunk,
  resetSeq,
  runProtocolConnect,
  type FrameHandler,
} from '@/protocol/connect';
import { DEFAULT_REMOTE_IDENTITY, type RemoteIdentity } from '@/protocol/types';

export const DJI_SERVICE_UUID = '0000fff0-0000-1000-8000-00805f9b34fb';
export const DJI_NOTIFY_UUID = '0000fff4-0000-1000-8000-00805f9b34fb';
export const DJI_WRITE_UUID = '0000fff5-0000-1000-8000-00805f9b34fb';

export interface DjiCameraDevice {
  deviceId: string;
  name: string;
  rssi?: number;
}

export type BleCameraState =
  | 'idle'
  | 'scanning'
  | 'connecting'
  | 'ble_connected'
  | 'protocol_connected'
  | 'error';

let bleInitialized = false;
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

/** Manufacturer bytes 0=0xAA, 1=0x08, 4=0xFA — ble.c bsp_link_is_dji_camera_adv */
export function isDjiCameraAdvertisement(result: ScanResult): boolean {
  const mfg = result.manufacturerData;
  if (!mfg) {
    return false;
  }
  for (const key of Object.keys(mfg)) {
    const raw = mfg[key];
    if (!raw) {
      continue;
    }
    const bytes = raw instanceof DataView ? dataViewToBytes(raw) : new Uint8Array(raw as ArrayBuffer);
    if (bytes.length >= 5 && bytes[0] === 0xaa && bytes[1] === 0x08 && bytes[4] === 0xfa) {
      return true;
    }
  }
  return false;
}

export async function initializeBle(): Promise<void> {
  if (!Capacitor.isNativePlatform()) {
    throw new Error('BLE camera mode requires a native Android build.');
  }
  if (!bleInitialized) {
    await BleClient.initialize({ androidNeverForLocation: false });
    bleInitialized = true;
  }
}

export async function requestBlePermissions(): Promise<void> {
  await initializeBle();
  const enabled = await BleClient.isEnabled();
  if (!enabled) {
    await BleClient.requestEnable();
  }
}

export async function startDjiScan(
  onDevices: (devices: DjiCameraDevice[]) => void,
): Promise<void> {
  await requestBlePermissions();
  seenDevices.clear();
  scanListener = onDevices;
  setState('scanning');

  await BleClient.requestLEScan({ allowDuplicates: true }, (result: ScanResult) => {
    if (!isDjiCameraAdvertisement(result)) {
      return;
    }
    const name = result.localName || result.device.name || 'DJI Camera';
    const entry: DjiCameraDevice = {
      deviceId: result.device.deviceId,
      name,
      rssi: result.rssi,
    };
    seenDevices.set(entry.deviceId, entry);
    scanListener?.([...seenDevices.values()]);
  });
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

  await BleClient.connect(device.deviceId, () => {
    connectedDeviceId = null;
    setState('idle');
  });

  connectedDeviceId = device.deviceId;
  await BleClient.discoverServices(device.deviceId);
  setState('ble_connected');

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
}

export async function disconnectDjiCamera(): Promise<void> {
  const id = connectedDeviceId;
  connectedDeviceId = null;
  frameHandlers = [];
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

export function getConnectedDeviceId(): string | null {
  return connectedDeviceId;
}
