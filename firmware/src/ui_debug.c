#include "ui_internal.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"

#include "buttons.h"
#include "encoder.h"
#include "heartbeat.h"
#include "rotary_switch.h"
#include "version.h"
#include "whb04b_codes.h"
#include "whb04b_usb.h"
#include "xpt2046_touch.h"

// ---------------------------------------------------------------------
// Screen "Debug": a scrolling, read-only list grouped into sections.
//
// The two USB sections show the raw packets in both directions. That is the
// point of the screen: the hex can be compared one to one against what the
// host logs, so a disagreement between pendant and host can be pinned to a
// side without a logic analyser.
//
// "Max loop time" is the important value: the time constant of the hardware
// watchdog is derived from it. It is a MAXIMUM since start (or since the
// last reset), never an average - an average would hide exactly the one
// long pass that matters.
// ---------------------------------------------------------------------

#define ROW_H        20
#define HEADER_ROW_H 24
#define PAD_SIDE     12

typedef enum {
    // STATUS
    ROW_USB = 0,
    ROW_HOST_AGE,
    ROW_MAX_LOOP,
    ROW_HEARTBEAT,
    ROW_FIRMWARE,
    // INPUTS
    ROW_ADC_AXIS,
    ROW_ADC_FEED,
    ROW_ENCODER,
    ROW_BUTTONS,
    ROW_TOUCH_RAW,
    // USB TO HOST
    ROW_TX_HEX,
    ROW_TX_KEY1,
    ROW_TX_KEY2,
    ROW_TX_FEED,
    ROW_TX_AXIS,
    ROW_TX_STEP,
    ROW_TX_CRC,
    // USB FROM HOST
    ROW_RX_HEX1,
    ROW_RX_HEX2,
    ROW_RX_HEX3,
    ROW_RX_SEED,
    ROW_RX_FLAGS,
    ROW_RX_X,
    ROW_RX_Y,
    ROW_RX_Z,
    ROW_RX_FEED,
    ROW_RX_SPINDLE,
    DEBUG_ROW_COUNT
} debug_row_t;

typedef struct {
    const char *section;  // non-NULL starts a new section above this row
    const char *name;
} row_spec_t;

// The columns of this table are aligned by hand. No clang-format setting
// reproduces that, so formatting is switched off for it.
// clang-format off
static const row_spec_t row_spec[DEBUG_ROW_COUNT] = {
    { "STATUS",                        "USB" },
    { NULL,                            "Last host data" },
    { NULL,                            "Max loop time" },
    { NULL,                            "Heartbeat" },
    { NULL,                            "Firmware" },

    { "INPUTS",                        "ADC axis" },
    { NULL,                            "ADC feed" },
    { NULL,                            "Encoder" },
    { NULL,                            "Buttons" },
    { NULL,                            "Touch raw" },

    { "USB TO HOST (IN REPORT)",       "Bytes" },
    { NULL,                            "Key 1" },
    { NULL,                            "Key 2" },
    { NULL,                            "Feed code" },
    { NULL,                            "Axis code" },
    { NULL,                            "Step count" },
    { NULL,                            "CRC" },

    { "USB FROM HOST (DISPLAY DATA)",  "Bytes 1-7" },
    { NULL,                            "Bytes 8-14" },
    { NULL,                            "Bytes 15-21" },
    { NULL,                            "Seed" },
    { NULL,                            "Flags" },
    { NULL,                            "X" },
    { NULL,                            "Y" },
    { NULL,                            "Z" },
    { NULL,                            "Feed rate" },
    { NULL,                            "Spindle rate" },
};
// clang-format on

static lv_obj_t *value_label[DEBUG_ROW_COUNT];

// Last text written per row. Comparing here rather than letting LVGL do it
// matters: lv_label_set_text_fmt() always invalidates, and an invalidation
// costs the transfer of a whole band over SPI.
static char value_cache[DEBUG_ROW_COUNT][28];

static void set_value(debug_row_t row, const char *text)
{
    if (strncmp(value_cache[row], text, sizeof(value_cache[row]) - 1) == 0) {
        return;
    }

    snprintf(value_cache[row], sizeof(value_cache[row]), "%s", text);
    lv_label_set_text(value_label[row], value_cache[row]);
}

static void set_value_fmt(debug_row_t row, const char *fmt, ...)
{
    char text[28];
    va_list args;

    va_start(args, fmt);
    vsnprintf(text, sizeof(text), fmt, args);
    va_end(args);

    set_value(row, text);
}

static void add_section(lv_obj_t *list, const char *title)
{
    lv_obj_t *row =
        ui_row_create(list, UI_SCREEN_W - UI_SCROLLBAR_W, HEADER_ROW_H, UI_ROW_PLAIN);

    lv_obj_t *label = lv_label_create(row);
    lv_obj_set_style_text_font(label, UI_FONT_14, 0);
    ui_apply_accent_label(label);
    lv_label_set_text(label, title);
    lv_obj_align(label, LV_ALIGN_BOTTOM_LEFT, PAD_SIDE, 0);
}

void ui_debug_create(void)
{
    lv_obj_t *screen = ui_screen_create(UI_SCREEN_DEBUG);
    ui_header_create(screen, "Debug", UI_SCREEN_MENU);

    lv_obj_t *list = ui_list_create(screen, 0, 0);
    lv_obj_set_style_pad_row(list, 0, 0);

    for (uint8_t i = 0; i < DEBUG_ROW_COUNT; i++) {
        if (row_spec[i].section != NULL) {
            add_section(list, row_spec[i].section);
        }

        lv_obj_t *row = ui_row_create(list, UI_SCREEN_W - UI_SCROLLBAR_W, ROW_H,
                                      UI_ROW_PLAIN_SEPARATED);

        lv_obj_t *name = lv_label_create(row);
        lv_obj_set_style_text_font(name, UI_FONT_14, 0);
        ui_apply_muted(name);
        lv_label_set_text(name, row_spec[i].name);
        lv_obj_align(name, LV_ALIGN_LEFT_MID, PAD_SIDE, 0);

        value_label[i] = lv_label_create(row);
        lv_obj_set_style_text_font(value_label[i], UI_FONT_NUM_TINY, 0);
        ui_apply_text(value_label[i]);
        lv_label_set_text(value_label[i], "");
        lv_obj_align(value_label[i], LV_ALIGN_RIGHT_MID, -PAD_SIDE, 0);

        value_cache[i][0] = '\0';
    }
}

static void hex_line(char *out, size_t out_len, const uint8_t *bytes, size_t count)
{
    size_t pos = 0;
    for (size_t i = 0; i < count && pos + 3 < out_len; i++) {
        pos += (size_t) snprintf(out + pos, out_len - pos, "%02X ", bytes[i]);
    }
    if (pos > 0) {
        out[pos - 1] = '\0';  // drop the trailing space
    }
}

static void update_tx(void)
{
    uint8_t report[WHB04B_IN_REPORT_LEN];

    if (!whb04b_usb_last_sent(report)) {
        for (debug_row_t r = ROW_TX_HEX; r <= ROW_TX_CRC; r++) {
            set_value(r, "--");
        }
        return;
    }

    char hex[28];
    hex_line(hex, sizeof(hex), report, sizeof(report));
    set_value(ROW_TX_HEX, hex);

    // Field order confirmed against UsbInPackage in usb.h. The two slots
    // carry RAW protocol codes, never a stored action byte, so the macro
    // flag cannot appear here - an Fn combination shows up as the plain code
    // in key 1 and "Fn" in key 2.
    set_value_fmt(ROW_TX_KEY1, "0x%02X %s", report[2], pendant_action_name(report[2]));
    set_value_fmt(ROW_TX_KEY2, "0x%02X %s", report[3], pendant_action_name(report[3]));
    set_value_fmt(ROW_TX_FEED, "0x%02X %s", report[4], ui_step_width_text(report[4]));
    set_value_fmt(ROW_TX_AXIS, "0x%02X %s", report[5], ui_axis_text(report[5]));
    set_value_fmt(ROW_TX_STEP, "%d", (int) (int8_t) report[6]);
    set_value_fmt(ROW_TX_CRC, "0x%02X", report[7]);
}

static void update_rx(void)
{
    uint8_t packet[WHB04B_OUT_PACKET_LEN];

    if (!whb04b_usb_last_received(packet)) {
        for (debug_row_t r = ROW_RX_HEX1; r <= ROW_RX_SPINDLE; r++) {
            set_value(r, "--");
        }
        return;
    }

    char hex[28];
    hex_line(hex, sizeof(hex), &packet[0], 7);
    set_value(ROW_RX_HEX1, hex);
    hex_line(hex, sizeof(hex), &packet[7], 7);
    set_value(ROW_RX_HEX2, hex);
    hex_line(hex, sizeof(hex), &packet[14], 7);
    set_value(ROW_RX_HEX3, hex);

    // Decoded from the raw bytes rather than from pendant_display_t, so the
    // two can be compared: a mismatch here points at the decoding, a
    // mismatch against the host at the transfer.
    set_value_fmt(ROW_RX_SEED, "0x%02X", packet[2]);
    set_value_fmt(ROW_RX_FLAGS, "0x%02X", packet[3]);

    static const debug_row_t coord_row[3] = { ROW_RX_X, ROW_RX_Y, ROW_RX_Z };
    for (uint8_t axis = 0; axis < 3; axis++) {
        const uint8_t *c = &packet[4 + axis * 4];
        uint16_t integer = (uint16_t) (c[0] | ((uint16_t) c[1] << 8));
        uint16_t fraction = (uint16_t) (c[2] | ((uint16_t) c[3] << 8));
        bool negative = (fraction & 0x8000u) != 0;
        fraction &= 0x7FFFu;
        set_value_fmt(coord_row[axis], "%s%u.%04u", negative ? "-" : "", integer,
                      fraction);
    }

    set_value_fmt(ROW_RX_FEED, "%u",
                  (unsigned) (packet[16] | ((uint16_t) packet[17] << 8)));
    set_value_fmt(ROW_RX_SPINDLE, "%u",
                  (unsigned) (packet[18] | ((uint16_t) packet[19] << 8)));
}

void ui_debug_update(void)
{
    const pendant_state_t *state = pendant_state();
    const pendant_display_t *display = &state->display;

    set_value(ROW_USB, state->usb_link_ok ? "connected" : "not connected");

    if (!display->valid) {
        set_value(ROW_HOST_AGE, "never");
    } else {
        uint32_t age_ms = to_ms_since_boot(get_absolute_time()) - display->last_rx_ms;
        set_value_fmt(ROW_HOST_AGE, "%u.%u s", (unsigned) (age_ms / 1000u),
                      (unsigned) ((age_ms % 1000u) / 100u));
    }

    uint32_t gap_us = heartbeat_max_loop_gap_us();
    set_value_fmt(ROW_MAX_LOOP, "%u.%u ms", (unsigned) (gap_us / 1000u),
                  (unsigned) ((gap_us % 1000u) / 100u));

    // The configured toggle interval, not a measurement: the monostable has
    // to be dimensioned against this together with the maximum loop time.
    set_value_fmt(ROW_HEARTBEAT, "%u ms", (unsigned) HEARTBEAT_INTERVAL_MS);
    set_value(ROW_FIRMWARE, CNC_PENDANT_VERSION_STR);

    set_value_fmt(ROW_ADC_AXIS, "%u " LV_SYMBOL_RIGHT " %s", rotary_axis_raw(),
                  ui_axis_text(state->axis_selected));
    set_value_fmt(ROW_ADC_FEED, "%u " LV_SYMBOL_RIGHT " %s", rotary_feed_raw(),
                  ui_step_width_text(state->feed_selected));
    set_value_fmt(ROW_ENCODER, "%ld", (long) encoder_total_detents());

    uint8_t mask = buttons_pressed_mask();
    set_value_fmt(ROW_BUTTONS, "%u%u%u%u%u%u%u%u", (mask >> 7) & 1u, (mask >> 6) & 1u,
                  (mask >> 5) & 1u, (mask >> 4) & 1u, (mask >> 3) & 1u, (mask >> 2) & 1u,
                  (mask >> 1) & 1u, mask & 1u);

    uint16_t raw_x, raw_y;
    if (xpt2046_get_last_raw(&raw_x, &raw_y)) {
        set_value_fmt(ROW_TOUCH_RAW, "%u, %u", raw_x, raw_y);
    } else {
        set_value(ROW_TOUCH_RAW, "--");
    }

    update_tx();
    update_rx();
}
