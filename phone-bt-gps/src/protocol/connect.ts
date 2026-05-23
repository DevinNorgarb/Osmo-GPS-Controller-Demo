import { buildFrame, extractAaFrames, parseFrame } from '@/protocol/frame';
import {
  parseConnectionCommand,
  parseConnectionResponse,
  serializeConnectionRequest,
  serializeConnectionResponse,
} from '@/protocol/payloads';
import { CmdType, type RemoteIdentity } from '@/protocol/types';

export type FrameHandler = (frame: ReturnType<typeof parseFrame>) => void;

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
): Promise<ReturnType<typeof parseFrame>> {
  return new Promise((resolve, reject) => {
    const timer = setTimeout(() => {
      unsubscribe();
      reject(new Error(`Timeout waiting for cmd ${cmdSet.toString(16)}/${cmdId.toString(16)}`));
    }, timeoutMs);

    const unsubscribe = subscribe((frame) => {
      if (!frame || frame.cmdSet !== cmdSet || frame.cmdId !== cmdId) {
        return;
      }
      clearTimeout(timer);
      unsubscribe();
      resolve(frame);
    });
  });
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
  const reqFrame = buildConnectionRequestFrame(identity, reqSeq);
  await writeFrame(reqFrame);

  let skippedImmediateResponse = false;

  try {
    const immediate = await waitForFrame(0x00, 0x19, 1000, subscribeFrames);
    const isCommand = (immediate.cmdType & 0x20) === 0;
    if (isCommand) {
      const cmd = parseConnectionCommand(immediate.data);
      if (!cmd || cmd.verifyMode !== 2) {
        throw new Error('Unexpected camera connection command (expected verify_mode=2)');
      }
      if (cmd.verifyData !== 0) {
        throw new Error('Camera rejected connection');
      }
      const ack = buildConnectionResponseFrame(identity, immediate.seq, 0);
      await writeFrame(ack);
      return;
    }

    const resp = parseConnectionResponse(immediate.data);
    if (!resp || resp.retCode !== 0) {
      throw new Error(`Connection handshake failed (ret_code=${resp?.retCode ?? '?'})`);
    }
    skippedImmediateResponse = true;
  } catch (e) {
    if (!(e instanceof Error) || !e.message.startsWith('Timeout')) {
      throw e;
    }
  }

  if (skippedImmediateResponse) {
    const cameraCmd = await waitForFrame(0x00, 0x19, 30000, subscribeFrames);
    const cmd = parseConnectionCommand(cameraCmd.data);
    if (!cmd || cmd.verifyMode !== 2) {
      throw new Error(`Unexpected verify_mode from camera: ${cmd?.verifyMode}`);
    }
    if (cmd.verifyData !== 0) {
      throw new Error('Camera rejected connection');
    }
    const ack = buildConnectionResponseFrame(identity, cameraCmd.seq, 0);
    await writeFrame(ack);
  }
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
