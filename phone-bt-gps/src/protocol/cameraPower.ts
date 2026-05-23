import { nextSeq } from '@/protocol/connect';
import { buildFrame } from '@/protocol/frame';
import { serializeCameraPowerMode } from '@/protocol/payloads';
import { CmdType } from '@/protocol/types';

export const CMD_SET_CAMERA_POWER = 0x00;
export const CMD_ID_CAMERA_POWER = 0x1a;

export const PowerMode = {
  NORMAL: 0x00,
  SLEEP: 0x03,
} as const;

/** Sleep camera — cmd 001A (matches command_logic_power_mode_switch_sleep). */
export function buildCameraSleepFrame(seq = nextSeq()): Uint8Array {
  const payload = serializeCameraPowerMode(PowerMode.SLEEP);
  return buildFrame(
    CMD_SET_CAMERA_POWER,
    CMD_ID_CAMERA_POWER,
    CmdType.CMD_RESPONSE_OR_NOT,
    payload,
    seq,
  );
}
