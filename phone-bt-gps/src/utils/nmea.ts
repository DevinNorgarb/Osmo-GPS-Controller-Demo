/** NMEA 0183 sentence builders for ESP32 Osmo GPS controller firmware. */

export interface GpsFix {
  latitude: number;
  longitude: number;
  altitudeMeters: number;
  speedMps: number | null;
  headingDegrees: number | null;
  accuracyMeters: number | null;
  altitudeAccuracyMeters: number | null;
  satelliteCount: number;
  timestamp: Date;
}

const NMEA_SAT_COUNT = 8;
const HDOP_PER_ACCURACY_M = 5;

function pad2(n: number): string {
  return String(n).padStart(2, '0');
}

/** ddmm.mmmmm from signed decimal degrees */
export function decimalToNmeaCoord(degrees: number, isLatitude: boolean): string {
  const abs = Math.abs(degrees);
  const deg = Math.floor(abs);
  const minutes = (abs - deg) * 60;
  const combined = deg * 100 + minutes;
  const precision = isLatitude ? 6 : 6;
  return combined.toFixed(precision);
}

export function latIndicator(lat: number): 'N' | 'S' {
  return lat >= 0 ? 'N' : 'S';
}

export function lonIndicator(lon: number): 'E' | 'W' {
  return lon >= 0 ? 'E' : 'W';
}

/** XOR checksum for chars between $ and * (exclusive). */
export function nmeaChecksum(bodyWithoutDollar: string): string {
  let cs = 0;
  for (let i = 0; i < bodyWithoutDollar.length; i++) {
    cs ^= bodyWithoutDollar.charCodeAt(i);
  }
  return cs.toString(16).toUpperCase().padStart(2, '0');
}

function formatTimeUtc(d: Date): string {
  const h = pad2(d.getUTCHours());
  const m = pad2(d.getUTCMinutes());
  const s = d.getUTCSeconds() + d.getUTCMilliseconds() / 1000;
  return `${h}${m}${s.toFixed(3).padStart(6, '0')}`;
}

function formatDateUtc(d: Date): string {
  const day = pad2(d.getUTCDate());
  const month = pad2(d.getUTCMonth() + 1);
  const year = pad2(d.getUTCFullYear() % 100);
  return `${day}${month}${year}`;
}

function finalizeSentence(body: string): string {
  const checksum = nmeaChecksum(body);
  return `$${body}*${checksum}`;
}

/** Horizontal accuracy (m) → HDOP for GGA field 8 (firmware fallback). */
export function accuracyToHdop(accuracyMeters: number | null): string {
  if (accuracyMeters == null || !Number.isFinite(accuracyMeters) || accuracyMeters <= 0) {
    return '1.0';
  }
  const hdop = accuracyMeters / HDOP_PER_ACCURACY_M;
  return Math.min(99.9, Math.max(0.5, hdop)).toFixed(1);
}

/** Estimate visible satellites from horizontal accuracy when OS does not expose count. */
export function estimateSatelliteCount(accuracyMeters: number | null, hasFix: boolean): number {
  if (!hasFix) {
    return 0;
  }
  if (accuracyMeters == null || !Number.isFinite(accuracyMeters)) {
    return NMEA_SAT_COUNT;
  }
  if (accuracyMeters <= 5) {
    return 12;
  }
  if (accuracyMeters <= 15) {
    return 10;
  }
  return NMEA_SAT_COUNT;
}

function sigmaMeters(value: number | null, fallback: number): string {
  if (value == null || !Number.isFinite(value) || value <= 0) {
    return fallback.toFixed(2);
  }
  return Math.min(99.99, value).toFixed(2);
}

/** Speed m/s → knots for RMC field 8 */
function mpsToKnots(mps: number | null): string {
  if (mps == null || Number.isNaN(mps) || mps < 0) {
    return '0.00';
  }
  return (mps * 1.943844).toFixed(2);
}

function headingField(heading: number | null): string {
  if (heading == null || Number.isNaN(heading) || heading < 0) {
    return '0.00';
  }
  const normalized = heading % 360;
  return normalized.toFixed(2);
}

/**
 * $GPRMC — firmware accepts $GPRMC / $GNRMC; status A when fix available.
 */
export function buildRmc(fix: GpsFix, hasFix: boolean): string {
  const t = formatTimeUtc(fix.timestamp);
  const status = hasFix ? 'A' : 'V';
  const lat = decimalToNmeaCoord(fix.latitude, true);
  const lon = decimalToNmeaCoord(fix.longitude, false);
  const latInd = latIndicator(fix.latitude);
  const lonInd = lonIndicator(fix.longitude);
  const speed = mpsToKnots(fix.speedMps);
  const course = headingField(fix.headingDegrees);
  const date = formatDateUtc(fix.timestamp);
  const body = `GPRMC,${t},${status},${lat},${latInd},${lon},${lonInd},${speed},${course},${date},,,A`;
  return finalizeSentence(body);
}

/**
 * $GPGGA — firmware accepts $GPGGA / $GNGGA; fix quality >= 1 when has fix.
 */
export function buildGga(fix: GpsFix, hasFix: boolean): string {
  const t = formatTimeUtc(fix.timestamp);
  const lat = decimalToNmeaCoord(fix.latitude, true);
  const lon = decimalToNmeaCoord(fix.longitude, false);
  const latInd = latIndicator(fix.latitude);
  const lonInd = lonIndicator(fix.longitude);
  const quality = hasFix ? '1' : '0';
  const sats = String(fix.satelliteCount || NMEA_SAT_COUNT);
  const hdop = accuracyToHdop(fix.accuracyMeters);
  const alt = fix.altitudeMeters.toFixed(3);
  const body = `GPGGA,${t},${lat},${latInd},${lon},${lonInd},${quality},${sats},${hdop},${alt},M,0.0,M,,`;
  return finalizeSentence(body);
}

/**
 * $GPGST — std-dev errors (m) for lat/lon/alt; firmware maps to DJI 0x0017 accuracy fields.
 */
export function buildGst(fix: GpsFix, hasFix: boolean): string {
  const t = formatTimeUtc(fix.timestamp);
  if (!hasFix) {
    const body = `GPGST,${t},0.00,0.00,0.00,0.00,0.00,0.00,0.00`;
    return finalizeSentence(body);
  }
  const horiz = fix.accuracyMeters ?? 10;
  const latSigma = sigmaMeters(fix.accuracyMeters, horiz);
  const lonSigma = sigmaMeters(fix.accuracyMeters, horiz);
  const altSigma = sigmaMeters(fix.altitudeAccuracyMeters, horiz * 1.5);
  const body = `GPGST,${t},0.00,0.00,0.00,0.00,${latSigma},${lonSigma},${altSigma}`;
  return finalizeSentence(body);
}

/** RMC + GGA + GST lines terminated with CRLF (UART / HC-05 convention). */
export function buildNmeaBlock(fix: GpsFix, hasFix: boolean): string {
  const rmc = buildRmc(fix, hasFix);
  const gga = buildGga(fix, hasFix);
  const gst = buildGst(fix, hasFix);
  return `${rmc}\r\n${gga}\r\n${gst}\r\n`;
}
