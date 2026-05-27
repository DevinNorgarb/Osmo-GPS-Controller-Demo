# ESP32-S3-Touch-LCD-1.28

[Waveshare product page](https://www.waveshare.com/product/mcu-tools/development-boards/esp32/esp32-s3-touch-lcd-1.28.htm) · [Wiki](https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-1.28)

The ESP32-S3-Touch-LCD-1.28 is a low-cost, high-performance microcontroller development board designed by Waveshare. It features an onboard 1.28-inch capacitive touch LCD, a lithium battery charging chip, a 6-axis sensor (3-axis accelerometer and 3-axis gyroscope), and other peripherals. It uses the **ESP32-S3R2** SoC (low-power Wi-Fi and BLE 5.0), plus external **16 MB Flash** and **2 MB PSRAM**. Hardware encryption accelerators, RNG, HMAC, and digital signature modules support IoT security requirements. Multiple low-power states suit IoT, mobile, wearable, and smart-home use cases.

## SKU

| SKU   | Product                    |
|-------|----------------------------|
| 25098 | ESP32-S3-Touch-LCD-1.28    |
| 26959 | ESP32-S3-Touch-LCD-1.28-B  |

## Features

- High-performance Xtensa 32-bit LX7 dual-core processor, up to 240 MHz
- 2.4 GHz Wi-Fi (802.11 b/g/n) and Bluetooth 5 (LE) with onboard antenna
- 512 KB SRAM, 384 KB ROM, 2 MB PSRAM, 16 MB external Flash
- USB Type-C (orientation-independent)
- 1.28-inch capacitive touch LCD, 240×240, 65K colors
- QMI8658 6-axis IMU (accelerometer + gyroscope)
- 3.7 V lithium battery charge/discharge and SH1.0 connector with 6 GPIOs
- Flexible clocking and per-module power control for low-power modes
- USB full-speed serial; GPIOs configurable for peripherals

## Onboard resources

1. **ESP32-S3R2 / ESP32-S3RH2** — Wi‑Fi / Bluetooth SoC, up to 240 MHz, **2 MB PSRAM** on package
2. **W25Q128JVSIQ** — **16 MB** NOR Flash
3. **CH343P** — USB‑to‑UART bridge
4. **MP1605** — Power module (up to **3.3 V / 2 A** output)
5. **ETA6096** — High‑efficiency lithium battery charge manager
6. **QMI8658** — 6‑axis IMU (3‑axis gyro + 3‑axis accelerometer)
7. **MX1.25 battery header** — 2‑pin connector for **3.7 V** Li‑ion (charge/discharge)
8. **USB Type‑C connector** — flashing + serial logging (USB 1.1 host/slave support per Waveshare)
9. **RESET** button
10. **BOOT** button — press/hold during reset to enter download mode

See the [wiki resource diagram](https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-1.28#Onboard_Resources).

## Interfaces

### Type-C

CH343P USB-UART to ESP32-S3 **GPIO43** (`UART_TXD`) and **GPIO44** (`UART_RXD`) for firmware and logs. Auto-download circuit supports flashing over Type-C alone.

### SH1.0 connector

12-pin FPC on the left edge: power (GND, VSYS, 3V3), BOOT/RESET straps, and six GPIOs (15, 16, 17, 18, 21, 33). **VSYS** accepts 5 V input to power the board. This firmware uses **GPIO16/17** as **UART2** for GNSS.

**NEO-M8 GPS wiring (pin table + diagram):** [docs/waveshare-neo-m8-gps-wiring.md](docs/waveshare-neo-m8-gps-wiring.md)

### LCD

1.28-inch display on **4-wire SPI** (up to 80 MHz). Touch on **I2C**. **GPIO2** controls backlight. **GPIO4** and **GPIO5** drive MOSFET switched contacts near the battery holder (e.g. vibration motor). See [schematic](https://files.waveshare.com/wiki/ESP32-S3-LCD-1.28/Esp32-s3-lcd-.128-sch.pdf).

### I2C

**GPIO6** (SDA) and **GPIO7** (SCL) — QMI8658 and touch controller (CST816S).

### MX1.25 / battery ADC

**GPIO1** — Battery voltage via 200 kΩ + 100 kΩ divider. 12-bit SAR ADC; voltage from raw ADC:

```text
V = 3.3 / (1 << 12) * 3 * AD_Value
```

## Pinout

| GPIO  | ESP32-S3R2 | LCD      | SH1.0 | MX1.25 | QMI8658 | Other        |
|-------|------------|----------|-------|--------|---------|--------------|
| GPIO0 | BOOT0      |          |       |        |         |              |
| GPIO1 | ADC        |          |       |        |         | Battery ADC  |
| GPIO2 |            | LCD_BL   |       |        |         | Backlight    |
| GPIO3 |            |          |       |        | INT2    |              |
| GPIO4 |            |          |       |        | INT1    | MOSFET1_CS   |
| GPIO5 |            |          |       |        | TP_INT  | MOSFET2_CS   |
| GPIO6 |            | TP_SDA   | SDA   |        |         | I2C SDA      |
| GPIO7 |            | TP_SCL   | SCL   |        |         | I2C SCL      |
| GPIO8 |            | LCD_DC   |       |        |         |              |
| GPIO9 |            | LCD_CS   |       |        |         |              |
| GPIO10|            | LCD_CLK  |       |        |         | SPI CLK      |
| GPIO11|            | LCD_MOSI |       |        |         |              |
| GPIO12|            | LCD_MISO |       |        |         |              |
| GPIO13|            | TP_RST   |       |        |         |              |
| GPIO14|            | LCD_RST  |       |        |         |              |
| GPIO15|            |          | GPIO15|        |         |              |
| GPIO16|            |          | GPIO16|        |         |              |
| GPIO17|            |          | GPIO17|        |         |              |
| GPIO18|            |          | GPIO18|        |         |              |
| GPIO19|            |          |       |        |         |              |
| GPIO20|            |          |       |        |         |              |
| GPIO21|            |          | GPIO21|        |         |              |
| GPIO33|            |          | GPIO33|        |         |              |

UART (Type-C): **GPIO43** TX, **GPIO44** RX.

Diagrams: [interface description](https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-1.28#Interfaces) · [pinout](https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-1.28#Pinout) · [dimensions](https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-1.28#Dimensions).

## Specifications

### LCD

| Parameter         | Value              | Parameter          | Value        |
|-------------------|--------------------|--------------------|--------------|
| Display driver    | GC9A01A            | Display interface  | SPI          |
| Touch controller  | CST816S            | Touch interface    | I2C          |
| Resolution        | 240×240 RGB        | Display area       | Φ32.4 mm     |
| Panel             | IPS                | Pixel size         | 0.135×0.135 mm |

### IMU (QMI8658)

| Parameter   | Value |
|-------------|-------|
| Sensor      | QMI8658 |
| Accelerometer | 16-bit; ±2, ±4, ±8, ±16 g |
| Gyroscope   | 16-bit; ±16 … ±2048 °/s |

## Development

Supports **Arduino IDE** and **MicroPython**.

- **Arduino** — Large community, libraries, and examples. See wiki: [Working with Arduino](https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-1.28#Arduino_IDE).
- **MicroPython** — Python 3 on-device via REPL. See wiki: [MicroPython](https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-1.28#MicroPython).

**Download mode:** Long-press **BOOT**, tap **RESET**, release **BOOT**.

### TFT_eSPI (Arduino) notes

For [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI) on this board, use HSPI and typical GPIO mapping (verify against schematic):

| Signal | GPIO |
|--------|------|
| MOSI   | 11   |
| MISO   | 12   |
| SCLK   | 10   |
| CS     | 9    |
| DC     | 8    |
| RST    | 14   |
| BL     | 2    |

Add `#define USE_HSPI_PORT` in the user setup to avoid crashes on ESP32-S3.


| ESP32-S3R2 | LCD     | SH1.0  | MX1.25 | QMI8658 | other       |
|------------|---------|--------|--------|---------|-------------|
| GPIO0      |         |        |        |         | BOOT0       |
| GPIO1      |         |        | ADC    |         |             |
| GPIO2      | LCD_BL  |        |        |         |             |
| GPIO3      |         |        |        | INT2    |             |
| GPIO4      |         |        |        | INT1    | MOSFET1_CS  |
| GPIO5      | TP_INT  |        |        |         | MOSFET2_CS  |
| GPIO6      | TP_SDA  |        |        | SDA     |             |
| GPIO7      | TP_SCL  |        |        | SCL     |             |
| GPIO8      | LCD_DC  |        |        |         |             |
| GPIO9      | LCD_CS  |        |        |         |             |
| GPIO10     | LCD_CLK |        |        |         |             |
| GPIO11     | LCD_MOSI|        |        |         |             |
| GPIO12     | LCD_MISO|        |        |         |             |
| GPIO13     | TP_RST  |        |        |         |             |
| GPIO14     | LCD_RST |        |        |         |             |
| GPIO15     |         | GPIO15 |        |         |             |
| GPIO16     |         | GPIO16 |        |         |             |
| GPIO17     |         | GPIO17 |        |         |             |
| GPIO18     |         | GPIO18 |        |         |             |
| GPIO19     |         |        |        |         |             |
| GPIO20     |         |        |        |         |             |
| GPIO21     |         | GPIO21 |        |         |             |
| GPIO33     |         | GPIO33 |        |         |             |