import { nextSeq } from '@/protocol/connect';
import { buildFrame } from '@/protocol/frame';
import { serializeKeyReport } from '@/protocol/payloads';
import { CmdType } from '@/protocol/types';

export const CMD_SET_KEY_REPORT = 0x00;
export const CMD_ID_KEY_REPORT = 0x11;

/** Key codes — protocol_data_segment.md Key Reporting (0011) */
export const KeyCode = {
  RECORD: 0x01,
  QS: 0x02,
  SNAPSHOT: 0x03,
} as const;

/** Report key events (short / long / multi press) */
export const KeyReportMode = {
  PRESS_RELEASE: 0x00,
  EVENT: 0x01,
} as const;

export const KeyEventValue = {
  SHORT_PRESS: 0x00,
  LONG_PRESS: 0x01,
} as const;

export function buildKeyReportFrame(
  keyCode: number,
  seq = nextSeq(),
): Uint8Array {
  const payload = serializeKeyReport(
    keyCode,
    KeyReportMode.EVENT,
    KeyEventValue.SHORT_PRESS,
  );
  return buildFrame(
    CMD_SET_KEY_REPORT,
    CMD_ID_KEY_REPORT,
    CmdType.CMD_RESPONSE_OR_NOT,
    payload,
    seq,
  );
}

/** Shutter short press — toggles capture/record (matches command_logic_key_report_record). */
export function buildRecordKeyReportFrame(seq = nextSeq()): Uint8Array {
  return buildKeyReportFrame(KeyCode.RECORD, seq);
}

/** QS button — cycle quick-switch modes (matches command_logic_key_report_qs). */
export function buildQsKeyReportFrame(seq = nextSeq()): Uint8Array {
  return buildKeyReportFrame(KeyCode.QS, seq);
}
