/* SPDX-License-Identifier: MIT */
/*
 * Copyright (C) 2025 SZ DJI Technology Co., Ltd.
 *
 * All information contained herein is, and remains, the property of DJI.
 * The intellectual and technical concepts contained herein are proprietary
 * to DJI and may be covered by U.S. and foreign patents, patents in process,
 * and protected by trade secret or copyright law.  Dissemination of this
 * information, including but not limited to data and other proprietary
 * material(s) incorporated within the information, in any form, is strictly
 * prohibited without the express written consent of DJI.
 *
 * If you receive this source code without DJI's authorization, you may not
 * further disseminate the information, and you must immediately remove the
 * source code and notify DJI of its removal. DJI reserves the right to pursue
 * legal actions against you for any loss(es) or damage(s) caused by your
 * failure to do so.
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "driver/gpio.h"

#include "board_pins.h"
#include "connect_logic.h"
#include "status_logic.h"
#include "gps_logic.h"

#if BOARD_HAS_RGB_LED
#include "led_strip.h"
#endif

#define TAG "LOGIC_LIGHT"
#define LED_STRIP_LENGTH 1

#if BOARD_HAS_RGB_LED
static led_strip_handle_t led_strip = NULL;

static void init_rgb_led(void) {
    led_strip_config_t strip_config = {
        .strip_gpio_num = BOARD_LED_GPIO,
        .max_leds = LED_STRIP_LENGTH,
    };
    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000,
        .flags.with_dma = false,
    };
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    led_strip_clear(led_strip);
    ESP_LOGI(TAG, "RGB LED initialized on GPIO %d", BOARD_LED_GPIO);
}
#else
/* ESP32 DevKit / WROVER: single GPIO LED (active-high; invert in set_rgb_color if your board is active-low) */
static void init_board_led(void) {
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << BOARD_LED_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io);
    gpio_set_level(BOARD_LED_GPIO, 0);
    ESP_LOGI(TAG, "Board LED initialized on GPIO %d (mono)", BOARD_LED_GPIO);
}
#endif

static void set_rgb_color(uint8_t red, uint8_t green, uint8_t blue) {
#if BOARD_HAS_RGB_LED
    led_strip_set_pixel(led_strip, 0, red, green, blue);
    led_strip_refresh(led_strip);
#else
    (void)green;
    (void)blue;
    const int on = (red | green | blue) ? 1 : 0;
#if BOARD_LED_ACTIVE_LOW
    gpio_set_level(BOARD_LED_GPIO, on ? 0 : 1);
#else
    gpio_set_level(BOARD_LED_GPIO, on);
#endif
#endif
}

uint8_t led_red = 0, led_green = 0, led_blue = 0;
bool led_blinking = false;
bool current_led_on = false;

static void update_led_state() {
    connect_state_t current_connect_state = connect_logic_get_state();
    bool current_camera_recording = is_camera_recording();
    bool current_gps_connected = is_gps_found();

    led_blinking = false;

    switch (current_connect_state) {
        case BLE_NOT_INIT:
            led_red = 13;
            led_green = 0;
            led_blue = 0;
            break;

        case BLE_INIT_COMPLETE:
            led_red = 13;
            led_green = 13;
            led_blue = 0;
            break;

        case BLE_SEARCHING:
            led_blinking = true;
            led_red = 0;
            led_green = 0;
            led_blue = 13;
            break;

        case BLE_CONNECTED:
            led_red = 0;
            led_green = 0;
            led_blue = 13;
            break;

        case PROTOCOL_CONNECTED:
            if (current_camera_recording) {
                if (current_gps_connected) {
                    led_blinking = true;
                    led_red = 6;
                    led_green = 0;
                    led_blue = 6;
                } else {
                    led_blinking = true;
                    led_red = 0;
                    led_green = 13;
                    led_blue = 0;
                }
            } else {
                if (current_gps_connected) {
                    led_red = 6;
                    led_green = 0;
                    led_blue = 6;
                } else {
                    led_red = 0;
                    led_green = 13;
                    led_blue = 0;
                }
            }
            break;

        default:
            led_red = 0;
            led_green = 0;
            led_blue = 0;
            break;
    }
}

static void led_state_timer_callback(TimerHandle_t xTimer) {
    (void)xTimer;
    update_led_state();
}

static void led_blink_timer_callback(TimerHandle_t xTimer) {
    (void)xTimer;
    if (led_blinking) {
        if (current_led_on) {
            set_rgb_color(0, 0, 0);
        } else {
            set_rgb_color(led_red, led_green, led_blue);
        }
        current_led_on = !current_led_on;
    } else {
        set_rgb_color(led_red, led_green, led_blue);
    }
}

int init_light_logic() {
#if BOARD_HAS_RGB_LED
    init_rgb_led();
#else
    init_board_led();
#endif

    TimerHandle_t led_state_timer = xTimerCreate("led_state_timer", pdMS_TO_TICKS(500), pdTRUE, (void *)0, led_state_timer_callback);
    if (led_state_timer == NULL) {
        ESP_LOGE(TAG, "Failed to create LED state timer");
        return -1;
    }
    xTimerStart(led_state_timer, 0);

    TimerHandle_t led_blink_timer = xTimerCreate("led_blink_timer", pdMS_TO_TICKS(500), pdTRUE, (void *)0, led_blink_timer_callback);
    if (led_blink_timer == NULL) {
        ESP_LOGE(TAG, "Failed to create LED blink timer");
        return -1;
    }
    xTimerStart(led_blink_timer, 0);
    ESP_LOGI(TAG, "LED timers started");
    return 0;
}
