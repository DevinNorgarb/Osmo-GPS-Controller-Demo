import { buildNmeaBlock } from '@/utils/nmea';
import { writeNmea } from '@/services/bluetooth';
import {
  readCurrentFix,
  requestLocationPermissions,
  resetMotionHistory,
} from '@/services/geolocationFix';

/** DJI recommends 10 Hz GPS push after camera connect (Q&A §9). HC-05 path uses 1 Hz NMEA. */
const STREAM_INTERVAL_MS = 1000;

export type StreamState = 'idle' | 'running';

let intervalId: ReturnType<typeof setInterval> | null = null;
let currentAddress: string | null = null;
let onTick: ((lastLine: string) => void) | null = null;
let onError: ((message: string) => void) | null = null;
let writeInFlight = false;

export { requestLocationPermissions };

async function sendOneFix(address: string): Promise<string> {
  const { fix, hasFix } = await readCurrentFix();
  const block = buildNmeaBlock(fix, hasFix);
  await writeNmea(address, block);
  const lines = block.trim().split('\r\n');
  return lines[lines.length - 1] ?? block;
}

export function startGpsStream(
  address: string,
  callbacks: {
    onLine: (line: string) => void;
    onError: (message: string) => void;
  },
): void {
  stopGpsStream();
  currentAddress = address;
  onTick = callbacks.onLine;
  onError = callbacks.onError;
  resetMotionHistory();

  const tick = async () => {
    if (!currentAddress || writeInFlight) {
      return;
    }
    writeInFlight = true;
    try {
      const lastLine = await sendOneFix(currentAddress);
      onTick?.(lastLine);
    } catch (e) {
      const msg = e instanceof Error ? e.message : String(e);
      onError?.(msg);
    } finally {
      writeInFlight = false;
    }
  };

  void tick();
  intervalId = setInterval(() => void tick(), STREAM_INTERVAL_MS);
}

export function stopGpsStream(): void {
  if (intervalId != null) {
    clearInterval(intervalId);
    intervalId = null;
  }
  currentAddress = null;
  onTick = null;
  onError = null;
  writeInFlight = false;
}

export function isStreaming(): boolean {
  return intervalId != null;
}
