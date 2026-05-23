import type { GpsPushPayload, RemoteIdentity } from '@/protocol/types';

function writeU32Le(buf: Uint8Array, offset: number, value: number): void {
  const v = value >>> 0;
  buf[offset] = v & 0xff;
  buf[offset + 1] = (v >> 8) & 0xff;
  buf[offset + 2] = (v >> 16) & 0xff;
  buf[offset + 3] = (v >> 24) & 0xff;
}

function writeI32Le(buf: Uint8Array, offset: number, value: number): void {
  writeU32Le(buf, offset, value | 0);
}

function writeFloatLe(buf: Uint8Array, offset: number, value: number): void {
  const view = new DataView(buf.buffer, buf.byteOffset + offset, 4);
  view.setFloat32(0, value, true);
}

function readU8(buf: Uint8Array, offset: number): number {
  return buf[offset] ?? 0;
}

function readU16Le(buf: Uint8Array, offset: number): number {
  return (buf[offset] ?? 0) | ((buf[offset + 1] ?? 0) << 8);
}

function readU32Le(buf: Uint8Array, offset: number): number {
  return (
    (buf[offset]! |
      (buf[offset + 1]! << 8) |
      (buf[offset + 2]! << 16) |
      (buf[offset + 3]! << 24)) >>>
    0
  );
}

/** connection_request_command_frame (33 bytes) */
export function serializeConnectionRequest(identity: RemoteIdentity): Uint8Array {
  const buf = new Uint8Array(33);
  writeU32Le(buf, 0, identity.deviceId);
  buf[4] = identity.macAddr.length & 0xff;
  for (let i = 0; i < 16; i++) {
    buf[5 + i] = i < identity.macAddr.length ? identity.macAddr[i]! : 0;
  }
  writeU32Le(buf, 21, identity.fwVersion);
  buf[25] = 0; // conidx
  buf[26] = identity.verifyMode & 0xff;
  writeU16Le(buf, 27, identity.verifyData);
  // reserved[4] already zero
  return buf;
}

function writeU16Le(buf: Uint8Array, offset: number, value: number): void {
  buf[offset] = value & 0xff;
  buf[offset + 1] = (value >> 8) & 0xff;
}

/** connection_request_response_frame (9 bytes) */
export function serializeConnectionResponse(
  deviceId: number,
  retCode: number,
  cameraReserved: number,
): Uint8Array {
  const buf = new Uint8Array(9);
  writeU32Le(buf, 0, deviceId);
  buf[4] = retCode & 0xff;
  buf[5] = cameraReserved & 0xff;
  return buf;
}

export interface ParsedConnectionCommand {
  deviceId: number;
  macAddrLen: number;
  verifyMode: number;
  verifyData: number;
}

export interface ParsedConnectionResponse {
  deviceId: number;
  retCode: number;
  cameraNumber: number;
}

export function parseConnectionCommand(data: Uint8Array): ParsedConnectionCommand | null {
  if (data.length < 33) {
    return null;
  }
  return {
    deviceId: readU32Le(data, 0),
    macAddrLen: readU8(data, 4),
    verifyMode: readU8(data, 26),
    verifyData: readU16Le(data, 27),
  };
}

export function parseConnectionResponse(data: Uint8Array): ParsedConnectionResponse | null {
  if (data.length < 5) {
    return null;
  }
  return {
    deviceId: readU32Le(data, 0),
    retCode: readU8(data, 4),
    cameraNumber: readU8(data, 5),
  };
}

/** gps_data_push_command_frame (48 bytes) */
/** key_report_command_frame (4 bytes) — cmd 0011 */
export function serializeKeyReport(
  keyCode: number,
  mode: number,
  keyValue: number,
): Uint8Array {
  const buf = new Uint8Array(4);
  buf[0] = keyCode & 0xff;
  buf[1] = mode & 0xff;
  writeU16Le(buf, 2, keyValue);
  return buf;
}

/** record_control_command_frame (9 bytes) — cmd 1D03 */
export function serializeRecordControl(deviceId: number, recordCtrl: number): Uint8Array {
  const buf = new Uint8Array(9);
  writeU32Le(buf, 0, deviceId);
  buf[4] = recordCtrl & 0xff;
  return buf;
}

/** camera_power_mode_switch_command_frame (1 byte) — cmd 001A */
export function serializeCameraPowerMode(powerMode: number): Uint8Array {
  return new Uint8Array([powerMode & 0xff]);
}

export function parseCommandRetCode(data: Uint8Array): number | null {
  if (data.length < 1) {
    return null;
  }
  return data[0]!;
}

export function retCodeLabel(retCode: number): string {
  switch (retCode) {
    case 0x00:
      return 'OK';
    case 0x01:
      return 'parse error';
    case 0x02:
      return 'execution failed';
    case 0xff:
      return 'undefined error';
    default:
      return `code 0x${retCode.toString(16)}`;
  }
}

export function serializeGpsPush(payload: GpsPushPayload): Uint8Array {
  const buf = new Uint8Array(48);
  writeI32Le(buf, 0, payload.yearMonthDay);
  writeI32Le(buf, 4, payload.hourMinuteSecond);
  writeI32Le(buf, 8, payload.gpsLongitude);
  writeI32Le(buf, 12, payload.gpsLatitude);
  writeI32Le(buf, 16, payload.heightMm);
  writeFloatLe(buf, 20, payload.speedToNorthCms);
  writeFloatLe(buf, 24, payload.speedToEastCms);
  writeFloatLe(buf, 28, payload.speedDownCms);
  writeU32Le(buf, 32, payload.verticalAccuracyMm);
  writeU32Le(buf, 36, payload.horizontalAccuracyMm);
  writeU32Le(buf, 40, payload.speedAccuracyCms);
  writeU32Le(buf, 44, payload.satelliteNumber);
  return buf;
}
