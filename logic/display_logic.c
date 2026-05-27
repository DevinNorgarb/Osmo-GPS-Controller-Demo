/* SPDX-License-Identifier: MIT */

#include "sdkconfig.h"
#include "display_logic.h"

#if CONFIG_LILYGO_LILY_PI || CONFIG_WAVESHARE_ESP32_S3_TOUCH_LCD_128

#include <stdio.h>
#include <stdint.h>

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
/* 240×240 round panel — content in ~188px column; screen clips to circle. */
#define WS_FONT_TITLE   &lv_font_montserrat_20
#define WS_FONT_ROW     &lv_font_montserrat_18
#define WS_FONT_DETAIL  &lv_font_montserrat_14
#define WS_FONT_BTN     &lv_font_montserrat_22
#define WS_COL_W        188
#define WS_ROW_H        36

typedef struct {
    lv_obj_t *row;
    lv_obj_t *dot;
    lv_obj_t *value;
} ws_status_row_t;

static ws_status_row_t s_row_cam;
static ws_status_row_t s_row_gps;
static ws_status_row_t s_row_rec;
static lv_obj_t *s_btn_action;
static lv_obj_t *s_lbl_action;
static lv_obj_t *s_lbl_gps_detail;

static void ws_style_row_container(lv_obj_t *row)
{
    lv_obj_set_size(row, WS_COL_W, WS_ROW_H);
    lv_obj_set_style_radius(row, 10, LV_PART_MAIN);
    lv_obj_set_style_bg_color(row, lv_color_hex(0x161616), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(row, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_left(row, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_right(row, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_top(row, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(row, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, 8, LV_PART_MAIN);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
}

static void ws_status_row_create(ws_status_row_t *out, lv_obj_t *parent, const char *title)
{
    out->row = lv_obj_create(parent);
    ws_style_row_container(out->row);

    out->dot = lv_obj_create(out->row);
    lv_obj_set_size(out->dot, 10, 10);
    lv_obj_set_style_radius(out->dot, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(out->dot, lv_color_hex(0x606060), LV_PART_MAIN);
    lv_obj_set_style_border_width(out->dot, 0, LV_PART_MAIN);
    lv_obj_remove_flag(out->dot, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *lbl_title = lv_label_create(out->row);
    lv_label_set_text(lbl_title, title);
    lv_obj_set_style_text_font(lbl_title, WS_FONT_ROW, 0);
    lv_obj_set_style_text_color(lbl_title, lv_color_hex(0xA0A0A0), 0);
    lv_obj_set_flex_grow(lbl_title, 1);

    out->value = lv_label_create(out->row);
    lv_obj_set_style_text_font(out->value, WS_FONT_ROW, 0);
    lv_obj_set_style_text_color(out->value, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_align(out->value, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_text(out->value, "—");
}

static void ws_status_row_set(ws_status_row_t *row, const char *value, uint32_t color_hex)
{
    lv_label_set_text(row->value, value);
    lv_color_t c = lv_color_hex(color_hex);
    lv_obj_set_style_text_color(row->value, c, 0);
    lv_obj_set_style_bg_color(row->dot, c, LV_PART_MAIN);
}

static const char *camera_status_short(connect_state_t state)
{
    switch (state) {
    case BLE_NOT_INIT:
        return "Starting";
    case BLE_INIT_COMPLETE:
        return "Offline";
    case BLE_SEARCHING:
        return "Searching";
    case BLE_CONNECTED:
        return "Pairing";
    case PROTOCOL_CONNECTED:
        return "Connected";
    case BLE_DISCONNECTING:
        return "Disconnect";
    default:
        return "?";
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
    bool enabled = false;

    connect_state_t conn = connect_logic_get_state();

    if (conn == BLE_NOT_INIT) {
        bg = lv_color_hex(0x303030);
        border = lv_color_hex(0x606060);
        text = "WAIT";
    } else if (conn == BLE_SEARCHING || conn == BLE_CONNECTED || conn == BLE_DISCONNECTING) {
        bg = lv_color_hex(0x404020);
        border = lv_color_hex(0xFFD040);
        text = "BUSY";
    } else if (conn == PROTOCOL_CONNECTED) {
        enabled = true;
        if (recording) {
            bg = lv_color_hex(0x802020);
            border = lv_color_hex(0xFF4040);
            text = "STOP REC";
        } else {
            bg = lv_color_hex(0x204020);
            border = lv_color_hex(0x40FF80);
            text = "START REC";
        }
    } else {
        bg = lv_color_hex(0x203060);
        border = lv_color_hex(0x4080FF);
        text = "CONNECT";
        enabled = true;
    }

    lv_obj_set_style_bg_color(s_btn_action, bg, LV_PART_MAIN);
    lv_obj_set_style_border_color(s_btn_action, border, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_btn_action, 2, LV_PART_MAIN);
    lv_label_set_text(s_lbl_action, text);

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
    ws_status_row_set(&s_row_cam, camera_status_short(conn), camera_status_color(conn));

    if (gps_valid) {
        ws_status_row_set(&s_row_gps, "Fix", 0x40FF80);
    } else if (gps_found) {
        ws_status_row_set(&s_row_gps, "Waiting", 0xFFD040);
    } else {
        ws_status_row_set(&s_row_gps, "No signal", 0xFF6060);
    }

    if (conn == PROTOCOL_CONNECTED) {
        ws_status_row_set(&s_row_rec, recording ? "Recording" : "Ready",
                          recording ? 0xFF5050 : 0xFFFFFF);
    } else {
        ws_status_row_set(&s_row_rec, "—", 0x606060);
    }

    if (gps_valid) {
        char detail[80];
        snprintf(detail, sizeof(detail), "%.5f, %.5f\n%.0f m  ·  %u sats",
                 gps_get_latitude(), gps_get_longitude(),
                 gps_get_altitude(), (unsigned)gps_get_num_satellites());
        lv_label_set_text(s_lbl_gps_detail, detail);
        lv_obj_remove_flag(s_lbl_gps_detail, LV_OBJ_FLAG_HIDDEN);
    } else {
        /* Show why GPS is “waiting”: data missing vs no fix yet. */
        char detail[96];
        uint32_t ms_ago = gps_get_last_nmea_ms_ago();
        if (ms_ago == UINT32_MAX) {
            snprintf(detail, sizeof(detail), "No NMEA yet\n(check wiring / baud)");
        } else {
            snprintf(detail, sizeof(detail), "NMEA seen %ums ago\nwaiting for fix", (unsigned)ms_ago);
        }
        lv_label_set_text(s_lbl_gps_detail, detail);
        lv_obj_remove_flag(s_lbl_gps_detail, LV_OBJ_FLAG_HIDDEN);
    }

    style_action_button(recording);
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
    lv_obj_set_style_radius(scr, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_clip_corner(scr, true, LV_PART_MAIN);

    lv_obj_t *col = lv_obj_create(scr);
    lv_obj_set_size(col, WS_COL_W, 212);
    lv_obj_center(col);
    lv_obj_set_style_bg_opa(col, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(col, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(col, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_row(col, 8, LV_PART_MAIN);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_remove_flag(col, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(col);
    lv_label_set_text(title, "Osmo GPS");
    lv_obj_set_style_text_font(title, WS_FONT_TITLE, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xE8E8E8), 0);
    lv_obj_set_style_pad_bottom(title, 4, 0);

    ws_status_row_create(&s_row_cam, col, "Camera");
    ws_status_row_create(&s_row_gps, col, "GPS");
    ws_status_row_create(&s_row_rec, col, "Record");

    /* Legacy pointers for non-Waveshare update path — unused on Waveshare. */
    s_lbl_ble = s_row_cam.value;
    s_lbl_gps = s_row_gps.value;
    s_lbl_record = s_row_rec.value;

    s_btn_action = lv_button_create(col);
    lv_obj_set_size(s_btn_action, WS_COL_W, 50);
    lv_obj_set_style_radius(s_btn_action, 25, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_btn_action, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(s_btn_action, 8, LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(s_btn_action, LV_OPA_30, LV_PART_MAIN);
    lv_obj_add_event_cb(s_btn_action, action_btn_cb, LV_EVENT_CLICKED, NULL);

    s_lbl_action = lv_label_create(s_btn_action);
    lv_obj_set_style_text_font(s_lbl_action, WS_FONT_BTN, 0);
    lv_obj_set_style_text_color(s_lbl_action, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(s_lbl_action);

    s_lbl_gps_detail = lv_label_create(col);
    lv_obj_set_style_text_font(s_lbl_gps_detail, WS_FONT_DETAIL, 0);
    lv_obj_set_style_text_color(s_lbl_gps_detail, lv_color_hex(0x909090), 0);
    lv_obj_set_style_text_align(s_lbl_gps_detail, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(s_lbl_gps_detail, WS_COL_W);
    lv_label_set_long_mode(s_lbl_gps_detail, LV_LABEL_LONG_WRAP);
    lv_obj_add_flag(s_lbl_gps_detail, LV_OBJ_FLAG_HIDDEN);
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
