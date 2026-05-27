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
/* 240×240 round panel — keep text inside ~184px inscribed square (corners clip). */
#define WS_FONT_TITLE   &lv_font_montserrat_14
#define WS_FONT_BODY    &lv_font_montserrat_14
#define WS_FONT_BTN     &lv_font_montserrat_20
#define WS_TEXT_W       148
#define WS_RING_SIZE    218

typedef enum {
    WS_PAGE_MAIN = 0,
    WS_PAGE_GPS = 1,
    WS_PAGE_HELP = 2,
    WS_PAGE_COUNT = 3,
} ws_page_t;

static lv_obj_t *s_tileview;
static lv_obj_t *s_page_dots[WS_PAGE_COUNT];
static lv_obj_t *s_btn_action;
static lv_obj_t *s_lbl_action;
static lv_obj_t *s_lbl_lat;
static lv_obj_t *s_lbl_lng;
static lv_obj_t *s_lbl_alt;

static void ws_style_body_label(lv_obj_t *label)
{
    lv_obj_set_style_text_font(label, WS_FONT_BODY, 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(label, WS_TEXT_W);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
}

static void ws_tile_set_page(lv_obj_t *tile, ws_page_t page)
{
    lv_obj_set_user_data(tile, (void *)(intptr_t)page);
}

static ws_page_t ws_tile_get_page(lv_obj_t *tile)
{
    return (ws_page_t)(intptr_t)lv_obj_get_user_data(tile);
}

static void ws_update_page_dots(ws_page_t active)
{
    for (int i = 0; i < WS_PAGE_COUNT; i++) {
        uint32_t color = (i == (int)active) ? 0xFFFFFF : 0x505050;
        lv_obj_set_style_bg_color(s_page_dots[i], lv_color_hex(color), LV_PART_MAIN);
    }
}

static void ws_tileview_page_cb(lv_event_t *e)
{
    lv_obj_t *tv = lv_event_get_target(e);
    lv_obj_t *tile = lv_tileview_get_tile_active(tv);
    if (tile != NULL) {
        ws_update_page_dots(ws_tile_get_page(tile));
    }
}

static void ws_add_round_ring(lv_obj_t *scr)
{
    lv_obj_t *ring = lv_obj_create(scr);
    lv_obj_set_size(ring, WS_RING_SIZE, WS_RING_SIZE);
    lv_obj_center(ring);
    lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ring, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_color(ring, lv_color_hex(0x282828), LV_PART_MAIN);
    lv_obj_set_style_border_width(ring, 2, LV_PART_MAIN);
    lv_obj_remove_flag(ring, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
}

static void ws_add_page_dots(lv_obj_t *scr)
{
    lv_obj_t *cont = lv_obj_create(scr);
    lv_obj_set_size(cont, 48, 12);
    lv_obj_align(cont, LV_ALIGN_BOTTOM_MID, 0, -14);
    lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(cont, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(cont, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(cont, 8, LV_PART_MAIN);
    lv_obj_remove_flag(cont, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    for (int i = 0; i < WS_PAGE_COUNT; i++) {
        s_page_dots[i] = lv_obj_create(cont);
        lv_obj_set_size(s_page_dots[i], 6, 6);
        lv_obj_set_style_radius(s_page_dots[i], LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_set_style_bg_color(s_page_dots[i], lv_color_hex(0x505050), LV_PART_MAIN);
        lv_obj_set_style_border_width(s_page_dots[i], 0, LV_PART_MAIN);
        lv_obj_remove_flag(s_page_dots[i], LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    }
    ws_update_page_dots(WS_PAGE_MAIN);
    lv_obj_move_foreground(cont);
}

static void set_status_row(lv_obj_t *label, const char *text, uint32_t color_hex)
{
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, lv_color_hex(color_hex), 0);
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
        text = "CONNECTING";
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
    char row[32];

    snprintf(row, sizeof(row), "Cam  %s", camera_status_short(conn));
    set_status_row(s_lbl_ble, row, camera_status_color(conn));

    if (gps_valid) {
        snprintf(row, sizeof(row), "GPS  Fix");
        set_status_row(s_lbl_gps, row, 0x40FF80);
    } else if (gps_found) {
        set_status_row(s_lbl_gps, "GPS  Waiting", 0xFFD040);
    } else {
        set_status_row(s_lbl_gps, "GPS  No signal", 0xFF6060);
    }

    if (conn == PROTOCOL_CONNECTED) {
        snprintf(row, sizeof(row), "Rec  %s", recording ? "ON" : "Ready");
        set_status_row(s_lbl_record, row, recording ? 0xFF4040 : 0xFFFFFF);
    } else {
        set_status_row(s_lbl_record, "Rec  —", 0x888888);
    }

    if (gps_valid) {
        char line[40];
        snprintf(line, sizeof(line), "%.5f", gps_get_latitude());
        set_status_row(s_lbl_lat, line, 0xFFFFFF);
        snprintf(line, sizeof(line), "%.5f", gps_get_longitude());
        set_status_row(s_lbl_lng, line, 0xFFFFFF);
        snprintf(line, sizeof(line), "%.0f m  /  %u sats",
                 gps_get_altitude(), (unsigned)gps_get_num_satellites());
        set_status_row(s_lbl_alt, line, 0xC0C0C0);
    } else {
        set_status_row(s_lbl_lat, "—", 0x888888);
        set_status_row(s_lbl_lng, "—", 0x888888);
        set_status_row(s_lbl_alt, "—", 0x888888);
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
    ws_add_round_ring(scr);

    s_tileview = lv_tileview_create(scr);
    lv_obj_set_size(s_tileview, STATUS_LCD_HRES, STATUS_LCD_VRES);
    lv_obj_center(s_tileview);
    lv_obj_set_style_bg_opa(s_tileview, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_tileview, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(s_tileview, ws_tileview_page_cb, LV_EVENT_VALUE_CHANGED, NULL);

    /* Main: compact status + action (swipe right → GPS, down → help). */
    lv_obj_t *tile_main = lv_tileview_add_tile(s_tileview, 0, 0, LV_DIR_RIGHT | LV_DIR_BOTTOM);
    ws_tile_set_page(tile_main, WS_PAGE_MAIN);
    lv_obj_set_style_bg_opa(tile_main, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(tile_main, 0, LV_PART_MAIN);
    lv_obj_remove_flag(tile_main, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title_main = lv_label_create(tile_main);
    lv_label_set_text(title_main, "Osmo GPS");
    lv_obj_set_style_text_font(title_main, WS_FONT_TITLE, 0);
    lv_obj_set_style_text_color(title_main, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(title_main, LV_ALIGN_TOP_MID, 0, 52);

    s_lbl_ble = lv_label_create(tile_main);
    ws_style_body_label(s_lbl_ble);
    lv_obj_align(s_lbl_ble, LV_ALIGN_TOP_MID, 0, 78);

    s_lbl_gps = lv_label_create(tile_main);
    ws_style_body_label(s_lbl_gps);
    lv_obj_align(s_lbl_gps, LV_ALIGN_TOP_MID, 0, 100);

    s_lbl_record = lv_label_create(tile_main);
    ws_style_body_label(s_lbl_record);
    lv_obj_align(s_lbl_record, LV_ALIGN_TOP_MID, 0, 122);

    s_btn_action = lv_button_create(tile_main);
    lv_obj_set_size(s_btn_action, 136, 44);
    lv_obj_align(s_btn_action, LV_ALIGN_CENTER, 0, 38);
    lv_obj_set_style_radius(s_btn_action, 22, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_btn_action, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(s_btn_action, action_btn_cb, LV_EVENT_CLICKED, NULL);

    s_lbl_action = lv_label_create(s_btn_action);
    lv_obj_set_style_text_font(s_lbl_action, WS_FONT_BTN, 0);
    lv_obj_set_style_text_color(s_lbl_action, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(s_lbl_action);

    lv_obj_t *hint_main = lv_label_create(tile_main);
    lv_label_set_text(hint_main, "Swipe right for GPS");
    lv_obj_set_style_text_font(hint_main, WS_FONT_BODY, 0);
    lv_obj_set_style_text_color(hint_main, lv_color_hex(0x606060), 0);
    lv_obj_align(hint_main, LV_ALIGN_BOTTOM_MID, 0, -32);

    /* GPS details (swipe left to main, down to help). */
    lv_obj_t *tile_gps = lv_tileview_add_tile(s_tileview, 1, 0, LV_DIR_LEFT | LV_DIR_BOTTOM);
    ws_tile_set_page(tile_gps, WS_PAGE_GPS);
    lv_obj_set_style_bg_opa(tile_gps, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(tile_gps, 0, LV_PART_MAIN);
    lv_obj_remove_flag(tile_gps, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title_gps = lv_label_create(tile_gps);
    lv_label_set_text(title_gps, "GPS");
    lv_obj_set_style_text_font(title_gps, WS_FONT_TITLE, 0);
    lv_obj_set_style_text_color(title_gps, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(title_gps, LV_ALIGN_TOP_MID, 0, 52);

    lv_obj_t *lbl_lat_hdr = lv_label_create(tile_gps);
    lv_label_set_text(lbl_lat_hdr, "Latitude");
    lv_obj_set_style_text_font(lbl_lat_hdr, WS_FONT_BODY, 0);
    lv_obj_set_style_text_color(lbl_lat_hdr, lv_color_hex(0x808080), 0);
    lv_obj_align(lbl_lat_hdr, LV_ALIGN_TOP_MID, 0, 76);

    s_lbl_lat = lv_label_create(tile_gps);
    ws_style_body_label(s_lbl_lat);
    lv_obj_align(s_lbl_lat, LV_ALIGN_TOP_MID, 0, 94);

    lv_obj_t *lbl_lng_hdr = lv_label_create(tile_gps);
    lv_label_set_text(lbl_lng_hdr, "Longitude");
    lv_obj_set_style_text_font(lbl_lng_hdr, WS_FONT_BODY, 0);
    lv_obj_set_style_text_color(lbl_lng_hdr, lv_color_hex(0x808080), 0);
    lv_obj_align(lbl_lng_hdr, LV_ALIGN_TOP_MID, 0, 118);

    s_lbl_lng = lv_label_create(tile_gps);
    ws_style_body_label(s_lbl_lng);
    lv_obj_align(s_lbl_lng, LV_ALIGN_TOP_MID, 0, 136);

    lv_obj_t *lbl_alt_hdr = lv_label_create(tile_gps);
    lv_label_set_text(lbl_alt_hdr, "Alt / Sats");
    lv_obj_set_style_text_font(lbl_alt_hdr, WS_FONT_BODY, 0);
    lv_obj_set_style_text_color(lbl_alt_hdr, lv_color_hex(0x808080), 0);
    lv_obj_align(lbl_alt_hdr, LV_ALIGN_TOP_MID, 0, 158);

    s_lbl_alt = lv_label_create(tile_gps);
    ws_style_body_label(s_lbl_alt);
    lv_obj_align(s_lbl_alt, LV_ALIGN_TOP_MID, 0, 176);

    /* Help / controls (swipe up to main). */
    lv_obj_t *tile_help = lv_tileview_add_tile(s_tileview, 0, 1, LV_DIR_TOP);
    ws_tile_set_page(tile_help, WS_PAGE_HELP);
    lv_obj_set_style_bg_opa(tile_help, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(tile_help, 0, LV_PART_MAIN);
    lv_obj_remove_flag(tile_help, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title_help = lv_label_create(tile_help);
    lv_label_set_text(title_help, "Controls");
    lv_obj_set_style_text_font(title_help, WS_FONT_TITLE, 0);
    lv_obj_set_style_text_color(title_help, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(title_help, LV_ALIGN_TOP_MID, 0, 52);

    lv_obj_t *help_body = lv_label_create(tile_help);
    lv_label_set_text(help_body,
                      "Button: connect / record\n"
                      "BOOT tap: record\n"
                      "BOOT 1s: cancel connect\n"
                      "BOOT 3s: disconnect\n\n"
                      "Swipe right: GPS\n"
                      "Swipe up: main");
    ws_style_body_label(help_body);
    lv_obj_set_style_text_color(help_body, lv_color_hex(0xC0C0C0), 0);
    lv_obj_align(help_body, LV_ALIGN_TOP_MID, 0, 76);

    ws_add_page_dots(scr);
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
