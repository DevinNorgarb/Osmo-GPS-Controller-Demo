/** DJI R SDK frame types — mirrors logic/enums_logic.h */

export const CmdType = {
  CMD_NO_RESPONSE: 0x00,
  CMD_RESPONSE_OR_NOT: 0x01,
  CMD_WAIT_RESULT: 0x02,
  ACK_NO_RESPONSE: 0x20,
  ACK_RESPONSE_OR_NOT: 0x21,
  ACK_WAIT_RESULT: 0x22,
} as const;

export type CmdTypeValue = (typeof CmdType)[keyof typeof CmdType];

export const PROTOCOL_HEADER_LENGTH = 14;
export const PROTOCOL_TAIL_LENGTH = 4;

export interface ParsedFrame {
  cmdType: number;
  seq: number;
  cmdSet: number;
  cmdId: number;
  data: Uint8Array;
}

/** Remote identity sent in connection request (0019) — matches key_logic.c example. */
export interface RemoteIdentity {
  deviceId: number;
  macAddr: number[];
  fwVersion: number;
  verifyMode: number;
  verifyData: number;
  cameraReserved: number;
}

export const DEFAULT_REMOTE_IDENTITY: RemoteIdentity = {
  deviceId: 0x12345678,
  macAddr: [0x38, 0x34, 0x56, 0x78, 0x9a, 0xbc],
  fwVersion: 0,
  verifyMode: 0,
  verifyData: 0,
  cameraReserved: 0,
};

export interface GpsPushPayload {
  yearMonthDay: number;
  hourMinuteSecond: number;
  gpsLongitude: number;
  gpsLatitude: number;
  heightMm: number;
  speedToNorthCms: number;
  speedToEastCms: number;
  speedDownCms: number;
  verticalAccuracyMm: number;
  horizontalAccuracyMm: number;
  speedAccuracyCms: number;
  satelliteNumber: number;
}
