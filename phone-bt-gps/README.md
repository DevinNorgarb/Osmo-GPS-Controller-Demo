# Osmo BT GPS

Sibling to the ESP32 firmware in this repo: a **Vue 3 + Ionic 8 + Capacitor 8** phone app that connects **directly to an Osmo camera over BLE** and pushes phone GPS using the DJI R SDK protocol.

## What it does

1. **Scan** for DJI BLE advertisements (manufacturer bytes `0xAA`, `0x08`, `0xFA` at indices 0, 1, 4).
2. **Connect** GATT service `FFF0`, notify `FFF4`, write `FFF5`.
3. **Protocol handshake** (cmd `0x0019`) — same sequence as ESP32 `connect_logic_protocol_connect`.
4. **GPS push** (cmd `0x0017`) from phone Geolocation at **1 Hz** (tune in `cameraGpsPush.ts` toward 10 Hz when stable).

**First pairing:** enable **First-time pairing** so `verify_mode=1`; confirm the code on the camera screen.

**Remote identity** defaults match ESP32 `key_logic.c` (`device_id` `0x12345678`, example MAC). Adjust in `src/protocol/types.ts` if needed.

> **Legacy:** HC-05 / Bluetooth Classic SPP to ESP32 is **not used** by this app. Old `bluetooth.ts` / `gpsStream.ts` remain in the tree for reference only.

## Quick usage

1. Build and install the native app on a **physical phone** (Android or iOS).
2. **Scan for Osmo cameras** → tap your camera → **Connect & pair protocol**.
3. **Start GPS push to camera** (grant Bluetooth and Location when prompted).
4. **Stop** / **Disconnect** when done.

Browser dev (`npm run dev`) is **UI preview only** — BLE needs a native install. The header chip shows `web` in the browser and `android` / `ios` on device.

## Build

Requirements: **Node.js 20+**, **Android Studio** (SDK 34+) for Android, Xcode for iOS, JDK 17.

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

## Permissions (Android 12+)

On a physical device, open the app and use **Grant permissions** (or tap **Scan** — permissions are requested automatically).

| System prompt | Android permission | Purpose |
|---------------|-------------------|---------|
| **Nearby devices** | `BLUETOOTH_SCAN`, `BLUETOOTH_CONNECT` | BLE scan + GATT |
| **Location** | `ACCESS_FINE_LOCATION`, `ACCESS_COARSE_LOCATION` | Phone GPS; BLE scan on many OEMs |

If you tapped **Don’t allow**, use **Open app settings** on the Permissions card and enable Nearby devices + Location for Osmo BT GPS, then return and tap **Grant permissions** again.

iOS: allow **Bluetooth** and **Location** when prompted on first scan.

## Plugins

- **[@capacitor-community/bluetooth-le](https://www.npmjs.com/package/@capacitor-community/bluetooth-le)** — BLE GATT to Osmo camera.
- **[@capacitor/geolocation](https://capacitorjs.com/docs/apis/geolocation)** — phone position for GPS push.

## Project layout

| Path | Role |
|------|------|
| `src/protocol/` | DJI frame build/parse, CRC, connection + GPS payloads |
| `src/services/bleCamera.ts` | Scan, connect, notify, protocol handshake |
| `src/services/cameraGpsPush.ts` | Geolocation → `0017` push loop |
| `src/services/permissions.ts` | BLE + location permission requests and status |
| `src/services/geolocationFix.ts` | Position → fix helper |
| `src/views/HomePage.vue` | BLE camera UI |
| `src/services/bluetooth.ts` | *(legacy)* HC-05 SPP — unused by UI |
| `src/services/gpsStream.ts` | *(legacy)* NMEA @ 1 Hz — unused by UI |

## Limitations / TODO

- **Hardware validation** on Osmo Action 4/5/6 — handshake and GPS overlay vary by firmware.
- **10 Hz GPS push** — lower `intervalMs` in `startCameraGpsPush`; watch BLE throughput.
- **iOS** — CoreBluetooth pairing differs from Android; test on device.
- **Android BLE reliability** — retry scan after toggling BT; some OEMs need Location on for scan; bond/pair popups vary by camera firmware.
- **Key reporting / record** — not on phone (ESP32 BOOT still handles shutter).
- **Browser** — no BLE; use installed APK/IPA.
