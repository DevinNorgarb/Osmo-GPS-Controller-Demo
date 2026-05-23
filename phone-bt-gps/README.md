# Osmo BT GPS (Android)

This directory is a **sibling project** to the ESP32 firmware in this repository (PlatformIO / ESP-IDF under `main/`, `logic/`, and related folders at the repo root).

Minimal **Vue 3 + Ionic 8 + Capacitor 8** app that streams the phone’s GPS as **NMEA 0183** over **Bluetooth Classic SPP** to an **HC-05 / HC-06** module wired to the ESP32 UART on the [Osmo GPS Controller Demo](../README.md).

**Android only** — HC-05 uses Classic Bluetooth, not BLE.

## Hardware / ESP32

- Pair the HC-05 with the phone in **Android Settings → Bluetooth** (default PIN **1234**).
- HC-05 TX/RX → ESP32 GPS UART (e.g. DOIT DevKit: **GPIO17 TX**, **GPIO16 RX**, **115200** baud — see [`main/board_pins.h`](../main/board_pins.h)).
- When connected and streaming, the HC-05 LED stays **solid**; the ESP32 parses `$GPRMC` / `$GPGGA` (also `$GNRMC` / `$GNGGA`) like the LC76G module.

## NMEA rate

The app sends **RMC + GGA** once per second (**1 Hz**). The ESP32 demo GNSS path can run up to 10 Hz with a module command; 1 Hz is enough for dashboard GPS on the camera.

## Quick usage

1. Pair HC-05 in system Bluetooth settings.
2. Open **Osmo BT GPS** on the phone.
3. **Refresh paired devices** → tap your HC-05 → **Connect**.
4. **Start GPS stream** (grant location when prompted).
5. **Stop** when done, then **Disconnect**.

## Build (development machine)

Requirements: **Node.js 20+**, **Android Studio** (SDK 34+), JDK 17.

```bash
cd phone-bt-gps
npm install
npm run build
npx cap add android    # first time only
npx cap sync android
npx cap open android
```

In Android Studio: select a device/emulator → **Run**.

Or from CLI after sync:

```bash
cd android && ./gradlew assembleDebug
```

Install the APK from `android/app/build/outputs/apk/debug/`.

### Repeat after web changes

```bash
npm run build
npx cap sync android
```

Shortcut: `npm run cap:sync` (build + sync).

## Permissions (Android)

Capacitor Geolocation and `@ascentio-it/capacitor-bluetooth-serial` merge most manifest entries on `cap sync`. Confirm `android/app/src/main/AndroidManifest.xml` includes (or is merged from plugins):

| Permission | Purpose |
|------------|---------|
| `BLUETOOTH` / `BLUETOOTH_ADMIN` | Pre-API 31 |
| `BLUETOOTH_CONNECT` | Connect to paired HC-05 (API 31+) |
| `BLUETOOTH_SCAN` | List paired devices (API 31+) |
| `ACCESS_FINE_LOCATION` | Required for BT scan/connect on many OEMs + GPS |
| `ACCESS_COARSE_LOCATION` | Fallback |

If paired devices stay empty on Android 12+, grant **Nearby devices** / **Bluetooth** and **Location** for this app in system settings.

**Note:** Use a **physical device** with real GPS. Emulators can test BT/UI but not meaningful NMEA fixes.

## Plugin choice

**[@ascentio-it/capacitor-bluetooth-serial](https://www.npmjs.com/package/@ascentio-it/capacitor-bluetooth-serial)** (fork of `@e-is/capacitor-bluetooth-serial`):

- **Bluetooth Classic serial (SPP)** — works with HC-05/HC-06.
- **`getPairedDevices()`** — matches “pair in Settings, then connect in app”.
- **`checkBluetoothPermissions()`** — Android 12+ `BLUETOOTH_CONNECT` / `BLUETOOTH_SCAN`.
- Maintained for **Capacitor 8**; Android-focused.

Not used: `@capacitor-community/bluetooth-le` (BLE only).

## Sample NMEA (with fix)

```
$GPRMC,074700.000,A,2234.732734,N,11356.317512,E,1.67,285.57,220526,,,A*XX
$GPGGA,074700.000,2234.732734,N,11356.317512,E,1,8,1.0,47.379,M,0.0,M,,*XX
```

(`*XX` = computed XOR checksum; CRLF appended on transmit.)

## Limitations

- **Android only** (no iOS Classic SPP in this stack).
- **Browser / `npm run dev`** — UI only; BT write requires native build.
- **10 Hz** stream target (100 ms interval in `src/services/gpsStream.ts`; actual rate depends on the phone GNSS).
- Phone GPS accuracy/heading depend on device sensors; satellite count is estimated from horizontal accuracy when the OS does not expose a count.

## Project layout

| Path | Role |
|------|------|
| `src/utils/nmea.ts` | RMC/GGA/GST builders + checksum |
| `src/services/bluetooth.ts` | Pair list, connect, write |
| `src/services/gpsStream.ts` | Geolocation @ 10 Hz → NMEA |
| `src/views/HomePage.vue` | Single-page UI |
