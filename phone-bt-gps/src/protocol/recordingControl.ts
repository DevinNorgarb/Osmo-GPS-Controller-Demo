import { nextSeq } from '@/protocol/connect';
import { buildFrame } from '@/protocol/frame';
import { serializeRecordControl } from '@/protocol/payloads';
import { CmdType } from '@/protocol/types';

export const CMD_SET_RECORD_CONTROL = 0x1d;
export const CMD_ID_RECORD_CONTROL = 0x03;

/** Device ID used by ESP32 command_logic start/stop record (Osmo Action 4). */
export const DEFAULT_RECORD_DEVICE_ID = 0x33ff0000;

export const RecordCtrl = {
  START: 0x00,
  STOP: 0x01,
} as const;

export function buildRecordControlFrame(
  recordCtrl: number,
  deviceId = DEFAULT_RECORD_DEVICE_ID,
  seq = nextSeq(),
): Uint8Array {
  const payload = serializeRecordControl(deviceId, recordCtrl);
  return buildFrame(
    CMD_SET_RECORD_CONTROL,
    CMD_ID_RECORD_CONTROL,
    CmdType.CMD_RESPONSE_OR_NOT,
    payload,
    seq,
  );
}

export function buildStartRecordFrame(seq = nextSeq()): Uint8Array {
  return buildRecordControlFrame(RecordCtrl.START, DEFAULT_RECORD_DEVICE_ID, seq);
}

export function buildStopRecordFrame(seq = nextSeq()): Uint8Array {
  return buildRecordControlFrame(RecordCtrl.STOP, DEFAULT_RECORD_DEVICE_ID, seq);
}
