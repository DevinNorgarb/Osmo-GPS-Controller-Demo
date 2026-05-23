import { buildFrame } from '@/protocol/frame';
import { serializeGpsPush } from '@/protocol/payloads';
import { CmdType, type GpsPushPayload } from '@/protocol/types';
import { nextSeq } from '@/protocol/connect';
import type { GpsFix } from '@/utils/nmea';

const DEFAULT_H_ACCURACY_MM = 1000;
const DEFAULT_V_ACCURACY_MM = 1000;
const DEFAULT_SPEED_ACCURACY_CMS = 10;

/** Build GPS push (0017) binary frame from a phone fix — mirrors gps_logic.c gps_push_data(). */
export function fixToGpsPayload(fix: GpsFix, hasFix: boolean): GpsPushPayload | null {
  if (!hasFix) {
    return null;
  }

  const d = fix.timestamp;
  const year = d.getUTCFullYear();
  const month = d.getUTCMonth() + 1;
  const day = d.getUTCDate();
  const hour = d.getUTCHours();
  const minute = d.getUTCMinutes();
  const second = d.getUTCSeconds();

  const yearMonthDay = year * 10000 + month * 100 + day;
  const hourMinuteSecond = (hour + 8) * 10000 + minute * 100 + second;

  const headingRad =
    fix.headingDegrees != null && fix.headingDegrees >= 0
      ? (fix.headingDegrees * Math.PI) / 180
      : 0;
  const speedMps = fix.speedMps != null && fix.speedMps >= 0 ? fix.speedMps : 0;
  const speedNorthCms = speedMps * Math.cos(headingRad) * 100;
  const speedEastCms = speedMps * Math.sin(headingRad) * 100;

  const hAccMm =
    fix.accuracyMeters != null && fix.accuracyMeters > 0
      ? Math.round(fix.accuracyMeters * 1000)
      : DEFAULT_H_ACCURACY_MM;
  const vAccMm =
    fix.altitudeAccuracyMeters != null && fix.altitudeAccuracyMeters > 0
      ? Math.round(fix.altitudeAccuracyMeters * 1000)
      : DEFAULT_V_ACCURACY_MM;

  return {
    yearMonthDay,
    hourMinuteSecond,
    gpsLongitude: Math.round(fix.longitude * 1e7),
    gpsLatitude: Math.round(fix.latitude * 1e7),
    heightMm: Math.round(fix.altitudeMeters * 1000),
    speedToNorthCms: speedNorthCms,
    speedToEastCms: speedEastCms,
    speedDownCms: 0,
    verticalAccuracyMm: vAccMm,
    horizontalAccuracyMm: hAccMm,
    speedAccuracyCms: DEFAULT_SPEED_ACCURACY_CMS,
    satelliteNumber: fix.satelliteCount,
  };
}

export function buildGpsPushFrame(payload: GpsPushPayload): Uint8Array {
  const data = serializeGpsPush(payload);
  return buildFrame(0x00, 0x17, CmdType.CMD_NO_RESPONSE, data, nextSeq());
}
