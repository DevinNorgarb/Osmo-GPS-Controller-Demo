<template>
  <ion-page>
    <ion-header>
      <ion-toolbar>
        <ion-title>Osmo BT GPS</ion-title>
      </ion-toolbar>
      <ion-toolbar>
        <ion-segment v-model="mode" :disabled="anyBusy">
          <ion-segment-button value="ble">
            <ion-label>Camera (BLE)</ion-label>
          </ion-segment-button>
          <ion-segment-button value="classic">
            <ion-label>HC-05 (legacy)</ion-label>
          </ion-segment-button>
        </ion-segment>
      </ion-toolbar>
    </ion-header>

    <ion-content class="ion-padding">
      <!-- Direct BLE → Osmo camera -->
      <template v-if="mode === 'ble'">
        <ion-card>
          <ion-card-header>
            <ion-card-title>BLE camera</ion-card-title>
            <ion-card-subtitle>{{ bleStatusLabel }}</ion-card-subtitle>
          </ion-card-header>
          <ion-card-content>
            <ion-button
              expand="block"
              fill="outline"
              :disabled="bleBusy || bleState === 'scanning'"
              @click="scanCameras"
            >
              Scan for Osmo cameras
            </ion-button>
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
        <p v-else class="hint">Power on the camera and enable Bluetooth. Scan finds DJI adv (0xAA, 0x08, 0xFA).</p>

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

        <ion-card v-if="bleStatusMessage">
          <ion-card-header>
            <ion-card-title>Status</ion-card-title>
          </ion-card-header>
          <ion-card-content>
            <code class="nmea-log">{{ bleStatusMessage }}</code>
          </ion-card-content>
        </ion-card>
      </template>

      <!-- Legacy HC-05 SPP -->
      <template v-else>
        <ion-card>
          <ion-card-header>
            <ion-card-title>HC-05 bridge</ion-card-title>
            <ion-card-subtitle>{{ classicStatusLabel }}</ion-card-subtitle>
          </ion-card-header>
          <ion-card-content>
            <ion-button expand="block" fill="outline" :disabled="classicBusy" @click="refreshDevices">
              Refresh paired devices
            </ion-button>
          </ion-card-content>
        </ion-card>

        <ion-list v-if="devices.length">
          <ion-list-header>Paired Bluetooth devices</ion-list-header>
          <ion-item
            v-for="device in devices"
            :key="device.address"
            button
            :detail="false"
            @click="selectDevice(device)"
          >
            <ion-label>
              <h2>{{ device.name }}</h2>
              <p>{{ device.address }}</p>
            </ion-label>
            <ion-icon
              v-if="selected?.address === device.address"
              slot="end"
              :icon="checkmarkCircle"
              color="primary"
            />
          </ion-item>
        </ion-list>
        <p v-else class="hint">Pair HC-05 in Android Settings first (PIN 1234).</p>

        <div class="actions">
          <ion-button
            expand="block"
            :disabled="!selected || classicBusy || connectionStatus === 'connected' || connectionStatus === 'streaming'"
            @click="connect"
          >
            Connect
          </ion-button>
          <ion-button
            expand="block"
            color="medium"
            fill="outline"
            :disabled="connectionStatus === 'disconnected' || connectionStatus === 'connecting' || classicBusy"
            @click="disconnect"
          >
            Disconnect
          </ion-button>
          <ion-button
            expand="block"
            color="success"
            :disabled="connectionStatus !== 'connected' && connectionStatus !== 'streaming'"
            @click="startStream"
          >
            Start GPS stream (NMEA)
          </ion-button>
          <ion-button
            expand="block"
            color="danger"
            fill="outline"
            :disabled="connectionStatus !== 'streaming'"
            @click="stopStream"
          >
            Stop
          </ion-button>
        </div>

        <ion-card v-if="lastNmea">
          <ion-card-header>
            <ion-card-title>Last NMEA sent</ion-card-title>
          </ion-card-header>
          <ion-card-content>
            <code class="nmea-log">{{ lastNmea }}</code>
          </ion-card-content>
        </ion-card>
      </template>

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
  IonSegment,
  IonSegmentButton,
  IonToggle,
} from '@ionic/vue';
import { checkmarkCircle } from 'ionicons/icons';
import {
  connectDevice,
  disconnectDevice,
  listPairedDevices,
  type PairedDevice,
} from '@/services/bluetooth';
import {
  isStreaming,
  requestLocationPermissions,
  startGpsStream,
  stopGpsStream,
} from '@/services/gpsStream';
import {
  connectDjiCamera,
  disconnectDjiCamera,
  getBleCameraState,
  onBleCameraStateChange,
  startDjiScan,
  stopDjiScan,
  type BleCameraState,
  type DjiCameraDevice,
} from '@/services/bleCamera';
import {
  isCameraGpsPushing,
  startCameraGpsPush,
  stopCameraGpsPush,
} from '@/services/cameraGpsPush';
import { DEFAULT_REMOTE_IDENTITY } from '@/protocol/types';

type ConnectionStatus = 'disconnected' | 'connecting' | 'connected' | 'streaming';

const mode = ref<'ble' | 'classic'>('ble');

const devices = ref<PairedDevice[]>([]);
const selected = ref<PairedDevice | null>(null);
const connectionStatus = ref<ConnectionStatus>('disconnected');
const lastNmea = ref('');
const errorMessage = ref('');
const classicBusy = ref(false);

const cameraDevices = ref<DjiCameraDevice[]>([]);
const selectedCamera = ref<DjiCameraDevice | null>(null);
const bleState = ref<BleCameraState>('idle');
const bleBusy = ref(false);
const bleStatusMessage = ref('');
const firstPairing = ref(false);
const cameraPushing = ref(false);

let unsubscribeBleState: (() => void) | null = null;

const anyBusy = computed(() => classicBusy.value || bleBusy.value);

const classicStatusLabel = computed(() => {
  switch (connectionStatus.value) {
    case 'disconnected':
      return 'Disconnected';
    case 'connecting':
      return 'Connecting…';
    case 'connected':
      return 'Connected (HC-05 SPP)';
    case 'streaming':
      return 'Streaming NMEA @ 1 Hz';
    default:
      return '';
  }
});

const bleStatusLabel = computed(() => {
  switch (bleState.value) {
    case 'idle':
      return 'Not connected';
    case 'scanning':
      return 'Scanning…';
    case 'connecting':
      return 'Connecting BLE + protocol…';
    case 'ble_connected':
      return 'BLE linked (handshake…)';
    case 'protocol_connected':
      return cameraPushing.value ? 'Protocol OK · GPS pushing' : 'Protocol connected';
    default:
      return bleState.value;
  }
});

function clearError(): void {
  errorMessage.value = '';
}

function setError(msg: string): void {
  errorMessage.value = msg;
}

async function refreshDevices(): Promise<void> {
  clearError();
  classicBusy.value = true;
  try {
    devices.value = await listPairedDevices();
    if (selected.value && !devices.value.find((d) => d.address === selected.value?.address)) {
      selected.value = null;
    }
  } catch (e) {
    setError(e instanceof Error ? e.message : String(e));
  } finally {
    classicBusy.value = false;
  }
}

function selectDevice(device: PairedDevice): void {
  if (connectionStatus.value === 'streaming') {
    return;
  }
  selected.value = device;
  clearError();
}

async function connect(): Promise<void> {
  if (!selected.value) return;
  clearError();
  classicBusy.value = true;
  connectionStatus.value = 'connecting';
  try {
    await requestLocationPermissions();
    await connectDevice(selected.value.address);
    connectionStatus.value = 'connected';
  } catch (e) {
    connectionStatus.value = 'disconnected';
    setError(e instanceof Error ? e.message : String(e));
  } finally {
    classicBusy.value = false;
  }
}

async function disconnect(): Promise<void> {
  if (isStreaming()) {
    stopStream();
  }
  clearError();
  classicBusy.value = true;
  try {
    if (selected.value) {
      await disconnectDevice(selected.value.address);
    }
    connectionStatus.value = 'disconnected';
  } catch (e) {
    setError(e instanceof Error ? e.message : String(e));
  } finally {
    classicBusy.value = false;
  }
}

function startStream(): void {
  if (!selected.value) return;
  clearError();
  connectionStatus.value = 'streaming';
  startGpsStream(selected.value.address, {
    onLine: (line) => {
      lastNmea.value = line;
    },
    onError: (msg) => {
      setError(msg);
      connectionStatus.value = 'connected';
    },
  });
}

function stopStream(): void {
  stopGpsStream();
  connectionStatus.value = 'connected';
  clearError();
}

function selectCamera(cam: DjiCameraDevice): void {
  if (cameraPushing.value) {
    return;
  }
  selectedCamera.value = cam;
  clearError();
}

async function scanCameras(): Promise<void> {
  clearError();
  bleBusy.value = true;
  cameraDevices.value = [];
  try {
    await requestLocationPermissions();
    await startDjiScan((list) => {
      cameraDevices.value = list;
    });
    bleState.value = getBleCameraState();
  } catch (e) {
    setError(e instanceof Error ? e.message : String(e));
  } finally {
    bleBusy.value = false;
  }
}

async function stopScan(): Promise<void> {
  await stopDjiScan();
  bleState.value = getBleCameraState();
}

async function connectCamera(): Promise<void> {
  if (!selectedCamera.value) return;
  clearError();
  bleBusy.value = true;
  try {
    await requestLocationPermissions();
    const identity = {
      ...DEFAULT_REMOTE_IDENTITY,
      verifyMode: firstPairing.value ? 1 : 0,
      verifyData: firstPairing.value ? Math.floor(Math.random() * 10000) : 0,
    };
    await connectDjiCamera(selectedCamera.value, identity);
    bleStatusMessage.value = 'Protocol connected. Start GPS push when outdoors with a fix.';
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

function startCameraGps(): void {
  clearError();
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

onMounted(() => {
  unsubscribeBleState = onBleCameraStateChange((s) => {
    bleState.value = s;
  });
  void refreshDevices();
});

onUnmounted(() => {
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

.actions {
  display: flex;
  flex-direction: column;
  gap: 0.5rem;
  margin: 1rem 0;
}

.verify-row {
  --padding-start: 0;
}

.nmea-log {
  display: block;
  font-size: 0.75rem;
  word-break: break-all;
  white-space: pre-wrap;
}
</style>
