/** NMEA 0183 sentence builders for ESP32 Osmo GPS controller firmware. */

export interface GpsFix {
  latitude: number;
  longitude: number;
  altitudeMeters: number;
  speedMps: number | null;
  headingDegrees: number | null;
  accuracyMeters: number | null;
  satelliteCount: number;
  timestamp: Date;
}

const NMEA_SAT_COUNT = 8;
const NMEA_HDOP = '1.0';

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
  const alt = fix.altitudeMeters.toFixed(3);
  const body = `GPGGA,${t},${lat},${latInd},${lon},${lonInd},${quality},${sats},${NMEA_HDOP},${alt},M,0.0,M,,`;
  return finalizeSentence(body);
}

/** RMC + GGA lines terminated with CRLF (UART / HC-05 convention). */
export function buildNmeaBlock(fix: GpsFix, hasFix: boolean): string {
  const rmc = buildRmc(fix, hasFix);
  const gga = buildGga(fix, hasFix);
  return `${rmc}\r\n${gga}\r\n`;
}
