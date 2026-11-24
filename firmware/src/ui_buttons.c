#include "ui_internal.h"

// ---------------------------------------------------------------------
// Screen "Buttons": the eight key assignments as a scrolling list. Tapping
// a row opens the picker for that key.
//
// Eight rows of 56 px plus 6 px gaps come to 490 px in a 184 px viewport, so
// the list scrolls. Scrolling is the only gesture anywhere in this
// interface.
// ---------------------------------------------------------------------

#define ROW_H      56
#define ROW_GAP    6
#define PAD_LEFT   8
#define PAD_RIGHT  4
#define NUMBER_BOX 32
#define ROW_PAD    12

// Leave the scrollbar its own lane so it never sits on top of the text
#define ROW_W (UI_SCREEN_W - PAD_LEFT - PAD_RIGHT - UI_SCROLLBAR_W)

static lv_obj_t *function_label[PENDANT_BUTTON_COUNT];

static void row_event_cb(lv_event_t *e)
{
    ui_function_pick_open((uint8_t) (uintptr_t) lv_event_get_user_data(e));
}

void ui_buttons_create(void)
{
    lv_obj_t *screen = ui_screen_create(UI_SCREEN_BUTTONS);
    ui_header_create(screen, "Buttons", UI_SCREEN_MENU);

    lv_obj_t *list = ui_list_create(screen, PAD_LEFT, PAD_RIGHT);
    lv_obj_set_style_pad_row(list, ROW_GAP, 0);

    for (uint8_t i = 0; i < PENDANT_BUTTON_COUNT; i++) {
        lv_obj_t *row = ui_row_create(list, ROW_W, ROW_H, UI_ROW_PANEL);
        lv_obj_set_style_pad_all(row, ROW_PAD, 0);
        lv_obj_add_event_cb(row, row_event_cb, LV_EVENT_CLICKED, (void *) (uintptr_t) i);

        lv_obj_t *number_box = lv_obj_create(row);
        ui_apply_panel_active(number_box);
        lv_obj_set_size(number_box, NUMBER_BOX, NUMBER_BOX);
        lv_obj_align(number_box, LV_ALIGN_LEFT_MID, 0, 0);
        lv_obj_set_style_radius(number_box, 6, 0);
        lv_obj_set_style_pad_all(number_box, 0, 0);
        lv_obj_clear_flag(number_box, LV_OBJ_FLAG_CLICKABLE);

        lv_obj_t *number = lv_label_create(number_box);
        lv_obj_set_style_text_font(number, UI_FONT_18, 0);
        ui_apply_accent_label(number);
        lv_label_set_text_fmt(number, "%u", i + 1);
        lv_obj_center(number);

        function_label[i] = lv_label_create(row);
        lv_obj_set_style_text_font(function_label[i], UI_FONT_20, 0);
        ui_apply_text(function_label[i]);
        lv_label_set_text(function_label[i], "");
        lv_obj_align(function_label[i], LV_ALIGN_LEFT_MID, NUMBER_BOX + ROW_PAD, 0);

        lv_obj_t *chevron = lv_label_create(row);
        lv_obj_set_style_text_font(chevron, UI_FONT_20, 0);
        ui_apply_muted(chevron);
        lv_label_set_text(chevron, LV_SYMBOL_RIGHT);
        lv_obj_align(chevron, LV_ALIGN_RIGHT_MID, 0, 0);
    }
}

void ui_buttons_refresh(void)
{
    const button_mapping_t *mapping = pendant_mapping();

    for (uint8_t i = 0; i < PENDANT_BUTTON_COUNT; i++) {
        lv_label_set_text(function_label[i],
                          pendant_action_name(mapping->assigned_function[i]));
    }
}
