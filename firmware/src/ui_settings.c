#include "ui_internal.h"

// ---------------------------------------------------------------------
// Screen "Settings": one scrolling list of segmented choices.
//
// Each row is a muted label with a row of equally wide fields underneath,
// 48 px high so every one of them is a proper touch target. The selected
// field carries the accent border.
//
// Changes take effect immediately (theme and rotation visibly so). They are
// written to flash when the screen is LEFT, not on every tap - a flash erase
// freezes the main loop and with it the safety heartbeat, so it must happen
// as rarely as possible and never while the machine is being operated. See
// ui_goto().
// ---------------------------------------------------------------------

#define PAD_LEFT     8
#define PAD_RIGHT    4
#define ROW_GAP      14
#define SEG_H        UI_MIN_TOUCH_PX
#define SEG_GAP      6
#define LABEL_H      20
#define MAX_SEGMENTS 4
#define ROW_PAD      12

#define CONTENT_W (UI_SCREEN_W - PAD_LEFT - PAD_RIGHT - UI_SCROLLBAR_W)

typedef enum {
    SET_THEME = 0,
    SET_AUTO_RETURN,
    SET_HANDWHEEL,
    SET_UNIT,
    SET_ROTATE,
    SETTING_COUNT
} setting_id_t;

typedef struct {
    const char *label;
    const char *option[MAX_SEGMENTS];
    uint8_t count;
} setting_spec_t;

static const setting_spec_t setting_spec[SETTING_COUNT] = {
    [SET_THEME] = { "Color theme", { "Dark", "Light" }, 2 },
    [SET_AUTO_RETURN] = { "Return to main screen after",
                          { "Off", "15 s", "30 s", "60 s" },
                          4 },
    [SET_HANDWHEEL] = { "Handwheel direction", { "Normal", "Reversed" }, 2 },
    [SET_UNIT] = { "Unit label", { "mm", "inch" }, 2 },
    [SET_ROTATE] = { "Rotate display", { "0\xC2\xB0", "180\xC2\xB0" }, 2 },
};

static lv_obj_t *segment[SETTING_COUNT][MAX_SEGMENTS];

// The settings struct has one field per row, so reading and writing go
// through a switch rather than an offset table - fewer places to get wrong
// than a table of pointers into a packed struct.
static uint8_t setting_get(setting_id_t id)
{
    const pendant_settings_t *s = pendant_settings();

    switch (id) {
    case SET_THEME:
        return s->theme;
    case SET_AUTO_RETURN:
        return s->auto_return_index;
    case SET_HANDWHEEL:
        return s->handwheel_reversed;
    case SET_UNIT:
        return s->unit_inch;
    case SET_ROTATE:
        return s->rotate_180;
    default:
        return 0;
    }
}

static void setting_set(setting_id_t id, uint8_t value)
{
    pendant_settings_t *s = pendant_settings();

    switch (id) {
    case SET_THEME:
        s->theme = value;
        break;
    case SET_AUTO_RETURN:
        s->auto_return_index = value;
        break;
    case SET_HANDWHEEL:
        s->handwheel_reversed = value;
        break;
    case SET_UNIT:
        s->unit_inch = value;
        break;
    case SET_ROTATE:
        s->rotate_180 = value;
        break;
    default:
        break;
    }
}

static void mark_selection(setting_id_t id)
{
    uint8_t selected = setting_get(id);

    for (uint8_t i = 0; i < setting_spec[id].count; i++) {
        lv_obj_t *obj = segment[id][i];
        // The panel styling stays in place underneath; only the selection
        // marker is added or taken away. Overriding colours by hand instead
        // would survive a theme change with the old colour baked in.
        if (i == selected) {
            ui_apply_selected(obj);
        } else {
            ui_clear_selected(obj);
        }
    }
}

static void segment_event_cb(lv_event_t *e)
{
    uint16_t packed = (uint16_t) (uintptr_t) lv_event_get_user_data(e);
    setting_id_t id = (setting_id_t) (packed >> 8);
    uint8_t value = (uint8_t) (packed & 0xFF);

    if (id >= SETTING_COUNT || value >= setting_spec[id].count) {
        return;
    }
    if (setting_get(id) == value) {
        return;
    }

    setting_set(id, value);

    // Theme and rotation have to be visible at once, so everything is
    // re-applied rather than special-cased per row.
    pendant_settings_apply();
    mark_selection(id);
}

static void reset_event_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    ui_goto(UI_SCREEN_RESET);
}

static void calibration_event_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    ui_goto(UI_SCREEN_CALIBRATION);
}

// A full-width row with a label on the left and a chevron on the right -
// the shape of something that opens another page, as opposed to the
// segmented rows above which change a value in place.
static void add_link_row(lv_obj_t *list, const char *text, lv_event_cb_t cb)
{
    lv_obj_t *row = lv_btn_create(list);
    ui_apply_panel(row);
    lv_obj_set_size(row, CONTENT_W, UI_MIN_TOUCH_PX);
    lv_obj_set_style_pad_hor(row, ROW_PAD, 0);
    lv_obj_add_event_cb(row, cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label = lv_label_create(row);
    lv_obj_set_style_text_font(label, UI_FONT_18, 0);
    ui_apply_text(label);
    lv_label_set_text(label, text);
    lv_obj_align(label, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t *chevron = lv_label_create(row);
    lv_obj_set_style_text_font(chevron, UI_FONT_20, 0);
    ui_apply_muted(chevron);
    lv_label_set_text(chevron, LV_SYMBOL_RIGHT);
    lv_obj_align(chevron, LV_ALIGN_RIGHT_MID, 0, 0);
}

static void add_setting_row(lv_obj_t *list, setting_id_t id)
{
    const setting_spec_t *spec = &setting_spec[id];

    lv_obj_t *row = ui_row_create(list, CONTENT_W, LABEL_H + SEG_H, UI_ROW_PLAIN);

    lv_obj_t *label = ui_label(row, UI_FONT_16, 0, 0);
    ui_apply_muted(label);
    lv_label_set_text(label, spec->label);
    lv_obj_align(label, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_coord_t seg_w =
        (lv_coord_t) ((CONTENT_W - (spec->count - 1) * SEG_GAP) / spec->count);

    for (uint8_t i = 0; i < spec->count; i++) {
        lv_obj_t *seg = lv_btn_create(row);
        ui_apply_panel(seg);
        lv_obj_set_size(seg, seg_w, SEG_H);
        lv_obj_set_pos(seg, (lv_coord_t) (i * (seg_w + SEG_GAP)), LABEL_H);
        lv_obj_add_event_cb(seg, segment_event_cb, LV_EVENT_CLICKED,
                            (void *) (uintptr_t) (uint16_t) ((id << 8) | i));

        lv_obj_t *text = lv_label_create(seg);
        lv_obj_set_style_text_font(text, UI_FONT_16, 0);
        ui_apply_text(text);
        lv_label_set_text(text, spec->option[i]);
        lv_obj_center(text);

        segment[id][i] = seg;
    }
}

void ui_settings_create(void)
{
    lv_obj_t *screen = ui_screen_create(UI_SCREEN_SETTINGS);
    ui_header_create(screen, "Settings", UI_SCREEN_MENU);

    lv_obj_t *list = ui_list_create(screen, PAD_LEFT, PAD_RIGHT);
    lv_obj_set_style_pad_row(list, ROW_GAP, 0);

    for (uint8_t id = 0; id < SETTING_COUNT; id++) {
        add_setting_row(list, (setting_id_t) id);
    }

    // Directly above "Reset to defaults", as specified. The calibration is
    // reached from here and no longer from the menu.
    add_link_row(list, "Touch calibration", calibration_event_cb);

    lv_obj_t *reset = lv_btn_create(list);
    ui_apply_panel(reset);
    lv_obj_set_size(reset, CONTENT_W, UI_MIN_TOUCH_PX);
    lv_obj_add_event_cb(reset, reset_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *reset_label = lv_label_create(reset);
    lv_obj_set_style_text_font(reset_label, UI_FONT_18, 0);
    ui_apply_text(reset_label);
    lv_label_set_text(reset_label, "Reset to defaults");
    lv_obj_center(reset_label);
}

void ui_settings_refresh(void)
{
    for (uint8_t id = 0; id < SETTING_COUNT; id++) {
        mark_selection((setting_id_t) id);
    }
}
