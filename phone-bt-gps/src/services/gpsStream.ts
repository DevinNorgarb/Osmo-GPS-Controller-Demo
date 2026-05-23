import { Geolocation, type Position } from '@capacitor/geolocation';
import { Capacitor } from '@capacitor/core';
import { buildNmeaBlock, estimateSatelliteCount, type GpsFix } from '@/utils/nmea';
import { writeNmea } from '@/services/bluetooth';

/** DJI recommends 10 Hz GPS push after camera connect (Q&A §9). */
const STREAM_INTERVAL_MS = 100;

export type StreamState = 'idle' | 'running';

let intervalId: ReturnType<typeof setInterval> | null = null;
let currentAddress: string | null = null;
let onTick: ((lastLine: string) => void) | null = null;
let onError: ((message: string) => void) | null = null;
let writeInFlight = false;
let prevSample: { lat: number; lon: number; t: number } | null = null;

const EARTH_RADIUS_M = 6371000;

function haversineMeters(lat1: number, lon1: number, lat2: number, lon2: number): number {
  const toRad = (d: number) => (d * Math.PI) / 180;
  const dLat = toRad(lat2 - lat1);
  const dLon = toRad(lon2 - lon1);
  const a =
    Math.sin(dLat / 2) ** 2 +
    Math.cos(toRad(lat1)) * Math.cos(toRad(lat2)) * Math.sin(dLon / 2) ** 2;
  return 2 * EARTH_RADIUS_M * Math.atan2(Math.sqrt(a), Math.sqrt(1 - a));
}

function bearingDegrees(lat1: number, lon1: number, lat2: number, lon2: number): number {
  const toRad = (d: number) => (d * Math.PI) / 180;
  const toDeg = (r: number) => (r * 180) / Math.PI;
  const dLon = toRad(lon2 - lon1);
  const y = Math.sin(dLon) * Math.cos(toRad(lat2));
  const x =
    Math.cos(toRad(lat1)) * Math.sin(toRad(lat2)) -
    Math.sin(toRad(lat1)) * Math.cos(toRad(lat2)) * Math.cos(dLon);
  return (toDeg(Math.atan2(y, x)) + 360) % 360;
}

function enrichMotionFromPrevious(fix: GpsFix, hasFix: boolean): GpsFix {
  if (!hasFix || prevSample == null) {
    return fix;
  }
  const dt = (fix.timestamp.getTime() - prevSample.t) / 1000;
  if (dt <= 0.05 || dt > 2) {
    return fix;
  }
  const dist = haversineMeters(prevSample.lat, prevSample.lon, fix.latitude, fix.longitude);
  const derivedSpeed = dist / dt;
  return {
    ...fix,
    speedMps:
      fix.speedMps != null && fix.speedMps > 0.1 ? fix.speedMps : derivedSpeed,
    headingDegrees:
      fix.headingDegrees != null && fix.headingDegrees >= 0
        ? fix.headingDegrees
        : bearingDegrees(prevSample.lat, prevSample.lon, fix.latitude, fix.longitude),
  };
}

function positionToFix(position: Position): { fix: GpsFix; hasFix: boolean } {
  const { coords, timestamp } = position;
  const accuracy = coords.accuracy ?? null;
  const hasCoords =
    Number.isFinite(coords.latitude) &&
    Number.isFinite(coords.longitude) &&
    (coords.latitude !== 0 || coords.longitude !== 0);
  const hasFix =
    hasCoords && (accuracy == null || (accuracy > 0 && accuracy < 500));

  const altitudeAccuracy =
    'altitudeAccuracy' in coords
      ? (coords as { altitudeAccuracy?: number | null }).altitudeAccuracy ?? null
      : null;

  const fix: GpsFix = {
    latitude: coords.latitude,
    longitude: coords.longitude,
    altitudeMeters:
      coords.altitude != null && Number.isFinite(coords.altitude) ? coords.altitude : 0,
    speedMps: coords.speed != null && coords.speed >= 0 ? coords.speed : null,
    headingDegrees: coords.heading != null && coords.heading >= 0 ? coords.heading : null,
    accuracyMeters: accuracy,
    altitudeAccuracyMeters: altitudeAccuracy,
    satelliteCount: estimateSatelliteCount(accuracy, hasFix),
    timestamp: new Date(timestamp),
  };

  const enriched = enrichMotionFromPrevious(fix, hasFix);
  if (hasFix) {
    prevSample = {
      lat: enriched.latitude,
      lon: enriched.longitude,
      t: enriched.timestamp.getTime(),
    };
  }

  return { fix: enriched, hasFix };
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
    maximumAge: 0,
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
  prevSample = null;

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
  prevSample = null;
  writeInFlight = false;
}

export function isStreaming(): boolean {
  return intervalId != null;
}
