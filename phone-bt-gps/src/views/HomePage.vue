<template>
  <ion-page>
    <ion-header>
      <ion-toolbar>
        <ion-title>Osmo BT GPS</ion-title>
      </ion-toolbar>
    </ion-header>

    <ion-content class="ion-padding">
      <ion-card>
        <ion-card-header>
          <ion-card-title>Connection</ion-card-title>
          <ion-card-subtitle>{{ statusLabel }}</ion-card-subtitle>
        </ion-card-header>
        <ion-card-content>
          <ion-button expand="block" fill="outline" :disabled="busy" @click="refreshDevices">
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
          :disabled="!selected || busy || connectionStatus === 'connected' || connectionStatus === 'streaming'"
          @click="connect"
        >
          Connect
        </ion-button>
        <ion-button
          expand="block"
          color="medium"
          fill="outline"
          :disabled="connectionStatus === 'disconnected' || connectionStatus === 'connecting' || busy"
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
          Start GPS stream
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

      <ion-card v-if="errorMessage">
        <ion-card-content>
          <ion-text color="danger">{{ errorMessage }}</ion-text>
        </ion-card-content>
      </ion-card>
    </ion-content>
  </ion-page>
</template>

<script setup lang="ts">
import { computed, onMounted, ref } from 'vue';
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

type ConnectionStatus = 'disconnected' | 'connecting' | 'connected' | 'streaming';

const devices = ref<PairedDevice[]>([]);
const selected = ref<PairedDevice | null>(null);
const connectionStatus = ref<ConnectionStatus>('disconnected');
const lastNmea = ref('');
const errorMessage = ref('');
const busy = ref(false);

const statusLabel = computed(() => {
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

function clearError(): void {
  errorMessage.value = '';
}

function setError(msg: string): void {
  errorMessage.value = msg;
}

async function refreshDevices(): Promise<void> {
  clearError();
  busy.value = true;
  try {
    devices.value = await listPairedDevices();
    if (selected.value && !devices.value.find((d) => d.address === selected.value?.address)) {
      selected.value = null;
    }
  } catch (e) {
    setError(e instanceof Error ? e.message : String(e));
  } finally {
    busy.value = false;
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
  busy.value = true;
  connectionStatus.value = 'connecting';
  try {
    await requestLocationPermissions();
    await connectDevice(selected.value.address);
    connectionStatus.value = 'connected';
  } catch (e) {
    connectionStatus.value = 'disconnected';
    setError(e instanceof Error ? e.message : String(e));
  } finally {
    busy.value = false;
  }
}

async function disconnect(): Promise<void> {
  if (isStreaming()) {
    stopStream();
  }
  clearError();
  busy.value = true;
  try {
    if (selected.value) {
      await disconnectDevice(selected.value.address);
    }
    connectionStatus.value = 'disconnected';
  } catch (e) {
    setError(e instanceof Error ? e.message : String(e));
  } finally {
    busy.value = false;
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

onMounted(() => {
  void refreshDevices();
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

.nmea-log {
  display: block;
  font-size: 0.75rem;
  word-break: break-all;
  white-space: pre-wrap;
}
</style>
