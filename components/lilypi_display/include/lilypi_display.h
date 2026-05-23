/* SPDX-License-Identifier: MIT */
#pragma once

#include "esp_err.h"
#include "lvgl.h"

#define LILYPI_LCD_HRES  480
#define LILYPI_LCD_VRES  320

/** Panel, backlight, LVGL port. Call before creating widgets. */
esp_err_t lilypi_display_hw_init(void);

/** Default LVGL display from hw_init. */
lv_display_t *lilypi_display_get_lv_display(void);
