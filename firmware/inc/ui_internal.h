#ifndef UI_INTERNAL_H
#define UI_INTERNAL_H

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

#include "pendant_settings.h"
#include "pendant_state.h"
#include "ui_theme.h"

typedef enum {
    UI_SCREEN_MAIN = 0,
    UI_SCREEN_MENU,
    UI_SCREEN_SETTINGS,
    UI_SCREEN_RESET,
    UI_SCREEN_BUTTONS,
    UI_SCREEN_FUNCTION_PICK,
    UI_SCREEN_FUNCTIONS,
    UI_SCREEN_CALIBRATION,
    UI_SCREEN_DEBUG,
    UI_SCREEN_COUNT
} ui_screen_id_t;

// Creates an empty screen in the background colour and returns it. Screens
// position their children absolutely, so no layout is set here.
lv_obj_t *ui_screen_create(ui_screen_id_t id);

// The identical 56 px header of every screen except Main: a Back button on
// the left, the page title beside it as plain text.
void ui_header_create(lv_obj_t *screen, const char *title, ui_screen_id_t back_target);

// The header's own background surface, including the rule at its bottom.
// Pages that build their own header (Functions, Assign) call this first.
lv_obj_t *ui_header_surface_create(lv_obj_t *screen);

// Just the Back button of that header, 96x48 at the top left. Used directly
// by the Functions screen, which puts its two tabs where the title would be.
lv_obj_t *ui_back_button_create(lv_obj_t *screen, ui_screen_id_t back_target);

void ui_goto(ui_screen_id_t id);

// A scrolling container filling everything below the header. Used by every
// list screen so the padding, the scroll direction and the scrollbar look
// the same in all of them.
lv_obj_t *ui_list_create(lv_obj_t *screen, lv_coord_t pad_left, lv_coord_t pad_right);

// ---------------------------------------------------------------------
// One row of a list.
//
// Four screens used to build this by hand, and they drifted: two of them
// gave their rows LV_OBJ_FLAG_CLICKABLE and two did not, which is precisely
// why Debug and Function codes would not scroll (see the note on scrolling
// in docs/ARCHITECTURE.md). The container now comes from one place; the labels inside
// stay with each screen, because those genuinely differ.
// ---------------------------------------------------------------------
typedef enum {
    UI_ROW_PLAIN,            // transparent, no rule
    UI_ROW_PLAIN_SEPARATED,  // transparent with a hairline along the bottom
    UI_ROW_PANEL             // panel surface, tappable
} ui_row_style_t;

lv_obj_t *ui_row_create(lv_obj_t *list, lv_coord_t w, lv_coord_t h, ui_row_style_t style);

// ---------------------------------------------------------------------
// The 3x3 tile grid of Functions and Assign (see ui_grid.c)
// ---------------------------------------------------------------------
#define UI_GRID_COLS   3
#define UI_GRID_ROWS   3
#define UI_GRID_CELLS  (UI_GRID_COLS * UI_GRID_ROWS)
#define UI_GRID_MARGIN 8
#define UI_GRID_GAP    6
#define UI_GRID_TILE_W 97
#define UI_GRID_TILE_H 52

// Eight actions per page; the ninth cell carries the page control.
#define UI_GRID_PER_PAGE (UI_GRID_CELLS - 1)

typedef struct {
    lv_obj_t *obj;
    lv_obj_t *name;
    lv_obj_t *detail;  // NULL when the action has no second line
} ui_grid_tile_t;

lv_coord_t ui_grid_cell_x(uint8_t index);
lv_coord_t ui_grid_cell_y(uint8_t index);
lv_obj_t *ui_grid_tile_create(lv_obj_t *parent, uint8_t index);
void ui_grid_tile_fill(ui_grid_tile_t *tile, const pendant_action_t *action);
void ui_grid_tile_set_pressed(const ui_grid_tile_t *tile, bool pressed);
bool ui_grid_released_inside(lv_event_t *e);

// A plain label in a given font, positioned absolutely. The colour comes
// from one of the ui_apply_* helpers so it follows a theme change.
lv_obj_t *ui_label(lv_obj_t *parent, const lv_font_t *font, lv_coord_t x, lv_coord_t y);

// Protocol codes as display text. Shared, because several screens need them
// and a second copy would drift.
const char *ui_axis_text(uint8_t axis_code);
const char *ui_step_width_text(uint8_t feed_code);
const char *ui_step_mode_text(uint8_t step_mode);

// One file per screen
void ui_main_create(void);
void ui_main_update(void);

void ui_menu_create(void);

void ui_settings_create(void);
void ui_settings_refresh(void);

void ui_reset_create(void);

void ui_buttons_create(void);
void ui_buttons_refresh(void);

void ui_function_pick_create(void);
void ui_function_pick_open(uint8_t key_index);

void ui_functions_create(void);
void ui_functions_update(void);  // the "Sent" confirmation lives here

void ui_calibration_create(void);
void ui_calibration_restart(void);

void ui_debug_create(void);
void ui_debug_update(void);

#endif
