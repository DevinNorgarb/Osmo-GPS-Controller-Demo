import { buildFrame, extractAaFrames, parseFrame } from '@/protocol/frame';
import {
  parseConnectionCommand,
  parseConnectionResponse,
  serializeConnectionRequest,
  serializeConnectionResponse,
} from '@/protocol/payloads';
import { CmdType, type RemoteIdentity } from '@/protocol/types';

export type ParsedFrame = NonNullable<ReturnType<typeof parseFrame>>;
export type FrameHandler = (frame: ParsedFrame) => void;

let seqCounter = 0;

export function nextSeq(): number {
  seqCounter = (seqCounter + 1) & 0xffff;
  return seqCounter;
}

export function resetSeq(): void {
  seqCounter = 0;
}

export function buildConnectionRequestFrame(identity: RemoteIdentity, seq: number): Uint8Array {
  const payload = serializeConnectionRequest(identity);
  return buildFrame(0x00, 0x19, CmdType.CMD_WAIT_RESULT, payload, seq);
}

export function buildConnectionResponseFrame(
  identity: RemoteIdentity,
  seq: number,
  retCode: number,
): Uint8Array {
  const payload = serializeConnectionResponse(
    identity.deviceId,
    retCode,
    identity.cameraReserved,
  );
  return buildFrame(0x00, 0x19, CmdType.ACK_NO_RESPONSE, payload, seq);
}

function waitForFrame(
  cmdSet: number,
  cmdId: number,
  timeoutMs: number,
  subscribe: (handler: FrameHandler) => () => void,
): Promise<ParsedFrame> {
  return new Promise((resolve, reject) => {
    const timer = setTimeout(() => {
      unsubscribe();
      reject(new Error(`Timeout waiting for cmd ${cmdSet.toString(16)}/${cmdId.toString(16)}`));
    }, timeoutMs);

    const unsubscribe = subscribe((frame) => {
      if (frame.cmdSet !== cmdSet || frame.cmdId !== cmdId) {
        return;
      }
      clearTimeout(timer);
      unsubscribe();
      resolve(frame);
    });
  });
}

async function handleCameraConnectionCommand(
  frame: ParsedFrame,
  identity: RemoteIdentity,
  writeFrame: (frame: Uint8Array) => Promise<void>,
): Promise<void> {
  const cmd = parseConnectionCommand(frame.data);
  if (!cmd || cmd.verifyMode !== 2) {
    throw new Error(`Unexpected verify_mode from camera: ${cmd?.verifyMode ?? '?'}`);
  }
  if (cmd.verifyData !== 0) {
    throw new Error('Camera rejected connection');
  }
  const ack = buildConnectionResponseFrame(identity, frame.seq, 0);
  await writeFrame(ack);
}

/**
 * DJI connection handshake (0019) — mirrors connect_logic_protocol_connect.
 */
export async function runProtocolConnect(
  identity: RemoteIdentity,
  writeFrame: (frame: Uint8Array) => Promise<void>,
  subscribeFrames: (handler: FrameHandler) => () => void,
): Promise<void> {
  const reqSeq = nextSeq();
  await writeFrame(buildConnectionRequestFrame(identity, reqSeq));

  let immediate: ParsedFrame;
  try {
    immediate = await waitForFrame(0x00, 0x19, 1000, subscribeFrames);
  } catch {
    throw new Error('No response to connection request (timeout)');
  }

  const isCommand = (immediate.cmdType & 0x20) === 0;
  if (isCommand) {
    await handleCameraConnectionCommand(immediate, identity, writeFrame);
    return;
  }

  const resp = parseConnectionResponse(immediate.data);
  if (!resp || resp.retCode !== 0) {
    throw new Error(`Connection handshake failed (ret_code=${resp?.retCode ?? '?'})`);
  }

  const cameraCmd = await waitForFrame(0x00, 0x19, 30000, subscribeFrames);
  await handleCameraConnectionCommand(cameraCmd, identity, writeFrame);
}

/** Feed raw notify bytes into frame handlers. */
export function dispatchNotifyChunk(chunk: Uint8Array, handler: FrameHandler): void {
  for (const frameBytes of extractAaFrames(chunk)) {
    const parsed = parseFrame(frameBytes);
    if (parsed) {
      handler(parsed);
    }
  }
}
