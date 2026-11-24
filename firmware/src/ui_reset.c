#include "ui_internal.h"

#include "settings_store.h"

// ---------------------------------------------------------------------
// Screen "Reset": a deliberate confirmation before throwing settings and
// key assignment away.
//
// The touch calibration is deliberately NOT part of it. Resetting it would
// leave the device with a mapping that may make the calibration screen
// unreachable, and it is a measured property of the panel rather than a
// preference - there is nothing sensible to reset it to.
// ---------------------------------------------------------------------

#define BTN_H    56
#define PAD_SIDE 12
#define BTN_GAP  8
#define BTN_W    ((UI_SCREEN_W - 2 * PAD_SIDE - BTN_GAP) / 2)

static void cancel_event_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    ui_goto(UI_SCREEN_SETTINGS);
}

static void confirm_event_cb(lv_event_t *e)
{
    LV_UNUSED(e);

    pendant_settings_set_defaults();
    pendant_set_default_mapping();
    pendant_settings_apply();

    // One write for both parts - they share a record. The calibration field
    // is left as it is, which is what keeps the calibration.
    settings_store_save_user_data();

    ui_goto(UI_SCREEN_MAIN);
}

void ui_reset_create(void)
{
    lv_obj_t *screen = ui_screen_create(UI_SCREEN_RESET);
    ui_header_create(screen, "Reset", UI_SCREEN_SETTINGS);

    lv_obj_t *question = ui_label(screen, UI_FONT_20, 0, 0);
    ui_apply_text(question);
    lv_label_set_long_mode(question, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(question, UI_SCREEN_W - 2 * PAD_SIDE);
    lv_obj_set_style_text_align(question, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(question,
                      "Reset all settings and the button assignment to defaults?");
    lv_obj_align(question, LV_ALIGN_TOP_MID, 0, UI_HEADER_H + 6);

    lv_obj_t *note = ui_label(screen, UI_FONT_15, 0, 0);
    ui_apply_muted(note);
    lv_label_set_text(note, "Touch calibration is kept.");
    lv_obj_align(note, LV_ALIGN_TOP_MID, 0, UI_HEADER_H + 84);

    lv_obj_t *cancel = lv_btn_create(screen);
    ui_apply_panel(cancel);
    lv_obj_set_size(cancel, BTN_W, BTN_H);
    lv_obj_align(cancel, LV_ALIGN_BOTTOM_LEFT, PAD_SIDE, -8);
    lv_obj_add_event_cb(cancel, cancel_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *cancel_label = lv_label_create(cancel);
    lv_obj_set_style_text_font(cancel_label, UI_FONT_20, 0);
    ui_apply_text(cancel_label);
    lv_label_set_text(cancel_label, "Cancel");
    lv_obj_center(cancel_label);

    lv_obj_t *confirm = lv_btn_create(screen);
    ui_apply_accent_fill(confirm);
    lv_obj_set_size(confirm, BTN_W, BTN_H);
    lv_obj_align(confirm, LV_ALIGN_BOTTOM_RIGHT, -PAD_SIDE, -8);
    lv_obj_add_event_cb(confirm, confirm_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *confirm_label = lv_label_create(confirm);
    lv_obj_set_style_text_font(confirm_label, UI_FONT_20, 0);
    ui_apply_on_accent(confirm_label);
    lv_label_set_text(confirm_label, "Reset");
    lv_obj_center(confirm_label);
}
