#include "ui.h"

#include "pico/stdlib.h"

#include "settings_store.h"
#include "ui_internal.h"
#include "whb04b_codes.h"

static lv_obj_t *screens[UI_SCREEN_COUNT];
static ui_screen_id_t active_screen = UI_SCREEN_MAIN;

static absolute_time_t next_refresh;
static absolute_time_t next_debug_refresh;

const char *ui_axis_text(uint8_t axis_code)
{
    switch (axis_code) {
    case WHB04B_AXIS_X:
        return "X";
    case WHB04B_AXIS_Y:
        return "Y";
    case WHB04B_AXIS_Z:
        return "Z";
    case WHB04B_AXIS_A:
        return "A";
    case WHB04B_AXIS_B:
        return "B";
    case WHB04B_AXIS_C:
        return "C";
    case WHB04B_AXIS_OFF:
        return "off";
    default:
        return "?";
    }
}

const char *ui_step_width_text(uint8_t feed_code)
{
    switch (feed_code) {
    case WHB04B_FEED_0_001:
        return "0.001";
    case WHB04B_FEED_0_01:
        return "0.01";
    case WHB04B_FEED_0_1:
        return "0.1";
    case WHB04B_FEED_1:
        return "1";
    case WHB04B_FEED_5:
        return "5";
    case WHB04B_FEED_10:
        return "10";
    case WHB04B_FEED_LEAD:
        return "Lead";
    default:
        return "?";
    }
}

const char *ui_step_mode_text(uint8_t step_mode)
{
    switch (step_mode) {
    case 0:
        return "CON";
    case 1:
        return "STEP";
    case 2:
        return "MPG";
    default:
        return "%";
    }
}

lv_obj_t *ui_row_create(lv_obj_t *list, lv_coord_t w, lv_coord_t h, ui_row_style_t style)
{
    lv_obj_t *row = lv_obj_create(list);

    if (style == UI_ROW_PANEL) {
        ui_apply_panel(row);
        lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
    } else {
        ui_apply_transparent(row);
        if (style == UI_ROW_PLAIN_SEPARATED) {
            ui_apply_separator_row(row);
        }
    }

    lv_obj_set_size(row, w, h);
    return row;
}

lv_obj_t *ui_label(lv_obj_t *parent, const lv_font_t *font, lv_coord_t x, lv_coord_t y)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_pos(label, x, y);
    lv_label_set_text(label, "");

    return label;
}

lv_obj_t *ui_list_create(lv_obj_t *screen, lv_coord_t pad_left, lv_coord_t pad_right)
{
    lv_obj_t *list = lv_obj_create(screen);
    ui_apply_transparent(list);
    lv_obj_set_size(list, UI_SCREEN_W, UI_SCREEN_H - UI_HEADER_H);
    lv_obj_set_pos(list, 0, UI_HEADER_H);
    lv_obj_set_style_pad_left(list, pad_left, 0);
    lv_obj_set_style_pad_right(list, pad_right, 0);
    lv_obj_set_style_pad_top(list, UI_CONTENT_GAP, 0);
    lv_obj_set_style_pad_bottom(list, 8, 0);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);

    // Vertical only: a horizontal drag must not shift the list sideways.
    // Scrolling is the only gesture anywhere in this interface.
    lv_obj_add_flag(list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(list, LV_DIR_VER);

    // CLICKABLE even though nothing listens for a click on the list itself.
    // lv_obj_hit_test() returns false for anything without that flag, and
    // find_scroll_obj() starts from whatever the hit test found - so on a
    // list whose rows are plain display objects the finger would hit
    // nothing, LVGL would land on the screen, and dragging would not scroll
    // at all. Lists with clickable rows (Buttons, Assign, Settings) worked
    // by accident; Debug and Function codes did not.
    lv_obj_add_flag(list, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_ON);
    ui_apply_scrollbar(list);

    return list;
}

static void nav_event_cb(lv_event_t *e)
{
    ui_goto((ui_screen_id_t) (uintptr_t) lv_event_get_user_data(e));
}

lv_obj_t *ui_screen_create(ui_screen_id_t id)
{
    lv_obj_t *screen = lv_obj_create(NULL);
    ui_apply_screen(screen);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    screens[id] = screen;
    return screen;
}

// The Back button and the title are the only two elements. The title is
// deliberately a plain label: a frame or a background would make it look
// like something to press.
// The header is a surface of its own, 56 px tall including a 2 px rule at
// the bottom. Every page that puts content under it leaves UI_CONTENT_GAP
// below that rule, so the first row never sits against the line.
//
// Created before anything else on the screen, so later widgets draw on top
// of it rather than disappearing behind it.
lv_obj_t *ui_header_surface_create(lv_obj_t *screen)
{
    lv_obj_t *bar = lv_obj_create(screen);
    ui_apply_header(bar);
    lv_obj_set_size(bar, UI_SCREEN_W, UI_HEADER_H);
    lv_obj_set_pos(bar, 0, 0);

    return bar;
}

lv_obj_t *ui_back_button_create(lv_obj_t *screen, ui_screen_id_t back_target)
{
    lv_obj_t *back = lv_btn_create(screen);
    ui_apply_accent_button(back);
    lv_obj_set_size(back, 96, UI_MIN_TOUCH_PX);
    lv_obj_set_pos(back, 8, (UI_HEADER_H - UI_MIN_TOUCH_PX) / 2);
    lv_obj_add_event_cb(back, nav_event_cb, LV_EVENT_CLICKED,
                        (void *) (uintptr_t) back_target);

    lv_obj_t *back_label = lv_label_create(back);
    lv_obj_set_style_text_font(back_label, UI_FONT_20, 0);
    ui_apply_accent_label(back_label);
    lv_label_set_text(back_label, LV_SYMBOL_LEFT " Back");
    lv_obj_center(back_label);

    return back;
}

void ui_header_create(lv_obj_t *screen, const char *title, ui_screen_id_t back_target)
{
    ui_header_surface_create(screen);

    lv_obj_t *back = ui_back_button_create(screen, back_target);

    lv_obj_t *title_label = ui_label(screen, UI_FONT_22, 0, 0);
    ui_apply_muted(title_label);
    lv_label_set_text(title_label, title);
    // Relative to the button, not to the screen: LV_ALIGN_LEFT_MID would
    // centre on the 240 px screen and drop the title behind the content.
    // 16 px is the gap between header elements.
    lv_obj_align_to(title_label, back, LV_ALIGN_OUT_RIGHT_MID, 16, 0);
}

void ui_goto(ui_screen_id_t id)
{
    // Leaving the settings screen is the moment its choices are committed.
    // A flash write freezes the main loop and with it the safety heartbeat,
    // so it must not happen while the machine is being operated - which is
    // why it is tied to leaving a menu page and not to each tap. Writing is
    // skipped entirely when nothing changed.
    if (active_screen == UI_SCREEN_SETTINGS && id != UI_SCREEN_RESET) {
        settings_store_save_user_data();
    }

    active_screen = id;

    if (id == UI_SCREEN_CALIBRATION) {
        ui_calibration_restart();
    }
    if (id == UI_SCREEN_BUTTONS) {
        ui_buttons_refresh();
    }
    if (id == UI_SCREEN_SETTINGS) {
        ui_settings_refresh();
    }

    lv_scr_load(screens[id]);
}

void ui_init(void)
{
    ui_theme_init();

    ui_main_create();
    ui_menu_create();
    ui_settings_create();
    ui_reset_create();
    ui_buttons_create();
    ui_function_pick_create();
    ui_functions_create();
    ui_calibration_create();
    ui_debug_create();

    ui_goto(UI_SCREEN_MAIN);

    next_refresh = get_absolute_time();
}

// Back to the main screen after a while without a touch. LVGL already
// tracks input inactivity, so there is no timer of our own to keep in sync.
//
// Not on Main, because there is nowhere to return to, and not during the
// calibration: that screen waits for four deliberate taps and jumping away
// mid-measurement would throw the partial result away.
static void handle_auto_return(void)
{
    if (active_screen == UI_SCREEN_MAIN || active_screen == UI_SCREEN_CALIBRATION) {
        return;
    }

    uint32_t timeout_ms = pendant_settings_auto_return_ms();
    if (timeout_ms == 0) {
        return;
    }

    if (lv_disp_get_inactive_time(NULL) >= timeout_ms) {
        ui_goto(UI_SCREEN_MAIN);
    }
}

void ui_update(void)
{
    // 30 Hz. On top of this interval come the LVGL refresh (20 ms) and the
    // SPI transfer. Going much faster does not pay off while the flush is
    // blocking - it would become the bottleneck, stretch the main loop and
    // in the end make the display feel more sluggish rather than less.
    if (absolute_time_diff_us(get_absolute_time(), next_refresh) > 0) {
        return;
    }
    next_refresh = delayed_by_ms(get_absolute_time(), 33);

    handle_auto_return();

    if (active_screen == UI_SCREEN_MAIN) {
        ui_main_update();
    } else if (active_screen == UI_SCREEN_FUNCTIONS) {
        // Collects the outcome of a tap and clears the confirmation again.
        // 30 Hz is far finer than the 400 ms the tick is shown for.
        ui_functions_update();
    } else if (active_screen == UI_SCREEN_DEBUG) {
        // 10 Hz: slow enough not to redraw the screen constantly, fast
        // enough that the button bit mask still follows a finger - that row
        // is how the key wiring gets checked. Each row compares its own text
        // first, so an unchanged screen costs nothing.
        if (absolute_time_diff_us(get_absolute_time(), next_debug_refresh) <= 0) {
            next_debug_refresh = delayed_by_ms(get_absolute_time(), 100);
            ui_debug_update();
        }
    }
}

void ui_show_main(void)
{
    ui_goto(UI_SCREEN_MAIN);
}

void ui_show_calibration(void)
{
    ui_goto(UI_SCREEN_CALIBRATION);
}
