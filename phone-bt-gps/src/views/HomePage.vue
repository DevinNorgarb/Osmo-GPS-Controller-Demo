<template>
  <ion-page>
    <ion-header>
      <ion-toolbar>
        <ion-title>Osmo BT GPS</ion-title>
        <ion-chip slot="end" :color="platformChipColor" outline class="platform-chip">
          <ion-label>{{ platformChipLabel }}</ion-label>
        </ion-chip>
      </ion-toolbar>
    </ion-header>

    <ion-content class="ion-padding">
      <ion-card v-if="appPlatform === 'web'" color="warning" class="web-banner">
        <ion-card-content>
          <strong>Browser preview</strong> — BLE does not work here. Install the native app:
          <code>npm run cap:sync</code>, open in Android Studio, run on a physical phone. The
          platform chip should read <strong>android</strong> or <strong>ios</strong> on device.
        </ion-card-content>
      </ion-card>

      <ol class="steps">
        <li :class="{ active: flowStep >= 1, done: flowStep > 1 }">
          <span class="step-num">1</span>
          <span class="step-text">Scan &amp; pick camera</span>
        </li>
        <li :class="{ active: flowStep >= 2, done: flowStep > 2 }">
          <span class="step-num">2</span>
          <span class="step-text">Connect &amp; pair protocol</span>
        </li>
        <li :class="{ active: flowStep >= 3, done: cameraPushing }">
          <span class="step-num">3</span>
          <span class="step-text">Push GPS to camera</span>
        </li>
      </ol>

      <ion-card v-if="appPlatform !== 'web'">
        <ion-card-header>
          <ion-card-title>Permissions</ion-card-title>
          <ion-card-subtitle>Required for BLE scan, connect, and GPS push</ion-card-subtitle>
        </ion-card-header>
        <ion-card-content>
          <ion-list lines="none" class="perm-list">
            <ion-item v-for="row in permissionRows" :key="row.id">
              <ion-label>
                <h3>{{ row.label }}</h3>
                <p>{{ row.detail }}</p>
              </ion-label>
              <ion-badge slot="end" :color="permissionBadgeColor(row.status)">
                {{ permissionStatusLabel(row.status) }}
              </ion-badge>
            </ion-item>
          </ion-list>
          <ion-button
            expand="block"
            :disabled="permissionsBusy"
            @click="grantPermissions"
          >
            Grant permissions
          </ion-button>
          <ion-button
            v-if="anyPermissionNeedsSettings"
            expand="block"
            fill="outline"
            color="medium"
            @click="openSettings"
          >
            Open app settings
          </ion-button>
          <p v-if="permissionMessage" class="perm-message">{{ permissionMessage }}</p>
        </ion-card-content>
      </ion-card>

      <ion-card>
        <ion-card-header>
          <ion-card-title>Osmo camera (BLE)</ion-card-title>
          <ion-card-subtitle>{{ bleStatusLabel }}</ion-card-subtitle>
        </ion-card-header>
        <ion-card-content>
          <div v-if="bleState === 'scanning'" class="scan-active">
            <ion-spinner name="crescent" />
            <ion-text color="primary">{{ scanProgressMessage }}</ion-text>
          </div>
          <ion-button
            expand="block"
            fill="outline"
            :disabled="bleBusy || bleState === 'scanning'"
            @click="scanCameras"
          >
            Scan for Osmo cameras
          </ion-button>
          <ion-item lines="none" class="debug-scan-row">
            <ion-label>
              <h3>Show all BLE devices</h3>
              <p>Debug: list non-DJI advertisers (not for connect)</p>
            </ion-label>
            <ion-toggle v-model="showAllBleDevices" :disabled="bleState === 'scanning'" />
          </ion-item>
          <ion-button
            v-if="bleState === 'scanning'"
            expand="block"
            color="medium"
            fill="clear"
            @click="stopScan"
          >
            Stop scan
          </ion-button>
        </ion-card-content>
      </ion-card>

      <ion-list v-if="cameraDevices.length">
        <ion-list-header>Discovered cameras</ion-list-header>
        <ion-item
          v-for="cam in cameraDevices"
          :key="cam.deviceId"
          button
          :detail="false"
          @click="selectCamera(cam)"
        >
          <ion-label>
            <h2>{{ cam.name }}</h2>
            <p>{{ cam.deviceId }}<span v-if="cam.rssi != null"> · {{ cam.rssi }} dBm</span></p>
          </ion-label>
          <ion-icon
            v-if="selectedCamera?.deviceId === cam.deviceId"
            slot="end"
            :icon="checkmarkCircle"
            color="primary"
          />
        </ion-item>
      </ion-list>
      <p v-else class="hint">
        Power on the camera and enable Bluetooth. Scan finds DJI advertisements (0xAA, 0x08, 0xFA).
      </p>

      <div class="actions">
        <ion-button
          expand="block"
          :disabled="!selectedCamera || bleBusy || bleState === 'protocol_connected' || bleState === 'connecting'"
          @click="connectCamera"
        >
          Connect &amp; pair protocol
        </ion-button>
        <ion-button
          expand="block"
          color="medium"
          fill="outline"
          :disabled="bleState === 'idle' || bleState === 'scanning' || bleBusy"
          @click="disconnectCamera"
        >
          Disconnect
        </ion-button>
        <ion-button
          expand="block"
          color="success"
          :disabled="bleState !== 'protocol_connected' || cameraPushing"
          @click="startCameraGps"
        >
          Start GPS push to camera
        </ion-button>
        <ion-button
          expand="block"
          color="danger"
          fill="outline"
          :disabled="!cameraPushing"
          @click="stopCameraGps"
        >
          Stop GPS push
        </ion-button>
      </div>

      <ion-item lines="none" class="verify-row">
        <ion-label>First-time pairing (verify popup on camera)</ion-label>
        <ion-toggle v-model="firstPairing" :disabled="bleState === 'protocol_connected'" />
      </ion-item>

      <ion-card v-if="bleState === 'protocol_connected'">
        <ion-card-header>
          <ion-card-title>Remote controls</ion-card-title>
          <ion-card-subtitle>DJI protocol commands over BLE (FFF5)</ion-card-subtitle>
        </ion-card-header>
        <ion-card-content>
          <div class="remote-actions">
            <ion-button
              expand="block"
              color="primary"
              :disabled="remoteBusy"
              @click="remoteRecordToggle"
            >
              Record / Stop
            </ion-button>
            <ion-button
              expand="block"
              fill="outline"
              :disabled="remoteBusy"
              @click="remoteStopExplicit"
            >
              Stop (1D03)
            </ion-button>
            <ion-button
              expand="block"
              fill="outline"
              :disabled="remoteBusy"
              @click="remoteQuickSwitch"
            >
              Quick Switch Mode
            </ion-button>
            <ion-button
              expand="block"
              color="medium"
              fill="outline"
              :disabled="remoteBusy"
              @click="remoteSleepCamera"
            >
              Sleep camera
            </ion-button>
          </div>
          <p v-if="remoteActionStatus" class="remote-status">
            <strong>Last action:</strong> {{ remoteActionStatus }}
          </p>
        </ion-card-content>
      </ion-card>

      <ion-card v-if="bleStatusMessage">
        <ion-card-header>
          <ion-card-title>Status</ion-card-title>
        </ion-card-header>
        <ion-card-content>
          <code class="status-log">{{ bleStatusMessage }}</code>
        </ion-card-content>
      </ion-card>

      <ion-card v-if="errorMessage">
        <ion-card-content>
          <ion-text color="danger">{{ errorMessage }}</ion-text>
        </ion-card-content>
      </ion-card>
    </ion-content>
  </ion-page>
</template>

<script setup lang="ts">
import { computed, onMounted, onUnmounted, ref } from 'vue';
import {
  IonPage,
  IonHeader,
  IonToolbar,
  IonTitle,
  IonContent,
  IonCard,
  IonCardHeader,
  IonCardTitle,
  IonCardSubtitle,
  IonCardContent,
  IonButton,
  IonList,
  IonListHeader,
  IonItem,
  IonLabel,
  IonIcon,
  IonText,
  IonToggle,
  IonChip,
  IonBadge,
  IonSpinner,
} from '@ionic/vue';
import { checkmarkCircle } from 'ionicons/icons';
import {
  connectDjiCamera,
  disconnectDjiCamera,
  getBleCameraState,
  onBleCameraStateChange,
  sendCameraSleep,
  sendQsKeyReport,
  sendRecordKeyReport,
  sendStopRecord,
  startDjiScan,
  stopDjiScan,
  type BleCameraState,
  type DjiCameraDevice,
  type DjiCommandResult,
} from '@/services/bleCamera';
import {
  isCameraGpsPushing,
  startCameraGpsPush,
  stopCameraGpsPush,
} from '@/services/cameraGpsPush';
import { requestLocationPermissions } from '@/services/geolocationFix';
import {
  getPermissionRows,
  isReadyForBleScan,
  openAppPermissionSettings,
  requestBleAndLocationPermissions,
  type PermissionRow,
  type PermissionUiStatus,
} from '@/services/permissions';
import { DEFAULT_REMOTE_IDENTITY } from '@/protocol/types';
import { getAppPlatform, getPlatformChipLabel, logPlatformDiagnostics } from '@/utils/platform';

const appPlatform = computed(() => getAppPlatform());
const platformChipLabel = computed(() => getPlatformChipLabel());
const platformChipColor = computed(() => {
  if (appPlatform.value === 'android' || appPlatform.value === 'ios') {
    return 'success';
  }
  if (appPlatform.value === 'web') {
    return 'warning';
  }
  return 'medium';
});

const cameraDevices = ref<DjiCameraDevice[]>([]);
const selectedCamera = ref<DjiCameraDevice | null>(null);
const bleState = ref<BleCameraState>('idle');
const bleBusy = ref(false);
const bleStatusMessage = ref('');
const firstPairing = ref(false);
const cameraPushing = ref(false);
const errorMessage = ref('');
const permissionRows = ref<PermissionRow[]>([]);
const permissionsBusy = ref(false);
const permissionMessage = ref('');
const showAllBleDevices = ref(false);
const scanProgressMessage = ref('');
const remoteBusy = ref(false);
const remoteActionStatus = ref('');

let unsubscribeBleState: (() => void) | null = null;
let scanNoDeviceTimeout: ReturnType<typeof setTimeout> | null = null;

const anyPermissionNeedsSettings = computed(() =>
  permissionRows.value.some((r) => r.needsSettings),
);

const flowStep = computed(() => {
  if (cameraPushing.value || bleState.value === 'protocol_connected') {
    return 3;
  }
  if (bleState.value === 'connecting' || bleState.value === 'ble_connected') {
    return 2;
  }
  return 1;
});

const bleStatusLabel = computed(() => {
  switch (bleState.value) {
    case 'idle':
      return 'Step 1 — scan and select a camera';
    case 'scanning':
      return 'Scanning for DJI cameras…';
    case 'connecting':
      return 'Step 2 — connecting BLE + protocol handshake…';
    case 'ble_connected':
      return 'BLE linked — finishing protocol handshake…';
    case 'protocol_connected':
      return cameraPushing.value
        ? 'Step 3 — GPS pushing to camera'
        : 'Step 3 — protocol OK; start GPS push';
    case 'error':
      return 'Connection error — try scan again';
    default:
      return bleState.value;
  }
});

function permissionBadgeColor(status: PermissionUiStatus): string {
  switch (status) {
    case 'granted':
      return 'success';
    case 'denied':
      return 'danger';
    case 'prompt':
      return 'warning';
    default:
      return 'medium';
  }
}

function permissionStatusLabel(status: PermissionUiStatus): string {
  switch (status) {
    case 'granted':
      return 'granted';
    case 'denied':
      return 'denied';
    case 'prompt':
      return 'needed';
    default:
      return 'n/a';
  }
}

async function refreshPermissionRows(): Promise<void> {
  permissionRows.value = await getPermissionRows();
}

async function grantPermissions(): Promise<void> {
  clearError();
  permissionsBusy.value = true;
  permissionMessage.value = '';
  try {
    await requestBleAndLocationPermissions();
    permissionMessage.value = 'Permissions OK — you can scan for cameras.';
  } catch (e) {
    const msg = e instanceof Error ? e.message : String(e);
    permissionMessage.value = msg;
    setError(msg);
  } finally {
    permissionsBusy.value = false;
    await refreshPermissionRows();
  }
}

async function openSettings(): Promise<void> {
  await openAppPermissionSettings();
}

function clearScanTimeout(): void {
  if (scanNoDeviceTimeout) {
    clearTimeout(scanNoDeviceTimeout);
    scanNoDeviceTimeout = null;
  }
}

function clearError(): void {
  errorMessage.value = '';
}

function setError(msg: string): void {
  errorMessage.value = msg;
}

function selectCamera(cam: DjiCameraDevice): void {
  if (cameraPushing.value) {
    return;
  }
  if (cam.debugOnly) {
    setError('This device was listed for debug only (not a DJI match). Pick a camera without “(debug)”.');
    return;
  }
  selectedCamera.value = cam;
  clearError();
}

async function scanCameras(): Promise<void> {
  clearError();
  clearScanTimeout();
  scanProgressMessage.value = '';

  if (appPlatform.value === 'web') {
    setError(
      'BLE scan does not work in the browser. Build and install the native app (platform chip must show android or ios).',
    );
    return;
  }

  bleBusy.value = true;
  cameraDevices.value = [];
  try {
    if (!isReadyForBleScan(permissionRows.value)) {
      scanProgressMessage.value = 'Requesting permissions…';
      await requestBleAndLocationPermissions();
      await refreshPermissionRows();
      if (!isReadyForBleScan(permissionRows.value)) {
        throw new Error(
          'Permissions incomplete. Grant Nearby devices / Bluetooth and Location, then scan again.',
        );
      }
    }

    await startDjiScan(
      (list) => {
        cameraDevices.value = list;
        if (list.length > 0) {
          clearScanTimeout();
          scanProgressMessage.value = `Found ${list.length} device(s) — tap to select.`;
        }
      },
      {
        showAllDevices: showAllBleDevices.value,
        onScanStatus: (msg) => {
          scanProgressMessage.value = msg;
          bleStatusMessage.value = msg;
        },
      },
    );
    bleState.value = getBleCameraState();
    scanProgressMessage.value = 'Scanning…';

    scanNoDeviceTimeout = setTimeout(() => {
      if (bleState.value === 'scanning' && cameraDevices.value.length === 0) {
        const hint =
          'No cameras found after 15s. Turn on the camera, enable Bluetooth pairing/discoverable mode, ' +
          'keep Location on, and move closer. Try “Show all BLE devices” to verify scan is working.';
        scanProgressMessage.value = hint;
        bleStatusMessage.value = hint;
      }
    }, 15_000);
  } catch (e) {
    setError(e instanceof Error ? e.message : String(e));
    scanProgressMessage.value = '';
    await refreshPermissionRows();
  } finally {
    bleBusy.value = false;
  }
}

async function stopScan(): Promise<void> {
  clearScanTimeout();
  scanProgressMessage.value = '';
  await stopDjiScan();
  bleState.value = getBleCameraState();
}

async function connectCamera(): Promise<void> {
  if (!selectedCamera.value) return;
  if (selectedCamera.value.debugOnly) {
    setError('Cannot connect to a debug-only BLE device. Scan without “Show all BLE devices” and pick a DJI camera.');
    return;
  }
  clearError();
  bleBusy.value = true;
  try {
    if (!isReadyForBleScan(permissionRows.value)) {
      await requestBleAndLocationPermissions();
      await refreshPermissionRows();
    }
    const identity = {
      ...DEFAULT_REMOTE_IDENTITY,
      verifyMode: firstPairing.value ? 1 : 0,
      verifyData: firstPairing.value ? Math.floor(Math.random() * 10000) : 0,
    };
    await connectDjiCamera(selectedCamera.value, identity);
    bleStatusMessage.value = 'Protocol connected. Go outdoors for a GPS fix, then start GPS push.';
  } catch (e) {
    setError(e instanceof Error ? e.message : String(e));
    await disconnectDjiCamera();
  } finally {
    bleBusy.value = false;
  }
}

async function disconnectCamera(): Promise<void> {
  stopCameraGps();
  clearError();
  remoteActionStatus.value = '';
  bleBusy.value = true;
  try {
    await disconnectDjiCamera();
    bleStatusMessage.value = '';
  } catch (e) {
    setError(e instanceof Error ? e.message : String(e));
  } finally {
    bleBusy.value = false;
  }
}

async function startCameraGps(): Promise<void> {
  clearError();
  bleBusy.value = true;
  try {
    await requestLocationPermissions();
  } catch (e) {
    setError(e instanceof Error ? e.message : String(e));
    bleBusy.value = false;
    await refreshPermissionRows();
    return;
  } finally {
    bleBusy.value = false;
  }
  cameraPushing.value = true;
  startCameraGpsPush({
    intervalMs: 1000,
    onStatus: (msg) => {
      bleStatusMessage.value = msg;
    },
    onError: (msg) => {
      setError(msg);
      cameraPushing.value = false;
    },
  });
}

function stopCameraGps(): void {
  stopCameraGpsPush();
  cameraPushing.value = false;
}

function formatRemoteResult(label: string, result: DjiCommandResult): string {
  const code =
    result.retCode != null ? ` (ret ${result.retCode}: ${result.message})` : ` — ${result.message}`;
  return `${label}: ${result.ok ? 'sent' : 'failed'}${code}`;
}

async function runRemoteAction(
  label: string,
  action: () => Promise<DjiCommandResult>,
): Promise<void> {
  if (bleState.value !== 'protocol_connected' || remoteBusy.value) {
    return;
  }
  clearError();
  remoteBusy.value = true;
  try {
    const result = await action();
    remoteActionStatus.value = formatRemoteResult(label, result);
    if (!result.ok) {
      setError(remoteActionStatus.value);
    }
  } catch (e) {
    const msg = e instanceof Error ? e.message : String(e);
    remoteActionStatus.value = `${label}: error — ${msg}`;
    setError(msg);
  } finally {
    remoteBusy.value = false;
  }
}

async function remoteRecordToggle(): Promise<void> {
  await runRemoteAction('Record / Stop (0011)', sendRecordKeyReport);
}

async function remoteStopExplicit(): Promise<void> {
  await runRemoteAction('Stop (1D03)', sendStopRecord);
}

async function remoteQuickSwitch(): Promise<void> {
  await runRemoteAction('Quick Switch (0011 QS)', sendQsKeyReport);
}

async function remoteSleepCamera(): Promise<void> {
  await runRemoteAction('Sleep (001A)', sendCameraSleep);
}

onMounted(() => {
  logPlatformDiagnostics();
  void refreshPermissionRows();
  unsubscribeBleState = onBleCameraStateChange((s) => {
    bleState.value = s;
  });
});

onUnmounted(() => {
  clearScanTimeout();
  unsubscribeBleState?.();
  if (isCameraGpsPushing()) {
    stopCameraGpsPush();
  }
  void stopDjiScan();
});
</script>

<style scoped>
.hint {
  color: var(--ion-color-medium);
  font-size: 0.9rem;
  margin: 0 0 1rem;
}

.platform-chip {
  margin-inline-end: 8px;
  font-size: 0.75rem;
  text-transform: lowercase;
}

.web-banner code {
  font-size: 0.85em;
}

.steps {
  list-style: none;
  margin: 0 0 1rem;
  padding: 0;
  display: flex;
  flex-direction: column;
  gap: 0.35rem;
}

.steps li {
  display: flex;
  align-items: center;
  gap: 0.5rem;
  color: var(--ion-color-medium);
  font-size: 0.85rem;
}

.steps li.active {
  color: var(--ion-color-primary);
  font-weight: 600;
}

.steps li.done .step-num {
  background: var(--ion-color-success);
}

.step-num {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  width: 1.4rem;
  height: 1.4rem;
  border-radius: 50%;
  background: var(--ion-color-medium);
  color: var(--ion-color-primary-contrast);
  font-size: 0.75rem;
  font-weight: 700;
}

.steps li.active .step-num {
  background: var(--ion-color-primary);
}

.actions {
  display: flex;
  flex-direction: column;
  gap: 0.5rem;
  margin: 1rem 0;
}

.verify-row {
  --padding-start: 0;
}

.status-log {
  display: block;
  font-size: 0.75rem;
  word-break: break-all;
  white-space: pre-wrap;
}

.perm-list {
  margin-bottom: 0.75rem;
  padding: 0;
}

.perm-list ion-item {
  --padding-start: 0;
}

.perm-message {
  margin: 0.75rem 0 0;
  font-size: 0.85rem;
  color: var(--ion-color-medium);
}

.scan-active {
  display: flex;
  align-items: center;
  gap: 0.75rem;
  margin-bottom: 0.75rem;
  font-size: 0.9rem;
}

.debug-scan-row {
  --padding-start: 0;
  margin-top: 0.25rem;
}

.remote-actions {
  display: flex;
  flex-direction: column;
  gap: 0.5rem;
}

.remote-status {
  margin: 0.75rem 0 0;
  font-size: 0.85rem;
  color: var(--ion-color-medium);
  word-break: break-word;
}
</style>
