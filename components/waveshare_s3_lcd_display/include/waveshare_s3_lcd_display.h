/* SPDX-License-Identifier: MIT */
#pragma once

#include "esp_err.h"
#include "lvgl.h"

#define WAVESHARE_S3_LCD_HRES  240
#define WAVESHARE_S3_LCD_VRES  240

/** GC9A01 panel, backlight, LVGL port. Call before creating widgets. */
esp_err_t waveshare_s3_lcd_display_hw_init(void);

/** Default LVGL display from hw_init. */
lv_display_t *waveshare_s3_lcd_display_get_lv_display(void);
