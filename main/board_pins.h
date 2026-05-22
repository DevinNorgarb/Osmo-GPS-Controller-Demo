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

#else /* ESP32 (DOIT DevKit V1, WROVER-KIT, generic WROOM devkits) */

/* BOOT button on DOIT ESP32 DevKit V1 and most 30-pin WROOM boards */
#define BOARD_BOOT_KEY_GPIO   GPIO_NUM_0
#define BOARD_LED_GPIO        GPIO_NUM_2
#define BOARD_HAS_RGB_LED     0
#define BOARD_GPS_UART_PORT   UART_NUM_2
#define BOARD_GPS_TXD_PIN     GPIO_NUM_17
#define BOARD_GPS_RXD_PIN     GPIO_NUM_16

/* DOIT onboard LED: on when GPIO is LOW (see platformio build_flags for this board) */
#if defined(BOARD_DOIT_ESP32_DEVKIT_V1)
#define BOARD_LED_ACTIVE_LOW  1
#else
#define BOARD_LED_ACTIVE_LOW  0
#endif

#endif
