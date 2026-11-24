#include "ui_internal.h"

#include "settings_store.h"

// ---------------------------------------------------------------------
// The picker opened from a row of the Buttons screen.
//
// It offers everything the original pendant can put on a key: "None", the
// 16 key codes and the 15 Fn combinations - 32 entries from the one table
// in pendant_state.c, laid out over four pages of the same 3x3 grid the
// Functions screen uses. The last page holds five.
//
// Unlike Functions, all NINE cells carry entries here; paging is done from
// the two arrow buttons in the header instead, because the pages belong to
// one list rather than to two independent tabs.
//
// The choice goes back through pendant_assign_action(), which also handles
// the swap when the action already sits on another key. Back leaves without
// changing anything; a choice is applied and returns to the Buttons screen
// straight away, with no confirm step, because every choice is reversible
// by picking again.
// ---------------------------------------------------------------------

#define ARROW_W    UI_MIN_TOUCH_PX
#define ARROW_H    UI_MIN_TOUCH_PX
#define HEADER_PAD 8
#define BACK_W     96

#define PAGE_COUNT ((PENDANT_ACTION_COUNT + UI_GRID_CELLS - 1) / UI_GRID_CELLS)

static ui_grid_tile_t cell_tile[UI_GRID_CELLS];
static lv_obj_t *cell_check[UI_GRID_CELLS];

static lv_obj_t *title_label;
static lv_obj_t *page_label;
static lv_obj_t *prev_button;
static lv_obj_t *next_button;

static uint8_t target_key = 0;
static uint8_t current_page = 0;

static size_t entry_index(uint8_t cell)
{
    return (size_t) current_page * UI_GRID_CELLS + cell;
}

// ---------------------------------------------------------------------
// Filling the page
//
// The nine tiles are REUSED across pages: their labels are rewritten rather
// than 32 tiles being built and hidden. That keeps the LVGL heap down, and
// with nine objects there is nothing to gain from a per-page container.
// ---------------------------------------------------------------------
static void refresh_page(void)
{
    uint8_t current = pendant_mapping()->assigned_function[target_key];

    for (uint8_t cell = 0; cell < UI_GRID_CELLS; cell++) {
        size_t index = entry_index(cell);
        ui_grid_tile_t *tile = &cell_tile[cell];

        if (index >= pendant_action_count) {
            lv_obj_add_flag(tile->obj, LV_OBJ_FLAG_HIDDEN);
            continue;
        }

        const pendant_action_t *action = &pendant_actions[index];
        lv_obj_clear_flag(tile->obj, LV_OBJ_FLAG_HIDDEN);

        lv_label_set_text(tile->name, action->name);
        lv_label_set_text(tile->detail, action->detail);

        // Keys and "None" have no second line, so the name takes the whole
        // tile; a macro gets name on top and combination underneath.
        if (action->detail[0] == '\0') {
            lv_obj_add_flag(tile->detail, LV_OBJ_FLAG_HIDDEN);
            lv_obj_center(tile->name);
        } else {
            lv_obj_clear_flag(tile->detail, LV_OBJ_FLAG_HIDDEN);
            lv_obj_align(tile->name, LV_ALIGN_TOP_MID, 0, 0);
        }

        bool selected = (action->value == current);
        if (selected) {
            ui_apply_selected(tile->obj);
            lv_obj_clear_flag(cell_check[cell], LV_OBJ_FLAG_HIDDEN);
        } else {
            ui_clear_selected(tile->obj);
            lv_obj_add_flag(cell_check[cell], LV_OBJ_FLAG_HIDDEN);
        }
    }

    lv_label_set_text_fmt(title_label, "Button %u", target_key + 1u);
    lv_label_set_text_fmt(page_label, "%u / %u", current_page + 1u, PAGE_COUNT);

    // An arrow at the end of the list is greyed out rather than removed, so
    // the header keeps its shape from page to page.
    if (current_page == 0) {
        lv_obj_add_state(prev_button, LV_STATE_DISABLED);
    } else {
        lv_obj_clear_state(prev_button, LV_STATE_DISABLED);
    }
    if (current_page + 1u >= PAGE_COUNT) {
        lv_obj_add_state(next_button, LV_STATE_DISABLED);
    } else {
        lv_obj_clear_state(next_button, LV_STATE_DISABLED);
    }
}

static void page_event_cb(lv_event_t *e)
{
    int8_t step = (int8_t) (intptr_t) lv_event_get_user_data(e);
    int16_t page = (int16_t) current_page + step;

    if (page < 0 || page >= (int16_t) PAGE_COUNT) {
        return;
    }

    current_page = (uint8_t) page;
    refresh_page();
}

static void tile_press_cb(lv_event_t *e)
{
    ui_grid_tile_t *tile = (ui_grid_tile_t *) lv_event_get_user_data(e);
    ui_grid_tile_set_pressed(tile, lv_event_get_code(e) == LV_EVENT_PRESSED);
}

static void option_event_cb(lv_event_t *e)
{
    if (!ui_grid_released_inside(e)) {
        return;  // the finger slid off - no selection is made
    }

    size_t index = entry_index((uint8_t) (uintptr_t) lv_event_get_user_data(e));
    if (index >= pendant_action_count) {
        return;
    }

    if (pendant_assign_action(target_key, pendant_actions[index].value)) {
        // Only on a real change, and only here on a menu page - never while
        // the machine is being operated.
        settings_store_save_user_data();
    }

    ui_goto(UI_SCREEN_BUTTONS);
}

static lv_obj_t *create_arrow(lv_obj_t *screen, const char *symbol, lv_coord_t x,
                              int8_t step)
{
    lv_obj_t *btn = lv_btn_create(screen);
    ui_apply_nav(btn);
    lv_obj_set_size(btn, ARROW_W, ARROW_H);
    lv_obj_set_pos(btn, x, (UI_HEADER_H - UI_HEADER_LINE_H - ARROW_H) / 2);
    lv_obj_add_event_cb(btn, page_event_cb, LV_EVENT_CLICKED, (void *) (intptr_t) step);

    lv_obj_t *label = lv_label_create(btn);
    lv_obj_set_style_text_font(label, UI_FONT_20, 0);
    ui_apply_accent_label(label);
    lv_label_set_text(label, symbol);
    lv_obj_center(label);

    return btn;
}

void ui_function_pick_create(void)
{
    lv_obj_t *screen = ui_screen_create(UI_SCREEN_FUNCTION_PICK);

    ui_header_surface_create(screen);
    ui_back_button_create(screen, UI_SCREEN_BUTTONS);

    next_button =
        create_arrow(screen, LV_SYMBOL_RIGHT, UI_SCREEN_W - HEADER_PAD - ARROW_W, +1);
    prev_button = create_arrow(screen, LV_SYMBOL_LEFT,
                               UI_SCREEN_W - HEADER_PAD - 2 * ARROW_W - 6, -1);

    // Centred between the Back button and the arrows
    lv_coord_t text_left = HEADER_PAD + BACK_W;
    lv_coord_t text_right = UI_SCREEN_W - HEADER_PAD - 2 * ARROW_W - 6;

    title_label = ui_label(screen, UI_FONT_18, 0, 0);
    ui_apply_text(title_label);
    lv_obj_set_width(title_label, text_right - text_left);
    lv_obj_set_style_text_align(title_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(title_label, text_left, 8);

    page_label = ui_label(screen, UI_FONT_NUM_TINY, 0, 0);
    ui_apply_muted(page_label);
    lv_obj_set_width(page_label, text_right - text_left);
    lv_obj_set_style_text_align(page_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(page_label, text_left, 30);

    for (uint8_t cell = 0; cell < UI_GRID_CELLS; cell++) {
        ui_grid_tile_t *tile = &cell_tile[cell];

        tile->obj = ui_grid_tile_create(screen, cell);
        lv_obj_add_event_cb(tile->obj, option_event_cb, LV_EVENT_RELEASED,
                            (void *) (uintptr_t) cell);
        lv_obj_add_event_cb(tile->obj, tile_press_cb, LV_EVENT_PRESSED, tile);
        lv_obj_add_event_cb(tile->obj, tile_press_cb, LV_EVENT_RELEASED, tile);
        lv_obj_add_event_cb(tile->obj, tile_press_cb, LV_EVENT_PRESS_LOST, tile);

        // Both labels always exist; refresh_page() decides what shows. That
        // way a page change is text, never object creation.
        tile->name = lv_label_create(tile->obj);
        lv_obj_set_style_text_font(tile->name, UI_FONT_16, 0);
        lv_label_set_long_mode(tile->name, LV_LABEL_LONG_WRAP);
        // Narrower than on the Functions screen: the check mark of the
        // current selection sits in the top right corner and the name must
        // not run underneath it.
        lv_obj_set_width(tile->name, UI_GRID_TILE_W - 24);
        lv_obj_set_style_text_align(tile->name, LV_TEXT_ALIGN_CENTER, 0);

        tile->detail = lv_label_create(tile->obj);
        lv_obj_set_style_text_font(tile->detail, UI_FONT_NUM_TINY, 0);
        ui_apply_accent_label(tile->detail);
        lv_obj_align(tile->detail, LV_ALIGN_BOTTOM_MID, 0, 0);

        cell_check[cell] = lv_label_create(tile->obj);
        lv_obj_set_style_text_font(cell_check[cell], UI_FONT_12, 0);
        ui_apply_accent_label(cell_check[cell]);
        lv_label_set_text(cell_check[cell], LV_SYMBOL_OK);
        lv_obj_align(cell_check[cell], LV_ALIGN_TOP_RIGHT, 0, 0);
        lv_obj_add_flag(cell_check[cell], LV_OBJ_FLAG_HIDDEN);
    }
}

void ui_function_pick_open(uint8_t key_index)
{
    if (key_index >= PENDANT_BUTTON_COUNT) {
        return;
    }
    target_key = key_index;

    // Open on the page that holds the current choice, so the marked tile is
    // in front of the user instead of being hunted for.
    size_t index = pendant_action_index(pendant_mapping()->assigned_function[key_index]);
    current_page =
        (index == PENDANT_ACTION_NOT_FOUND) ? 0 : (uint8_t) (index / UI_GRID_CELLS);

    refresh_page();
    ui_goto(UI_SCREEN_FUNCTION_PICK);
}
