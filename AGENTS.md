## Learned User Preferences

- Prefer PlatformIO (`pio run -t upload`, `pio device monitor`) for build/flash on this repo rather than native ESP-IDF alone.
- Primary hardware target is the DOIT ESP32 DevKit V1 (not ESP32-C6-only workflows).
- Phone GPS is bridged via HC-05/06 Bluetooth Classic SPP + an Android NMEA app, not firmware WiFi/BLE phone GPS.
- HC-05/06 baud should be set to 115200 once via AT/USB adapter; firmware does not auto-configure the module.

## Learned Workspace Facts

- Default PlatformIO env is `esp32doit-devkit-v1` (DOIT ESP32 DevKit V1, `BOARD_DOIT_ESP32_DEVKIT_V1`).
- LilyGO Lily Pi: PlatformIO env `lilygo-lily-pi` (`CONFIG_LILYGO_LILY_PI`, `sdkconfig.lilygo-lily-pi`); ST7796 480×320 on SPI (MOSI 19, SCLK 18, CS 5, DC 27, BL 12); status UI via LVGL; GPS UART2 still 16/17.
- Waveshare ESP32-S3-Touch-LCD-1.28: PlatformIO env `waveshare-esp32-s3-touch-lcd-1` (`CONFIG_WAVESHARE_ESP32_S3_TOUCH_LCD_128`); GC9A01 240×240 SPI (MOSI 11, SCLK 10, CS 9, DC 8, RST 14, BL 2); GPS UART2 on SH1.0 GPIO16/17; USB serial GPIO43/44. CH343 exposes two macOS ports — use `/dev/cu.wchusbserial*` for upload, not `usbmodem*` (checksum errors on wrong port). **Touch-first UX:** round LVGL screen shows Camera/GPS/Record status in plain language; one bottom pill button cycles CONNECT → CONNECTING → START REC / STOP REC. Tap connect/record (do not long-press BOOT at boot — 3s debounce). BOOT short-press mirrors record when protocol-connected; BOOT long-press cancels connect or disconnects.
- DOIT pins: BOOT GPIO0 (internal pull-up), status LED GPIO2 active-low; optional GPS UART2 RX=GPIO16, TX=GPIO17 @ 115200 8N1.
- Phone→ESP32 GPS wiring: HC TXD→GPIO16 (RX2), HC RXD→GPIO17 (TX2), GND common; HC VCC→ESP32 5V when module power spec is 3.6–6 V (UART is 3.3 V logic).
- Long-press BOOT connect runs in `key_connect` worker; single-press record/stop runs in `key_record` worker—both use 8192-word stacks, not `key_scan_task` (2048 overflows on BLE/protocol work).
- Single-press record should send DJI Key Reporting (cmd 0011, key_code 0x01) first; 1D03 start/stop is fallback when camera status is known.
- `notify_processing_task` in `data/data.c` needs an 8192-word stack on ESP32 (BLE notify parse + hex logging overflowed at 2048).
- Valid phone/GNSS input needs RMC status `A` and GGA fix quality ≥ 1; GPS UART logs with bytes but no `$` usually mean wrong HC baud (not 115200) or non-NMEA data.
- Companion mobile app is a **separate repo**: `Osmo-Phone-BT-GPS` (Vue + Ionic Capacitor). Default local path: `/Users/devinnorgarb/projects/devin/Osmo-Phone-BT-GPS/`. BLE GPS push to camera is the primary path; legacy HC-05 NMEA code remains in that repo for reference only.
