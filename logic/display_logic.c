/* SPDX-License-Identifier: MIT */

#include "sdkconfig.h"
#include "display_logic.h"

#if CONFIG_LILYGO_LILY_PI || CONFIG_WAVESHARE_ESP32_S3_TOUCH_LCD_128

#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "lvgl.h"

#include "connect_logic.h"
#include "gps_logic.h"
#include "status_logic.h"
#if CONFIG_WAVESHARE_ESP32_S3_TOUCH_LCD_128
#include "key_logic.h"
#endif

#if CONFIG_LILYGO_LILY_PI
#include "lilypi_display.h"
#define STATUS_LCD_HRES  LILYPI_LCD_HRES
#define STATUS_LCD_VRES  LILYPI_LCD_VRES
#elif CONFIG_WAVESHARE_ESP32_S3_TOUCH_LCD_128
#include "waveshare_s3_lcd_display.h"
#define STATUS_LCD_HRES  WAVESHARE_S3_LCD_HRES
#define STATUS_LCD_VRES  WAVESHARE_S3_LCD_VRES
#endif

#define TAG "DISPLAY_LOGIC"
#define UI_UPDATE_MS 500
#define DISPLAY_TASK_STACK 8192

static lv_obj_t *s_lbl_ble;
static lv_obj_t *s_lbl_gps;
static lv_obj_t *s_lbl_gps_coord;
static lv_obj_t *s_lbl_gps_detail;
static lv_obj_t *s_lbl_record;

#if CONFIG_WAVESHARE_ESP32_S3_TOUCH_LCD_128
#define STATUS_FONT_TITLE &lv_font_montserrat_20
#define STATUS_FONT_BODY  &lv_font_montserrat_14
#define STATUS_FONT_BTN   &lv_font_montserrat_20
#define STATUS_FONT_HINT  &lv_font_montserrat_14
#define STATUS_TEXT_W     180

static lv_obj_t *s_btn_action;
static lv_obj_t *s_lbl_action;
static lv_obj_t *s_lbl_guide;

static void style_status_label(lv_obj_t *label)
{
    lv_obj_set_style_text_font(label, STATUS_FONT_BODY, 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(label, STATUS_TEXT_W);
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
}

static void set_status_row(lv_obj_t *label, const char *text, uint32_t color_hex)
{
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, lv_color_hex(color_hex), 0);
}

static const char *camera_status_text(connect_state_t state)
{
    switch (state) {
    case BLE_NOT_INIT:
        return "Camera: Starting...";
    case BLE_INIT_COMPLETE:
        return "Camera: Not connected";
    case BLE_SEARCHING:
        return "Camera: Searching...";
    case BLE_CONNECTED:
        return "Camera: Pairing...";
    case PROTOCOL_CONNECTED:
        return "Camera: Connected";
    case BLE_DISCONNECTING:
        return "Camera: Disconnecting";
    default:
        return "Camera: Unknown";
    }
}

static uint32_t camera_status_color(connect_state_t state)
{
    if (state == PROTOCOL_CONNECTED) {
        return 0x40FF80;
    }
    if (state == BLE_SEARCHING || state == BLE_CONNECTED || state == BLE_DISCONNECTING) {
        return 0xFFD040;
    }
    if (state == BLE_NOT_INIT) {
        return 0x888888;
    }
    return 0xFFFFFF;
}

static void action_btn_cb(lv_event_t *e)
{
    (void)e;

    connect_state_t conn = connect_logic_get_state();
    if (conn == BLE_NOT_INIT) {
        return;
    }
    if (conn == PROTOCOL_CONNECTED) {
        key_logic_request_record();
    } else if (conn == BLE_INIT_COMPLETE) {
        key_logic_request_connect();
    }
}

static void style_action_button(bool recording)
{
    lv_color_t bg;
    lv_color_t border;
    const char *text;
    const char *guide;
    bool enabled = false;

    connect_state_t conn = connect_logic_get_state();

    if (conn == BLE_NOT_INIT) {
        bg = lv_color_hex(0x303030);
        border = lv_color_hex(0x606060);
        text = "WAIT";
        guide = "Bluetooth starting...";
    } else if (conn == BLE_SEARCHING || conn == BLE_CONNECTED || conn == BLE_DISCONNECTING) {
        bg = lv_color_hex(0x404020);
        border = lv_color_hex(0xFFD040);
        text = "CONNECTING";
        guide = (conn == BLE_DISCONNECTING) ? "Hold on..." : "Finding your Osmo...";
    } else if (conn == PROTOCOL_CONNECTED) {
        enabled = true;
        if (recording) {
            bg = lv_color_hex(0x802020);
            border = lv_color_hex(0xFF4040);
            text = "STOP REC";
            guide = "Tap to stop recording";
        } else {
            bg = lv_color_hex(0x204020);
            border = lv_color_hex(0x40FF80);
            text = "START REC";
            guide = "Tap to record (BOOT works too)";
        }
    } else {
        bg = lv_color_hex(0x203060);
        border = lv_color_hex(0x4080FF);
        text = "CONNECT";
        guide = "Tap to find your Osmo";
        enabled = true;
    }

    lv_obj_set_style_bg_color(s_btn_action, bg, LV_PART_MAIN);
    lv_obj_set_style_border_color(s_btn_action, border, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_btn_action, 2, LV_PART_MAIN);
    lv_label_set_text(s_lbl_action, text);
    lv_label_set_text(s_lbl_guide, guide);

    if (enabled) {
        lv_obj_remove_state(s_btn_action, LV_STATE_DISABLED);
        lv_obj_add_flag(s_btn_action, LV_OBJ_FLAG_CLICKABLE);
    } else {
        lv_obj_add_state(s_btn_action, LV_STATE_DISABLED);
        lv_obj_remove_flag(s_btn_action, LV_OBJ_FLAG_CLICKABLE);
    }
}
#else
#define STATUS_FONT_BODY  &lv_font_montserrat_14
#define STATUS_FONT_HINT  &lv_font_montserrat_14

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
#endif

static void update_status_labels(void)
{
    connect_state_t conn = connect_logic_get_state();
    bool gps_found = is_gps_found();
    bool gps_valid = is_current_gps_data_valid();
    bool recording = is_camera_recording();

#if CONFIG_WAVESHARE_ESP32_S3_TOUCH_LCD_128
    set_status_row(s_lbl_ble, camera_status_text(conn), camera_status_color(conn));

    if (gps_valid) {
        set_status_row(s_lbl_gps, "GPS: Fix ready", 0x40FF80);
        if (s_lbl_gps_coord != NULL) {
            char coord[64];
            snprintf(coord, sizeof(coord), "Lat: %.5f  Lng: %.5f", gps_get_latitude(), gps_get_longitude());
            set_status_row(s_lbl_gps_coord, coord, 0xFFFFFF);
        }
        if (s_lbl_gps_detail != NULL) {
            char detail[48];
            snprintf(detail, sizeof(detail), "Alt: %.0fm  Sats: %u",
                     gps_get_altitude(), (unsigned)gps_get_num_satellites());
            set_status_row(s_lbl_gps_detail, detail, 0xFFFFFF);
        }
    } else if (gps_found) {
        set_status_row(s_lbl_gps, "GPS: Waiting for fix", 0xFFD040);
        if (s_lbl_gps_coord != NULL) {
            set_status_row(s_lbl_gps_coord, "Lat: --  Lng: --", 0x888888);
        }
        if (s_lbl_gps_detail != NULL) {
            set_status_row(s_lbl_gps_detail, "Alt: --  Sats: --", 0x888888);
        }
    } else {
        set_status_row(s_lbl_gps, "GPS: No signal", 0xFF6060);
        if (s_lbl_gps_coord != NULL) {
            set_status_row(s_lbl_gps_coord, "Lat: --  Lng: --", 0x888888);
        }
        if (s_lbl_gps_detail != NULL) {
            set_status_row(s_lbl_gps_detail, "Alt: --  Sats: --", 0x888888);
        }
    }

    if (conn == PROTOCOL_CONNECTED) {
        set_status_row(s_lbl_record,
                        recording ? "Record: Recording" : "Record: Ready",
                        recording ? 0xFF4040 : 0xFFFFFF);
    } else {
        set_status_row(s_lbl_record, "Record: Connect first", 0x888888);
    }

    if (s_btn_action != NULL) {
        style_action_button(recording);
    }
#else
    lv_label_set_text(s_lbl_ble, ble_state_text(conn));
#endif

#if !CONFIG_WAVESHARE_ESP32_S3_TOUCH_LCD_128
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
#endif /* !Waveshare */
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
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), LV_PART_MAIN);

#if CONFIG_WAVESHARE_ESP32_S3_TOUCH_LCD_128
    const int y_title = 24;
    const int y_ble = 56;
    const int y_gps = 84;
    const int y_gps_coord = 106;
    const int y_gps_detail = 128;
    const int y_record = 150;

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "Osmo GPS");
    lv_obj_set_style_text_font(title, STATUS_FONT_TITLE, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, y_title);

    s_lbl_ble = lv_label_create(scr);
    style_status_label(s_lbl_ble);
    lv_obj_align(s_lbl_ble, LV_ALIGN_TOP_MID, 0, y_ble);

    s_lbl_gps = lv_label_create(scr);
    style_status_label(s_lbl_gps);
    lv_obj_align(s_lbl_gps, LV_ALIGN_TOP_MID, 0, y_gps);

    s_lbl_gps_coord = lv_label_create(scr);
    style_status_label(s_lbl_gps_coord);
    lv_obj_align(s_lbl_gps_coord, LV_ALIGN_TOP_MID, 0, y_gps_coord);

    s_lbl_gps_detail = lv_label_create(scr);
    style_status_label(s_lbl_gps_detail);
    lv_obj_align(s_lbl_gps_detail, LV_ALIGN_TOP_MID, 0, y_gps_detail);

    s_lbl_record = lv_label_create(scr);
    style_status_label(s_lbl_record);
    lv_obj_align(s_lbl_record, LV_ALIGN_TOP_MID, 0, y_record);

    s_btn_action = lv_button_create(scr);
    lv_obj_set_size(s_btn_action, 160, 48);
    lv_obj_align(s_btn_action, LV_ALIGN_BOTTOM_MID, 0, -48);
    lv_obj_set_style_radius(s_btn_action, 24, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_btn_action, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(s_btn_action, action_btn_cb, LV_EVENT_CLICKED, NULL);

    s_lbl_action = lv_label_create(s_btn_action);
    lv_obj_set_style_text_font(s_lbl_action, STATUS_FONT_BTN, 0);
    lv_obj_set_style_text_color(s_lbl_action, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(s_lbl_action);

    s_lbl_guide = lv_label_create(scr);
    lv_obj_set_style_text_font(s_lbl_guide, STATUS_FONT_HINT, 0);
    lv_obj_set_style_text_color(s_lbl_guide, lv_color_hex(0xA0A0A0), 0);
    lv_obj_set_style_text_align(s_lbl_guide, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(s_lbl_guide, STATUS_TEXT_W);
    lv_label_set_long_mode(s_lbl_guide, LV_LABEL_LONG_WRAP);
    lv_obj_align(s_lbl_guide, LV_ALIGN_BOTTOM_MID, 0, -10);
#else
    const int margin = 12;
    const int content_w = STATUS_LCD_HRES - 2 * margin;
    const int y_title = 16;
    const int y_ble = 56;
    const int y_gps = 120;
    const int y_record = 184;

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "Osmo GPS");
    lv_obj_set_style_text_font(title, STATUS_FONT_BODY, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, y_title);

    s_lbl_ble = lv_label_create(scr);
    lv_obj_set_style_text_font(s_lbl_ble, STATUS_FONT_BODY, 0);
    lv_obj_set_style_text_color(s_lbl_ble, lv_color_hex(0xE0E0E0), 0);
    lv_obj_align(s_lbl_ble, LV_ALIGN_TOP_LEFT, margin, y_ble);
    lv_label_set_long_mode(s_lbl_ble, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_lbl_ble, content_w);

    s_lbl_gps = lv_label_create(scr);
    lv_obj_set_style_text_font(s_lbl_gps, STATUS_FONT_BODY, 0);
    lv_obj_set_style_text_color(s_lbl_gps, lv_color_hex(0xE0E0E0), 0);
    lv_obj_align(s_lbl_gps, LV_ALIGN_TOP_LEFT, margin, y_gps);
    lv_label_set_long_mode(s_lbl_gps, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_lbl_gps, content_w);

    s_lbl_record = lv_label_create(scr);
    lv_obj_set_style_text_font(s_lbl_record, STATUS_FONT_BODY, 0);
    lv_obj_set_style_text_color(s_lbl_record, lv_color_hex(0xE0E0E0), 0);
    lv_obj_align(s_lbl_record, LV_ALIGN_TOP_LEFT, margin, y_record);
    lv_label_set_long_mode(s_lbl_record, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_lbl_record, content_w);

    lv_obj_t *hint = lv_label_create(scr);
    lv_label_set_text(hint, "BOOT: tap=rec hold=conn");
    lv_obj_set_style_text_font(hint, STATUS_FONT_HINT, 0);
    lv_obj_set_style_text_color(hint, lv_color_hex(0x808080), 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -10);
#endif

    update_status_labels();
}

static int display_hw_init(void)
{
#if CONFIG_LILYGO_LILY_PI
    if (lilypi_display_hw_init() != ESP_OK) {
        return -1;
    }
    if (lilypi_display_get_lv_display() == NULL) {
        return -1;
    }
#elif CONFIG_WAVESHARE_ESP32_S3_TOUCH_LCD_128
    if (waveshare_s3_lcd_display_hw_init() != ESP_OK) {
        return -1;
    }
    if (waveshare_s3_lcd_display_get_lv_display() == NULL) {
        return -1;
    }
#endif
    return 0;
}

#endif /* display boards */

int display_logic_init(void)
{
#if CONFIG_LILYGO_LILY_PI || CONFIG_WAVESHARE_ESP32_S3_TOUCH_LCD_128
    if (display_hw_init() != 0) {
        ESP_LOGE(TAG, "display hardware init failed");
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
    ESP_LOGI(TAG, "Status screen started (%dx%d)", STATUS_LCD_HRES, STATUS_LCD_VRES);
    return 0;
#else
    return 0;
#endif
}
