import { BluetoothSerial } from '@ascentio-it/capacitor-bluetooth-serial';
import {
  getClassicSppUnavailableMessage,
  isClassicSppSupported,
} from '@/utils/platform';

export interface PairedDevice {
  name: string;
  address: string;
}

export function assertClassicSppSupported(): void {
  if (!isClassicSppSupported()) {
    throw new Error(getClassicSppUnavailableMessage());
  }
}

export async function ensureBluetoothReady(): Promise<void> {
  assertClassicSppSupported();

  const { enabled } = await BluetoothSerial.isEnabled();
  if (!enabled) {
    const canEnable = await BluetoothSerial.canEnable();
    if (canEnable.enabled) {
      await BluetoothSerial.enable();
    } else {
      throw new Error('Enable Bluetooth in system settings.');
    }
  }

  const granted = await BluetoothSerial.checkBluetoothPermissions();
  if (!granted) {
    throw new Error(
      'Bluetooth permissions denied. Grant BLUETOOTH_CONNECT / BLUETOOTH_SCAN (Android 12+) in app settings.',
    );
  }
}

export async function listPairedDevices(): Promise<PairedDevice[]> {
  await ensureBluetoothReady();
  const { devices } = await BluetoothSerial.getPairedDevices();
  return devices.map((d) => ({
    name: d.name?.trim() || 'Unknown',
    address: d.address,
  }));
}

export async function connectDevice(address: string): Promise<void> {
  await ensureBluetoothReady();
  try {
    await BluetoothSerial.connect({ address });
  } catch {
    await BluetoothSerial.connectInsecure({ address });
  }
}

export async function disconnectDevice(address: string): Promise<void> {
  try {
    await BluetoothSerial.disconnect({ address });
  } catch {
    // ignore if already disconnected
  }
}

export async function isDeviceConnected(address: string): Promise<boolean> {
  try {
    const { connected } = await BluetoothSerial.isConnected({ address });
    return connected;
  } catch {
    return false;
  }
}

export async function writeNmea(address: string, payload: string): Promise<void> {
  await BluetoothSerial.write({ address, value: payload });
}
