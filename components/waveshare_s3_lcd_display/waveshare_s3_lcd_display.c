/* SPDX-License-Identifier: MIT */
/*
 * Waveshare ESP32-S3-Touch-LCD-1.28 — GC9A01 240×240 round panel (SPI).
 * Pin map: https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-1.28#Pinout
 */

#include "waveshare_s3_lcd_display.h"

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_lcd_gc9a01.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"

#define TAG "WS_S3_LCD"

#define PIN_LCD_MOSI   GPIO_NUM_11
#define PIN_LCD_MISO   GPIO_NUM_12
#define PIN_LCD_SCLK   GPIO_NUM_10
#define PIN_LCD_CS     GPIO_NUM_9
#define PIN_LCD_DC     GPIO_NUM_8
#define PIN_LCD_RST    GPIO_NUM_14
#define PIN_LCD_BL     GPIO_NUM_2

#define LCD_SPI_HOST   SPI2_HOST
#define LCD_DRAW_LINES 30

static lv_display_t *s_lv_disp;

static void backlight_on(void)
{
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << PIN_LCD_BL,
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&io);
    gpio_set_level(PIN_LCD_BL, 1);
}

esp_err_t waveshare_s3_lcd_display_hw_init(void)
{
    const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    ESP_RETURN_ON_ERROR(lvgl_port_init(&lvgl_cfg), TAG, "lvgl_port_init");

    spi_bus_config_t bus_cfg = {
        .sclk_io_num = PIN_LCD_SCLK,
        .mosi_io_num = PIN_LCD_MOSI,
        .miso_io_num = PIN_LCD_MISO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = WAVESHARE_S3_LCD_HRES * LCD_DRAW_LINES * sizeof(uint16_t),
    };
    ESP_RETURN_ON_ERROR(spi_bus_initialize(LCD_SPI_HOST, &bus_cfg, SPI_DMA_CH_AUTO), TAG, "spi_bus");

    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = PIN_LCD_DC,
        .cs_gpio_num = PIN_LCD_CS,
        .pclk_hz = 40 * 1000 * 1000,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };
    ESP_RETURN_ON_ERROR(
        esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_SPI_HOST, &io_config, &io_handle),
        TAG, "panel_io_spi");

    esp_lcd_panel_handle_t panel_handle = NULL;
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = PIN_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
        .bits_per_pixel = 16,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_gc9a01(io_handle, &panel_config, &panel_handle), TAG, "panel_gc9a01");

    esp_lcd_panel_reset(panel_handle);
    esp_lcd_panel_init(panel_handle);
    esp_lcd_panel_invert_color(panel_handle, true);
    esp_lcd_panel_mirror(panel_handle, true, false);
    esp_lcd_panel_disp_on_off(panel_handle, true);

    backlight_on();

    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = io_handle,
        .panel_handle = panel_handle,
        .buffer_size = WAVESHARE_S3_LCD_HRES * LCD_DRAW_LINES,
        .double_buffer = true,
        .hres = WAVESHARE_S3_LCD_HRES,
        .vres = WAVESHARE_S3_LCD_VRES,
        .monochrome = false,
        .color_format = LV_COLOR_FORMAT_RGB565,
        .rotation = {
            .swap_xy = false,
            .mirror_x = true,
            .mirror_y = false,
        },
        .flags = {
            .buff_dma = true,
        },
    };
    s_lv_disp = lvgl_port_add_disp(&disp_cfg);
    if (s_lv_disp == NULL) {
        ESP_LOGE(TAG, "lvgl_port_add_disp failed");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Display ready %dx%d", WAVESHARE_S3_LCD_HRES, WAVESHARE_S3_LCD_VRES);
    return ESP_OK;
}

lv_display_t *waveshare_s3_lcd_display_get_lv_display(void)
{
    return s_lv_disp;
}
