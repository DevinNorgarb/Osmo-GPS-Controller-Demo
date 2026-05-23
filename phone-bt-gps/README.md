# Osmo BT GPS (Android)

This directory is a **sibling project** to the ESP32 firmware in this repository (PlatformIO / ESP-IDF under `main/`, `logic/`, and related folders at the repo root).

**Vue 3 + Ionic 8 + Capacitor 8** app with two ways to get phone GPS to the Osmo stack:

| Mode | Transport | Target | Protocol |
|------|-----------|--------|----------|
| **Camera (BLE)** (default) | BLE GATT `FFF0` / notify `FFF4` / write `FFF5` | Osmo camera directly | DJI R SDK binary (connection `0019`, GPS push `0017`) |
| **HC-05 (legacy)** | Bluetooth Classic SPP | HC-05 → ESP32 UART | NMEA 0183 @ 1 Hz |

**Android only** for both modes. iOS is not supported (no Classic SPP; BLE camera mode untested on iOS).

## Camera (BLE) — phone as remote

Mirrors the ESP32 firmware path in `ble/ble.c`, `logic/connect_logic.c`, and `logic/gps_logic.c`:

1. Scan for DJI advertisements (manufacturer bytes `0xAA`, `0x08`, `0xFA` at indices 0, 1, 4).
2. Connect, enable notify on `FFF4`, write on `FFF5`.
3. Protocol handshake (cmd `0x0019`) — same sequence as `connect_logic_protocol_connect`.
4. Push GPS with cmd `0x0017` from phone Geolocation (starts at **1 Hz**; increase in `cameraGpsPush.ts` toward **10 Hz** when stable).

**First pairing:** enable **First-time pairing** so `verify_mode=1`; confirm the code on the camera screen.

**Remote identity** defaults match `key_logic.c` (`device_id` `0x12345678`, example MAC). Adjust in `src/protocol/types.ts` if needed.

## HC-05 (legacy) — ESP32 bridge

- Pair HC-05 in **Android Settings → Bluetooth** (PIN **1234**).
- HC-05 TX/RX → ESP32 GPS UART (DOIT: **GPIO17 TX**, **GPIO16 RX**, **115200** baud).
- App streams **RMC + GGA** NMEA @ 1 Hz.

## Quick usage (BLE camera)

1. Open **Osmo BT GPS** on a physical Android phone (real GPS).
2. Stay on **Camera (BLE)**.
3. **Scan for Osmo cameras** → tap your camera → **Connect & pair protocol**.
4. **Start GPS push to camera** (grant location when prompted).
5. **Stop** / **Disconnect** when done.

## Build

Requirements: **Node.js 20+**, **Android Studio** (SDK 34+), JDK 17.

```bash
cd phone-bt-gps
npm install
npm run build
npx cap add android    # first time only
npx cap sync android
npx cap open android
```

Shortcut: `npm run cap:sync` (build + sync).

Repeat after web changes: `npm run build && npx cap sync android`.

## Permissions (Android)

| Permission | Purpose |
|------------|---------|
| `BLUETOOTH_SCAN` / `BLUETOOTH_CONNECT` | BLE scan + GATT (API 31+) |
| `ACCESS_FINE_LOCATION` / `COARSE` | Phone GPS + BLE scan on many OEMs |
| Classic `BLUETOOTH` | HC-05 mode (API ≤ 30) |

Grant **Nearby devices**, **Bluetooth**, and **Location** for this app if scan or GPS fails.

## Plugins

- **[@capacitor-community/bluetooth-le](https://www.npmjs.com/package/@capacitor-community/bluetooth-le)** — BLE GATT to Osmo camera.
- **[@ascentio-it/capacitor-bluetooth-serial](https://www.npmjs.com/package/@ascentio-it/capacitor-bluetooth-serial)** — Classic SPP for HC-05.
- **[@capacitor/geolocation](https://capacitorjs.com/docs/apis/geolocation)** — phone position for both modes.

## Project layout

| Path | Role |
|------|------|
| `src/protocol/` | DJI frame build/parse, CRC, connection + GPS payloads |
| `src/services/bleCamera.ts` | Scan, connect, notify, protocol handshake |
| `src/services/cameraGpsPush.ts` | Geolocation → `0017` push loop |
| `src/services/geolocationFix.ts` | Shared position → fix helper |
| `src/services/bluetooth.ts` | HC-05 pair list, SPP write |
| `src/services/gpsStream.ts` | NMEA stream @ 1 Hz |
| `src/utils/nmea.ts` | RMC/GGA builders (HC-05 path) |
| `src/views/HomePage.vue` | UI for both modes |

## Limitations / TODO

- **Hardware validation** required on Osmo Action 4/5/6 — handshake and GPS overlay behavior vary by firmware.
- **10 Hz GPS push** — change `intervalMs` in `startCameraGpsPush`; watch BLE throughput and fix stability.
- **iOS** — not targeted; CoreBluetooth pairing flow differs.
- **Key reporting / record** — not implemented on phone (ESP32 still handles shutter via BOOT).
- **Browser / `npm run dev`** — UI only; BLE and SPP need a native build.
