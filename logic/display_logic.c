/* SPDX-License-Identifier: MIT */

#include "sdkconfig.h"
#include "display_logic.h"

#if CONFIG_LILYGO_LILY_PI || CONFIG_WAVESHARE_ESP32_S3_TOUCH_LCD_128

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
static lv_obj_t *s_lbl_record;

#if CONFIG_WAVESHARE_ESP32_S3_TOUCH_LCD_128
#define STATUS_FONT_BODY  &lv_font_montserrat_20
#define STATUS_FONT_HINT  &lv_font_montserrat_14
#define STATUS_TEXT_W     190
static volatile bool s_touch_seen;
static lv_obj_t *s_lbl_touch;
static lv_obj_t *s_btn_action;
static lv_obj_t *s_lbl_action;
static lv_obj_t *s_lbl_guide;

static const char *ble_state_text(connect_state_t state)
{
    switch (state) {
    case BLE_NOT_INIT:
        return "BLE  --";
    case BLE_INIT_COMPLETE:
        return "BLE  idle";
    case BLE_SEARCHING:
        return "BLE  scan";
    case BLE_CONNECTED:
        return "BLE  link";
    case PROTOCOL_CONNECTED:
        return "CAM  ok";
    case BLE_DISCONNECTING:
        return "BLE  ...";
    default:
        return "BLE  ?";
    }
}

static void style_status_label(lv_obj_t *label)
{
    lv_obj_set_style_text_font(label, STATUS_FONT_BODY, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(label, STATUS_TEXT_W);
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
}

static void mark_touch_seen(void)
{
    s_touch_seen = true;
    if (s_lbl_touch != NULL) {
        lv_label_set_text(s_lbl_touch, "touch OK");
        lv_obj_set_style_text_color(s_lbl_touch, lv_color_hex(0x40FF80), 0);
    }
}

static void action_btn_cb(lv_event_t *e)
{
    (void)e;
    mark_touch_seen();

    connect_state_t conn = connect_logic_get_state();
    if (conn == BLE_NOT_INIT) {
        ESP_LOGW(TAG, "Connect ignored: BLE still initializing");
        return;
    }
    if (conn == PROTOCOL_CONNECTED) {
        key_logic_request_record();
    } else if (conn != BLE_SEARCHING && conn != BLE_DISCONNECTING) {
        key_logic_request_connect();
    }
}

static void style_action_button(bool enabled, bool recording)
{
    lv_color_t bg;
    lv_color_t border;
    const char *text;
    const char *guide;

    connect_state_t conn = connect_logic_get_state();

    if (conn == BLE_NOT_INIT) {
        bg = lv_color_hex(0x303030);
        border = lv_color_hex(0x606060);
        text = "WAIT";
        guide = "BLE starting...";
        enabled = false;
    } else if (conn == BLE_SEARCHING || conn == BLE_DISCONNECTING) {
        bg = lv_color_hex(0x303030);
        border = lv_color_hex(0x606060);
        text = (conn == BLE_SEARCHING) ? "CONNECTING" : "DISCONNECT";
        guide = "Please wait...";
        enabled = false;
    } else if (conn == PROTOCOL_CONNECTED) {
        if (recording) {
            bg = lv_color_hex(0x802020);
            border = lv_color_hex(0xFF4040);
            text = "STOP REC";
            guide = "Tap to stop recording";
        } else {
            bg = lv_color_hex(0x204020);
            border = lv_color_hex(0x40FF80);
            text = "START REC";
            guide = "Tap to start recording";
        }
        enabled = true;
    } else {
        bg = lv_color_hex(0x203060);
        border = lv_color_hex(0x4080FF);
        text = "CONNECT";
        guide = "Tap to find camera";
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

static void touch_probe_cb(lv_event_t *e)
{
    (void)e;
    mark_touch_seen();
}

static void update_status_labels(void)
{
    connect_state_t conn = connect_logic_get_state();
    bool gps_found = is_gps_found();
    bool gps_valid = is_current_gps_data_valid();
    bool recording = is_camera_recording();

    lv_label_set_text(s_lbl_ble, ble_state_text(conn));

#if CONFIG_WAVESHARE_ESP32_S3_TOUCH_LCD_128
    if (conn == PROTOCOL_CONNECTED) {
        lv_obj_set_style_text_color(s_lbl_ble, lv_color_hex(0x40FF80), 0);
    } else if (conn == BLE_CONNECTED || conn == BLE_SEARCHING) {
        lv_obj_set_style_text_color(s_lbl_ble, lv_color_hex(0xFFD040), 0);
    } else {
        lv_obj_set_style_text_color(s_lbl_ble, lv_color_hex(0xFFFFFF), 0);
    }

    if (gps_valid) {
        lv_label_set_text(s_lbl_gps, "GPS  fix");
        lv_obj_set_style_text_color(s_lbl_gps, lv_color_hex(0x40FF80), 0);
    } else if (gps_found) {
        lv_label_set_text(s_lbl_gps, "GPS  wait");
        lv_obj_set_style_text_color(s_lbl_gps, lv_color_hex(0xFFD040), 0);
    } else {
        lv_label_set_text(s_lbl_gps, "GPS  ---");
        lv_obj_set_style_text_color(s_lbl_gps, lv_color_hex(0xFF6060), 0);
    }

    if (conn == PROTOCOL_CONNECTED) {
        lv_label_set_text(s_lbl_record, recording ? "REC  ON" : "REC  off");
        lv_obj_set_style_text_color(s_lbl_record,
                                     recording ? lv_color_hex(0xFF4040) : lv_color_hex(0xFFFFFF),
                                     0);
    } else {
        lv_label_set_text(s_lbl_record, "REC  n/a");
        lv_obj_set_style_text_color(s_lbl_record, lv_color_hex(0x888888), 0);
    }

    if (s_touch_seen && s_lbl_touch != NULL) {
        lv_label_set_text(s_lbl_touch, "touch OK");
        lv_obj_set_style_text_color(s_lbl_touch, lv_color_hex(0x40FF80), 0);
    }

    if (s_btn_action != NULL) {
        style_action_button(true, recording);
    }
#else
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
#endif
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
    const int y_title = 22;
    const int y_ble = 58;
    const int y_gps = 88;
    const int y_record = 118;

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "OSMO GPS");
    lv_obj_set_style_text_font(title, STATUS_FONT_BODY, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, y_title);

    s_lbl_touch = lv_label_create(scr);
    lv_label_set_text(s_lbl_touch, "touch --");
    lv_obj_set_style_text_font(s_lbl_touch, STATUS_FONT_HINT, 0);
    lv_obj_set_style_text_color(s_lbl_touch, lv_color_hex(0x808080), 0);
    lv_obj_align(s_lbl_touch, LV_ALIGN_TOP_MID, 0, y_title + 24);

    s_lbl_ble = lv_label_create(scr);
    style_status_label(s_lbl_ble);
    lv_obj_align(s_lbl_ble, LV_ALIGN_TOP_MID, 0, y_ble);

    s_lbl_gps = lv_label_create(scr);
    style_status_label(s_lbl_gps);
    lv_obj_align(s_lbl_gps, LV_ALIGN_TOP_MID, 0, y_gps);

    s_lbl_record = lv_label_create(scr);
    style_status_label(s_lbl_record);
    lv_obj_align(s_lbl_record, LV_ALIGN_TOP_MID, 0, y_record);

    s_btn_action = lv_button_create(scr);
    lv_obj_set_size(s_btn_action, 168, 52);
    lv_obj_align(s_btn_action, LV_ALIGN_BOTTOM_MID, 0, -44);
    lv_obj_set_style_radius(s_btn_action, 26, LV_PART_MAIN);
    lv_obj_add_event_cb(s_btn_action, action_btn_cb, LV_EVENT_CLICKED, NULL);

    s_lbl_action = lv_label_create(s_btn_action);
    lv_obj_set_style_text_font(s_lbl_action, STATUS_FONT_BODY, 0);
    lv_obj_set_style_text_color(s_lbl_action, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(s_lbl_action);

    s_lbl_guide = lv_label_create(scr);
    lv_obj_set_style_text_font(s_lbl_guide, STATUS_FONT_HINT, 0);
    lv_obj_set_style_text_color(s_lbl_guide, lv_color_hex(0xB0B0B0), 0);
    lv_obj_set_style_text_align(s_lbl_guide, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(s_lbl_guide, STATUS_TEXT_W);
    lv_obj_align(s_lbl_guide, LV_ALIGN_BOTTOM_MID, 0, -12);

    lv_obj_add_flag(scr, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(scr, touch_probe_cb, LV_EVENT_CLICKED, NULL);
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
