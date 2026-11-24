#include "ui_internal.h"

// ---------------------------------------------------------------------
// Screen "Menu": four tiles in a 2x2 grid below the header.
//
// Geometry: header 56, so 184 px remain. 8 px padding on every side and an
// 8 px gap leave 304 x 168 for the tiles, which makes each one 148 x 80 -
// comfortably above UI_MIN_TOUCH_PX in both directions.
// ---------------------------------------------------------------------

#define TILE_PAD 8
#define TILE_GAP 8
#define TILE_W   ((UI_SCREEN_W - 2 * TILE_PAD - TILE_GAP) / 2)
#define TILE_H   ((UI_SCREEN_H - UI_HEADER_H - 2 * TILE_PAD - TILE_GAP) / 2)

typedef struct {
    const char *icon;
    const char *text;
    ui_screen_id_t target;
} tile_spec_t;

static void tile_event_cb(lv_event_t *e)
{
    ui_goto((ui_screen_id_t) (uintptr_t) lv_event_get_user_data(e));
}

static void create_tile(lv_obj_t *screen, const tile_spec_t *spec, lv_coord_t x,
                        lv_coord_t y)
{
    lv_obj_t *tile = lv_btn_create(screen);
    ui_apply_panel(tile);
    lv_obj_set_size(tile, TILE_W, TILE_H);
    lv_obj_set_pos(tile, x, y);
    lv_obj_add_event_cb(tile, tile_event_cb, LV_EVENT_CLICKED,
                        (void *) (uintptr_t) spec->target);

    // Icon over caption as a centred flex column rather than two fixed
    // offsets: a caption may wrap to two lines, and with a fixed offset the
    // second line landed past the bottom edge. The column centres whatever
    // height the block ends up with.
    lv_obj_set_style_pad_all(tile, 4, 0);
    lv_obj_set_style_pad_row(tile, 2, 0);
    lv_obj_set_flex_flow(tile, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(tile, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    lv_obj_t *icon = lv_label_create(tile);
    lv_obj_set_style_text_font(icon, UI_FONT_28, 0);
    ui_apply_muted(icon);
    lv_label_set_text(icon, spec->icon);

    lv_obj_t *label = lv_label_create(tile);
    lv_obj_set_style_text_font(label, UI_FONT_18, 0);
    ui_apply_text(label);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(label, TILE_W - 12);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(label, spec->text);
}

void ui_menu_create(void)
{
    lv_obj_t *screen = ui_screen_create(UI_SCREEN_MENU);
    ui_header_create(screen, "Menu", UI_SCREEN_MAIN);

    static const tile_spec_t tiles[4] = {
        { LV_SYMBOL_KEYBOARD, "Buttons", UI_SCREEN_BUTTONS },
        { LV_SYMBOL_SETTINGS, "Settings", UI_SCREEN_SETTINGS },
        { LV_SYMBOL_CHARGE, "Functions", UI_SCREEN_FUNCTIONS },
        { LV_SYMBOL_LIST, "Debug", UI_SCREEN_DEBUG },
    };

    for (uint8_t i = 0; i < 4; i++) {
        lv_coord_t x = TILE_PAD + (i % 2) * (TILE_W + TILE_GAP);
        lv_coord_t y = UI_HEADER_H + TILE_PAD + (i / 2) * (TILE_H + TILE_GAP);
        create_tile(screen, &tiles[i], x, y);
    }
}
