#include "ui_internal.h"

// ---------------------------------------------------------------------
// The 3x3 tile grid shared by Functions and Assign.
//
// Both pages show the same thing in the same shape - a page of eight or
// nine action tiles - and they drifted apart once already when every screen
// built its own rows. The geometry and the tile contents live here; what
// differs (which actions, what the ninth cell does, what a tap means) stays
// with the screen.
//
// Geometry from the specification:
//   margin 8 px, 10 px below the header, 3 columns, 6 px gaps
//   usable width  320 - 2*8   = 304  ->  (304 - 2*6) / 3 = 97 px per tile
//   usable height 240 - 56 - 10 = 174 -> 3 * 52 + 2 * 6 = 168, 6 px to spare
// ---------------------------------------------------------------------

lv_coord_t ui_grid_cell_x(uint8_t index)
{
    return (lv_coord_t) (UI_GRID_MARGIN +
                         (index % UI_GRID_COLS) * (UI_GRID_TILE_W + UI_GRID_GAP));
}

lv_coord_t ui_grid_cell_y(uint8_t index)
{
    return (lv_coord_t) (UI_HEADER_H + UI_CONTENT_GAP +
                         (index / UI_GRID_COLS) * (UI_GRID_TILE_H + UI_GRID_GAP));
}

lv_obj_t *ui_grid_tile_create(lv_obj_t *parent, uint8_t index)
{
    lv_obj_t *tile = lv_btn_create(parent);
    ui_apply_tile(tile);
    lv_obj_set_size(tile, UI_GRID_TILE_W, UI_GRID_TILE_H);
    lv_obj_set_pos(tile, ui_grid_cell_x(index), ui_grid_cell_y(index));
    lv_obj_set_style_pad_all(tile, 4, 0);

    return tile;
}

// ---------------------------------------------------------------------
// Did this release happen on the tile it started on?
//
// Two LVGL behaviours decide this, and only the second one needed work:
//
//  1. The press STAYS with the tile it began on. Every object with a parent
//     is created with LV_OBJ_FLAG_PRESS_LOCK (lv_obj.c), and
//     indev_proc_press() only re-runs its hit test when that flag is
//     absent. So a finger moving to the neighbouring tile does NOT hand the
//     press over - the neighbour can never fire.
//  2. But the release itself is not hit-tested at all. indev_proc_release()
//     sends RELEASED and then CLICKED to the pressed object no matter where
//     the finger ended up. So without the check below, sliding OFF a tile
//     and letting go still triggers the tile the finger started on.
//
// Point 2 is a send the user did not ask for, and the specification rules
// it out: fill on touch, send only on release over the same tile, nothing
// at all if the finger slides off. The tiles therefore act on
// LV_EVENT_RELEASED and ask this first.
//
// Measured, not assumed: with this function stubbed to return true, the
// host test functest.c fails exactly its two sliding cases.
// ---------------------------------------------------------------------
bool ui_grid_released_inside(lv_event_t *e)
{
    lv_indev_t *indev = lv_event_get_indev(e);
    if (indev == NULL) {
        return false;
    }

    lv_point_t point;
    lv_indev_get_point(indev, &point);

    lv_area_t area;
    lv_obj_get_coords(lv_event_get_target(e), &area);

    return point.x >= area.x1 && point.x <= area.x2 && point.y >= area.y1 &&
           point.y <= area.y2;
}

// The name sets NO colour of its own on purpose: LVGL inherits text colour,
// and ui_apply_tile() carries one for the pressed state, so the name turns
// dark along with the fill without any help. The detail line below does set
// its own accent colour and therefore needs ui_set_on_accent() swapped in
// while pressed - that is what ui_grid_tile_set_pressed() is for.
void ui_grid_tile_fill(ui_grid_tile_t *tile, const pendant_action_t *action)
{
    lv_obj_t *obj = tile->obj;

    tile->name = lv_label_create(obj);
    lv_obj_set_style_text_font(tile->name, UI_FONT_16, 0);
    lv_label_set_long_mode(tile->name, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(tile->name, UI_GRID_TILE_W - 10);
    lv_obj_set_style_text_align(tile->name, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(tile->name, action->name);

    tile->detail = NULL;
    if (action->detail[0] != '\0') {
        tile->detail = lv_label_create(obj);
        lv_obj_set_style_text_font(tile->detail, UI_FONT_NUM_TINY, 0);
        ui_apply_accent_label(tile->detail);
        lv_label_set_text(tile->detail, action->detail);
        lv_obj_align(tile->detail, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_align(tile->name, LV_ALIGN_TOP_MID, 0, 0);
    } else {
        // Nothing underneath, so the name owns the whole tile
        lv_obj_center(tile->name);
    }
}

void ui_grid_tile_set_pressed(const ui_grid_tile_t *tile, bool pressed)
{
    if (tile->detail != NULL) {
        ui_set_on_accent(tile->detail, pressed);
    }
}
