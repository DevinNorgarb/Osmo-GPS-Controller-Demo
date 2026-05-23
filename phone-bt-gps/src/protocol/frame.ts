import { calculateCrc16 } from '@/protocol/crc16';
import { calculateCrc32 } from '@/protocol/crc32';
import {
  PROTOCOL_HEADER_LENGTH,
  PROTOCOL_TAIL_LENGTH,
  type CmdTypeValue,
  type ParsedFrame,
} from '@/protocol/types';

function writeU16Le(buf: Uint8Array, offset: number, value: number): void {
  buf[offset] = value & 0xff;
  buf[offset + 1] = (value >> 8) & 0xff;
}

function readU16Le(buf: Uint8Array, offset: number): number {
  return buf[offset]! | (buf[offset + 1]! << 8);
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

/** Build a full 0xAA DJI R SDK frame (header + payload + CRC32). */
export function buildFrame(
  cmdSet: number,
  cmdId: number,
  cmdType: CmdTypeValue,
  payload: Uint8Array,
  seq: number,
): Uint8Array {
  const frameLength = PROTOCOL_HEADER_LENGTH + payload.length + PROTOCOL_TAIL_LENGTH;
  const frame = new Uint8Array(frameLength);
  let offset = 0;

  frame[offset++] = 0xaa;

  const verLength = (frameLength & 0x03ff) >>> 0;
  writeU16Le(frame, offset, verLength);
  offset += 2;

  frame[offset++] = cmdType;
  frame[offset++] = 0x00; // ENC
  frame[offset++] = 0x00; // RES
  frame[offset++] = 0x00;
  frame[offset++] = 0x00;

  writeU16Le(frame, offset, seq);
  offset += 2;

  const crc16 = calculateCrc16(frame, offset);
  writeU16Le(frame, offset, crc16);
  offset += 2;

  frame[offset++] = cmdSet;
  frame[offset++] = cmdId;
  frame.set(payload, offset);
  offset += payload.length;

  const crc32 = calculateCrc32(frame, offset);
  frame[offset++] = crc32 & 0xff;
  frame[offset++] = (crc32 >> 8) & 0xff;
  frame[offset++] = (crc32 >> 16) & 0xff;
  frame[offset++] = (crc32 >> 24) & 0xff;

  return frame;
}

/** Parse and validate a single frame; returns null if invalid. */
export function parseFrame(frameData: Uint8Array): ParsedFrame | null {
  if (frameData.length < 16 || frameData[0] !== 0xaa) {
    return null;
  }

  const verLength = readU16Le(frameData, 1);
  const expectedLength = verLength & 0x03ff;
  if (expectedLength !== frameData.length) {
    return null;
  }

  const crc16Received = readU16Le(frameData, 10);
  const crc16Calc = calculateCrc16(frameData, 10);
  if (crc16Received !== crc16Calc) {
    return null;
  }

  const crc32Received = readU32Le(frameData, frameData.length - 4);
  const crc32Calc = calculateCrc32(frameData, frameData.length - 4);
  if (crc32Received !== crc32Calc) {
    return null;
  }

  const dataLength = frameData.length - 16;
  const data =
    dataLength > 0
      ? frameData.slice(14, 14 + dataLength)
      : new Uint8Array(0);

  return {
    cmdType: frameData[3]!,
    seq: readU16Le(frameData, 8),
    cmdSet: frameData[12]!,
    cmdId: frameData[13]!,
    data,
  };
}

/** Extract 0xAA frames from a notify chunk (may include 0x55 prefixes on OA5). */
export function extractAaFrames(chunk: Uint8Array): Uint8Array[] {
  const frames: Uint8Array[] = [];
  let i = 0;
  while (i < chunk.length) {
    if (chunk[i] !== 0xaa) {
      i++;
      continue;
    }
    if (i + 3 > chunk.length) {
      break;
    }
    const verLength = readU16Le(chunk, i + 1);
    const frameLen = verLength & 0x03ff;
    if (frameLen < 16 || i + frameLen > chunk.length) {
      i++;
      continue;
    }
    const candidate = chunk.slice(i, i + frameLen);
    if (parseFrame(candidate)) {
      frames.push(candidate);
      i += frameLen;
    } else {
      i++;
    }
  }
  return frames;
}
