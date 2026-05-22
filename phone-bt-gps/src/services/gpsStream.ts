import { Geolocation, type Position } from '@capacitor/geolocation';
import { Capacitor } from '@capacitor/core';
import { buildNmeaBlock, type GpsFix } from '@/utils/nmea';
import { writeNmea } from '@/services/bluetooth';

const STREAM_INTERVAL_MS = 1000;

export type StreamState = 'idle' | 'running';

let intervalId: ReturnType<typeof setInterval> | null = null;
let currentAddress: string | null = null;
let onTick: ((lastLine: string) => void) | null = null;
let onError: ((message: string) => void) | null = null;

function positionToFix(position: Position): { fix: GpsFix; hasFix: boolean } {
  const { coords, timestamp } = position;
  const hasFix =
    coords.latitude !== 0 ||
    coords.longitude !== 0 ||
    (coords.accuracy != null && coords.accuracy < 500);

  const fix: GpsFix = {
    latitude: coords.latitude,
    longitude: coords.longitude,
    altitudeMeters: coords.altitude ?? 0,
    speedMps: coords.speed ?? null,
    headingDegrees: coords.heading ?? null,
    accuracyMeters: coords.accuracy ?? null,
    satelliteCount: 8,
    timestamp: new Date(timestamp),
  };

  return { fix, hasFix: hasFix && Number.isFinite(coords.latitude) };
}

export async function requestLocationPermissions(): Promise<void> {
  if (!Capacitor.isNativePlatform()) {
    return;
  }
  const perm = await Geolocation.checkPermissions();
  if (perm.location === 'granted') {
    return;
  }
  const result = await Geolocation.requestPermissions();
  if (result.location !== 'granted') {
    throw new Error('Location permission required for GPS streaming.');
  }
}

async function sendOneFix(address: string): Promise<string> {
  const position = await Geolocation.getCurrentPosition({
    enableHighAccuracy: true,
    timeout: 8000,
    maximumAge: 500,
  });
  const { fix, hasFix } = positionToFix(position);
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

  const tick = async () => {
    if (!currentAddress) return;
    try {
      const lastLine = await sendOneFix(currentAddress);
      onTick?.(lastLine);
    } catch (e) {
      const msg = e instanceof Error ? e.message : String(e);
      onError?.(msg);
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
}

export function isStreaming(): boolean {
  return intervalId != null;
}
