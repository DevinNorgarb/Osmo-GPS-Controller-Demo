/* SPDX-License-Identifier: MIT */

#include "sdkconfig.h"
#include "display_logic.h"

#if CONFIG_LILYGO_LILY_PI

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "lilypi_display.h"
#include "lvgl.h"

#include "connect_logic.h"
#include "gps_logic.h"
#include "status_logic.h"

#define TAG "DISPLAY_LOGIC"
#define UI_UPDATE_MS 500
#define DISPLAY_TASK_STACK 8192

static lv_obj_t *s_lbl_ble;
static lv_obj_t *s_lbl_gps;
static lv_obj_t *s_lbl_record;

static const char *ble_state_text(connect_state_t state)
{
    switch (state) {
    case BLE_NOT_INIT:
        return "BLE: Not initialized";
    case BLE_INIT_COMPLETE:
        return "BLE: Ready (long-press BOOT to connect)";
    case BLE_SEARCHING:
        return "BLE: Searching...";
    case BLE_CONNECTED:
        return "BLE: Connected";
    case PROTOCOL_CONNECTED:
        return "Camera: Protocol connected";
    case BLE_DISCONNECTING:
        return "BLE: Disconnecting...";
    default:
        return "BLE: Unknown";
    }
}

static void update_status_labels(void)
{
    connect_state_t conn = connect_logic_get_state();
    bool gps_found = is_gps_found();
    bool gps_valid = is_current_gps_data_valid();
    bool recording = is_camera_recording();

    lv_label_set_text(s_lbl_ble, ble_state_text(conn));

    if (gps_valid) {
        lv_label_set_text(s_lbl_gps, "GPS: Valid fix (RMC+GGA)");
    } else if (gps_found) {
        lv_label_set_text(s_lbl_gps, "GPS: NMEA received (no fix)");
    } else {
        lv_label_set_text(s_lbl_gps, "GPS: No data / invalid");
    }

    if (conn == PROTOCOL_CONNECTED) {
        lv_label_set_text(s_lbl_record, recording ? "Recording: ON" : "Recording: OFF");
    } else {
        lv_label_set_text(s_lbl_record, "Recording: N/A");
    }
}

static void display_ui_task(void *arg)
{
    (void)arg;
    while (1) {
        if (lvgl_port_lock(0)) {
            update_status_labels();
            lvgl_port_unlock();
        }
        vTaskDelay(pdMS_TO_TICKS(UI_UPDATE_MS));
    }
}

static void create_status_screen(void)
{
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x101820), LV_PART_MAIN);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "Osmo GPS Controller");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 16);

    s_lbl_ble = lv_label_create(scr);
    lv_obj_set_style_text_font(s_lbl_ble, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_lbl_ble, lv_color_hex(0xE0E0E0), 0);
    lv_obj_align(s_lbl_ble, LV_ALIGN_TOP_LEFT, 12, 56);
    lv_label_set_long_mode(s_lbl_ble, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_lbl_ble, LILYPI_LCD_HRES - 24);

    s_lbl_gps = lv_label_create(scr);
    lv_obj_set_style_text_font(s_lbl_gps, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_lbl_gps, lv_color_hex(0xE0E0E0), 0);
    lv_obj_align(s_lbl_gps, LV_ALIGN_TOP_LEFT, 12, 120);
    lv_label_set_long_mode(s_lbl_gps, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_lbl_gps, LILYPI_LCD_HRES - 24);

    s_lbl_record = lv_label_create(scr);
    lv_obj_set_style_text_font(s_lbl_record, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_lbl_record, lv_color_hex(0xE0E0E0), 0);
    lv_obj_align(s_lbl_record, LV_ALIGN_TOP_LEFT, 12, 184);

    lv_obj_t *hint = lv_label_create(scr);
    lv_label_set_text(hint, "BOOT: tap=record  hold=connect");
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(hint, lv_color_hex(0x808080), 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -12);

    update_status_labels();
}

#endif /* CONFIG_LILYGO_LILY_PI */

int display_logic_init(void)
{
#if CONFIG_LILYGO_LILY_PI
    if (lilypi_display_hw_init() != ESP_OK) {
        ESP_LOGE(TAG, "display hardware init failed");
        return -1;
    }
    if (lilypi_display_get_lv_display() == NULL) {
        ESP_LOGE(TAG, "no LVGL display");
        return -1;
    }

    if (lvgl_port_lock(0)) {
        create_status_screen();
        lvgl_port_unlock();
    }

    BaseType_t ok = xTaskCreate(display_ui_task, "display_ui", DISPLAY_TASK_STACK, NULL, 2, NULL);
    if (ok != pdPASS) {
        ESP_LOGE(TAG, "display_ui task create failed");
        return -1;
    }
    ESP_LOGI(TAG, "Status screen started");
    return 0;
#else
    return 0;
#endif
}
