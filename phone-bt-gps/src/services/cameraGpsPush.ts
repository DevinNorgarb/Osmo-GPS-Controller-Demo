import { buildGpsPushFrame, fixToGpsPayload } from '@/protocol/gpsPush';
import { getBleCameraState, writeRawFrame } from '@/services/bleCamera';
import { setGpsBackgroundActive } from '@/services/backgroundService';
import { readCurrentFix, resetMotionHistory } from '@/services/geolocationFix';

/** Start at 1 Hz; can increase toward 10 Hz when stable (docs/Q&A). */
const DEFAULT_PUSH_INTERVAL_MS = 1000;

let intervalId: ReturnType<typeof setInterval> | null = null;
let pushInFlight = false;
let onStatus: ((msg: string) => void) | null = null;
let onError: ((msg: string) => void) | null = null;
let pushCount = 0;

export function startCameraGpsPush(callbacks: {
  onStatus: (message: string) => void;
  onError: (message: string) => void;
  intervalMs?: number;
}): void {
  stopCameraGpsPush();
  resetMotionHistory();
  onStatus = callbacks.onStatus;
  onError = callbacks.onError;
  pushCount = 0;

  const intervalMs = callbacks.intervalMs ?? DEFAULT_PUSH_INTERVAL_MS;

  const tick = async () => {
    if (pushInFlight || getBleCameraState() !== 'protocol_connected') {
      return;
    }
    pushInFlight = true;
    try {
      const { fix, hasFix } = await readCurrentFix();
      const payload = fixToGpsPayload(fix, hasFix);
      if (!payload) {
        onStatus?.('Waiting for GPS fix…');
        return;
      }
      const frame = buildGpsPushFrame(payload);
      await writeRawFrame(frame);
      pushCount++;
      onStatus?.(
        `GPS push #${pushCount} — ${fix.latitude.toFixed(5)}, ${fix.longitude.toFixed(5)}`,
      );
    } catch (e) {
      onError?.(e instanceof Error ? e.message : String(e));
    } finally {
      pushInFlight = false;
    }
  };

  setGpsBackgroundActive(true);
  void tick();
  intervalId = setInterval(() => void tick(), intervalMs);
}

export function stopCameraGpsPush(): void {
  if (intervalId != null) {
    clearInterval(intervalId);
    intervalId = null;
  }
  setGpsBackgroundActive(false);
  onStatus = null;
  onError = null;
  pushInFlight = false;
}

export function isCameraGpsPushing(): boolean {
  return intervalId != null;
}
