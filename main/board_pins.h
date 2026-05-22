/* SPDX-License-Identifier: MIT */
/*
 * Board pin map: ESP32-C6-DevKitC-1 (reference) vs common ESP32 / WROVER devkits.
 */

#pragma once

#include "driver/gpio.h"
#include "sdkconfig.h"

#if CONFIG_IDF_TARGET_ESP32C6

#define BOARD_BOOT_KEY_GPIO   GPIO_NUM_9
#define BOARD_LED_GPIO        8
#define BOARD_HAS_RGB_LED     1
#define BOARD_GPS_UART_PORT   LP_UART_NUM_0
#define BOARD_GPS_TXD_PIN     GPIO_NUM_5
#define BOARD_GPS_RXD_PIN     GPIO_NUM_4

#else /* ESP32, ESP32-S2/S3 builds use the same devkit-style map when added */

/* BOOT = GPIO0 on most ESP32-DevKitC / WROVER-KIT boards */
#define BOARD_BOOT_KEY_GPIO   GPIO_NUM_0
/* Many WROOM/WROVER boards: single LED on GPIO2 (active-low on some kits) */
#define BOARD_LED_GPIO        GPIO_NUM_2
#define BOARD_HAS_RGB_LED     0
#define BOARD_GPS_UART_PORT   UART_NUM_2
#define BOARD_GPS_TXD_PIN     GPIO_NUM_17
#define BOARD_GPS_RXD_PIN     GPIO_NUM_16

#endif
