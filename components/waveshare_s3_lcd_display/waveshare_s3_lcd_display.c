/* SPDX-License-Identifier: MIT */
/*
 * Waveshare ESP32-S3-Touch-LCD-1.28 — GC9A01 240×240 round panel (SPI).
 * Pin map: https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-1.28#Pinout
 */

#include "waveshare_s3_lcd_display.h"

#include "sdkconfig.h"

#if CONFIG_WAVESHARE_ESP32_S3_TOUCH_LCD_128

#include "driver/ledc.h"
#include "driver/i2c_master.h"
#include "driver/spi_master.h"
#include "esp_lcd_gc9a01.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_touch_cst816s.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "esp_lvgl_port_touch.h"

#define TAG "WS_S3_LCD"

#define PIN_LCD_MOSI   GPIO_NUM_11
#define PIN_LCD_MISO   GPIO_NUM_12
#define PIN_LCD_SCLK   GPIO_NUM_10
#define PIN_LCD_CS     GPIO_NUM_9
#define PIN_LCD_DC     GPIO_NUM_8
#define PIN_LCD_RST    GPIO_NUM_14
#define PIN_LCD_BL     GPIO_NUM_2

#define PIN_TP_INT     GPIO_NUM_5
#define PIN_TP_RST     GPIO_NUM_13
#define PIN_I2C_SDA    GPIO_NUM_6
#define PIN_I2C_SCL    GPIO_NUM_7

#define LCD_SPI_HOST   SPI2_HOST
#define LCD_DRAW_LINES 40

static lv_display_t *s_lv_disp;
static lv_indev_t *s_lv_touch;

static void backlight_on(void)
{
    const ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer));

    const ledc_channel_config_t channel = {
        .gpio_num = PIN_LCD_BL,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0,
        .duty = 255,
        .hpoint = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&channel));
}

static esp_err_t touch_init(void)
{
    i2c_master_bus_handle_t bus_handle = NULL;
    const i2c_master_bus_config_t i2c_bus_conf = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .sda_io_num = PIN_I2C_SDA,
        .scl_io_num = PIN_I2C_SCL,
        .i2c_port = -1,
        .flags = {
            .enable_internal_pullup = true,
        },
    };
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&i2c_bus_conf, &bus_handle), TAG, "i2c_new_master_bus");

    esp_lcd_panel_io_handle_t tp_io_handle = NULL;
    const esp_lcd_panel_io_i2c_config_t tp_io_cfg = ESP_LCD_TOUCH_IO_I2C_CST816S_CONFIG();
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_i2c(bus_handle, &tp_io_cfg, &tp_io_handle), TAG, "touch_io_i2c");

    const esp_lcd_touch_config_t tp_cfg = {
        .x_max = WAVESHARE_S3_LCD_HRES,
        .y_max = WAVESHARE_S3_LCD_VRES,
        .rst_gpio_num = PIN_TP_RST,
        .int_gpio_num = PIN_TP_INT,
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
        .flags = {
            .swap_xy = 0,
            .mirror_x = 1,
            .mirror_y = 0,
        },
    };

    esp_lcd_touch_handle_t tp = NULL;
    ESP_RETURN_ON_ERROR(esp_lcd_touch_new_i2c_cst816s(tp_io_handle, &tp_cfg, &tp), TAG, "touch_cst816s");

    const lvgl_port_touch_cfg_t touch_cfg = {
        .disp = s_lv_disp,
        .handle = tp,
    };
    s_lv_touch = lvgl_port_add_touch(&touch_cfg);
    ESP_RETURN_ON_FALSE(s_lv_touch != NULL, ESP_FAIL, TAG, "lvgl_port_add_touch failed");
    return ESP_OK;
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
        .pclk_hz = 80 * 1000 * 1000,
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
        .double_buffer = false,
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

    ESP_RETURN_ON_ERROR(touch_init(), TAG, "touch_init");

    ESP_LOGI(TAG, "Display ready %dx%d", WAVESHARE_S3_LCD_HRES, WAVESHARE_S3_LCD_VRES);
    return ESP_OK;
}

lv_display_t *waveshare_s3_lcd_display_get_lv_display(void)
{
    return s_lv_disp;
}

#else /* CONFIG_WAVESHARE_ESP32_S3_TOUCH_LCD_128 */

esp_err_t waveshare_s3_lcd_display_hw_init(void)
{
    return ESP_ERR_NOT_SUPPORTED;
}

lv_display_t *waveshare_s3_lcd_display_get_lv_display(void)
{
    return NULL;
}

#endif /* CONFIG_WAVESHARE_ESP32_S3_TOUCH_LCD_128 */
