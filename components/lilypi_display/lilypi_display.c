/* SPDX-License-Identifier: MIT */

#include "lilypi_display.h"

#include "sdkconfig.h"

#if CONFIG_LILYGO_LILY_PI

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_st7796.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"

#define TAG "LILYPI_DISP"

#define PIN_LCD_MOSI   GPIO_NUM_19
#define PIN_LCD_SCLK   GPIO_NUM_18
#define PIN_LCD_CS     GPIO_NUM_5
#define PIN_LCD_DC     GPIO_NUM_27
#define PIN_LCD_MISO   GPIO_NUM_23
#define PIN_LCD_BL     GPIO_NUM_12

#define LCD_SPI_HOST   SPI2_HOST
#define LCD_DRAW_LINES 40

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

esp_err_t lilypi_display_hw_init(void)
{
    const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    ESP_RETURN_ON_ERROR(lvgl_port_init(&lvgl_cfg), TAG, "lvgl_port_init");

    spi_bus_config_t bus_cfg = {
        .sclk_io_num = PIN_LCD_SCLK,
        .mosi_io_num = PIN_LCD_MOSI,
        .miso_io_num = PIN_LCD_MISO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LILYPI_LCD_HRES * LCD_DRAW_LINES * sizeof(uint16_t),
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
        .reset_gpio_num = GPIO_NUM_NC,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
        .bits_per_pixel = 16,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_st7796(io_handle, &panel_config, &panel_handle), TAG, "panel_st7796");

    esp_lcd_panel_reset(panel_handle);
    esp_lcd_panel_init(panel_handle);
    esp_lcd_panel_mirror(panel_handle, true, false);
    esp_lcd_panel_disp_on_off(panel_handle, true);

    backlight_on();

    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = io_handle,
        .panel_handle = panel_handle,
        .buffer_size = LILYPI_LCD_HRES * LCD_DRAW_LINES,
        .double_buffer = true,
        .hres = LILYPI_LCD_HRES,
        .vres = LILYPI_LCD_VRES,
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

    ESP_LOGI(TAG, "Display ready %dx%d", LILYPI_LCD_HRES, LILYPI_LCD_VRES);
    return ESP_OK;
}

lv_display_t *lilypi_display_get_lv_display(void)
{
    return s_lv_disp;
}

#else /* CONFIG_LILYGO_LILY_PI */

esp_err_t lilypi_display_hw_init(void)
{
    return ESP_ERR_NOT_SUPPORTED;
}

lv_display_t *lilypi_display_get_lv_display(void)
{
    return NULL;
}

#endif /* CONFIG_LILYGO_LILY_PI */
