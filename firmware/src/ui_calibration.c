#include "ui_internal.h"

#include "ili9341_driver.h"
#include "settings_store.h"
#include "xpt2046_touch.h"

// ---------------------------------------------------------------------
// Touch calibration, four points.
//
// Target positions are given in the CONTENT area (320 x 184 below the
// header) and offset by the header height here. Deliberately not right in
// the corners: a point hard against the panel edge cannot be hit cleanly
// with a finger. The edge values are extrapolated linearly afterwards.
//
// Tap order, and which corner each one is:
//   0 (290/154) right bottom    2 (30/30)   left  top
//   1 (30/154)  left  bottom    3 (290/30)  right top
//
// Nothing is written to flash until the measurement has passed the
// plausibility check - a wrong calibration saved over a working one would
// leave the device with a screen that cannot be operated, and the
// calibration page is the one page you would need to reach to fix it.
// ---------------------------------------------------------------------

#define POINT_X_LEFT   30
#define POINT_X_RIGHT  290
#define POINT_Y_TOP    (UI_HEADER_H + 30)
#define POINT_Y_BOTTOM (UI_HEADER_H + 154)

#define TARGET_COUNT 4

// Cross: circle r 9, arms reaching 20 px from the centre, 3 px thick (2.5 px
// in the specification - a half pixel does not exist on this panel).
#define CROSS_ARM    20
#define CROSS_THICK  3
#define CROSS_RADIUS 9
#define CROSS_BOX    (2 * CROSS_ARM)

static const lv_point_t targets[TARGET_COUNT] = {
    { POINT_X_RIGHT, POINT_Y_BOTTOM },
    { POINT_X_LEFT, POINT_Y_BOTTOM },
    { POINT_X_LEFT, POINT_Y_TOP },
    { POINT_X_RIGHT, POINT_Y_TOP },
};

// Which taps sit on which side, derived from the table above. Keeping these
// next to it means a change of the tap order cannot quietly desynchronise
// the arithmetic in apply_calibration().
#define TAP_RIGHT_A  0
#define TAP_RIGHT_B  3
#define TAP_LEFT_A   1
#define TAP_LEFT_B   2
#define TAP_TOP_A    2
#define TAP_TOP_B    3
#define TAP_BOTTOM_A 0
#define TAP_BOTTOM_B 1

static lv_obj_t *marker;
static lv_obj_t *instruction_label;
static lv_obj_t *progress_label;
static lv_obj_t *capture_overlay;

// Result overlay, used for both outcomes
static lv_obj_t *result_panel;
static lv_obj_t *result_icon;
static lv_obj_t *result_title;
static lv_obj_t *result_hint;
static lv_obj_t *result_button_label;
static bool result_is_failure;

static uint16_t sampled_x[TARGET_COUNT];
static uint16_t sampled_y[TARGET_COUNT];
static uint8_t step = 0;

static int32_t absolute(int32_t value)
{
    return value < 0 ? -value : value;
}

static void show_step(void)
{
    lv_obj_add_flag(result_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(capture_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(marker, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(instruction_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(progress_label, LV_OBJ_FLAG_HIDDEN);

    lv_obj_set_pos(marker, targets[step].x - CROSS_ARM, targets[step].y - CROSS_ARM);
    lv_label_set_text_fmt(progress_label, "Point %u of %u", step + 1u, TARGET_COUNT);
}

static void show_result(bool failure)
{
    result_is_failure = failure;

    lv_obj_add_flag(marker, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(capture_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(instruction_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(progress_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(result_panel, LV_OBJ_FLAG_HIDDEN);

    lv_label_set_text(result_icon, failure ? LV_SYMBOL_WARNING : LV_SYMBOL_OK);
    lv_label_set_text(result_title, failure ? "Calibration failed" : "Calibration saved");
    lv_label_set_text(result_hint, failure ? "Tap the center of each cross." : "");
    lv_label_set_text(result_button_label, failure ? "Try again" : "OK");
}

static void result_button_cb(lv_event_t *e)
{
    LV_UNUSED(e);

    if (result_is_failure) {
        step = 0;
        show_step();
    } else {
        ui_goto(UI_SCREEN_SETTINGS);
    }
}

static void apply_calibration(void)
{
    // Which raw axis runs along the screen width? The one that changes more
    // between the left and the right taps. This MEASURES the axis swap
    // instead of deriving it from the MADCTL value - the mapping depends on
    // the individual panel, and guessing has cost enough time already.
    int32_t h_spread_x = ((int32_t) sampled_x[TAP_RIGHT_A] + sampled_x[TAP_RIGHT_B]) -
                         ((int32_t) sampled_x[TAP_LEFT_A] + sampled_x[TAP_LEFT_B]);
    int32_t h_spread_y = ((int32_t) sampled_y[TAP_RIGHT_A] + sampled_y[TAP_RIGHT_B]) -
                         ((int32_t) sampled_y[TAP_LEFT_A] + sampled_y[TAP_LEFT_B]);
    int32_t v_spread_x = ((int32_t) sampled_x[TAP_BOTTOM_A] + sampled_x[TAP_BOTTOM_B]) -
                         ((int32_t) sampled_x[TAP_TOP_A] + sampled_x[TAP_TOP_B]);
    int32_t v_spread_y = ((int32_t) sampled_y[TAP_BOTTOM_A] + sampled_y[TAP_BOTTOM_B]) -
                         ((int32_t) sampled_y[TAP_TOP_A] + sampled_y[TAP_TOP_B]);

    bool swap_xy = absolute(h_spread_y) > absolute(h_spread_x);

    // Plausibility, part one: the two screen directions must be carried by
    // DIFFERENT raw axes. If the same one dominates both, the taps did not
    // describe a rectangle - a dangling touch line or four taps in roughly
    // the same place - and every number after this would be nonsense.
    bool axes_consistent = swap_xy ? (absolute(v_spread_x) > absolute(v_spread_y))
                                   : (absolute(v_spread_y) > absolute(v_spread_x));
    if (!axes_consistent) {
        show_result(true);
        return;
    }

    const uint16_t *horizontal = swap_xy ? sampled_y : sampled_x;
    const uint16_t *vertical = swap_xy ? sampled_x : sampled_y;

    int32_t left = ((int32_t) horizontal[TAP_LEFT_A] + horizontal[TAP_LEFT_B]) / 2;
    int32_t right = ((int32_t) horizontal[TAP_RIGHT_A] + horizontal[TAP_RIGHT_B]) / 2;
    int32_t top = ((int32_t) vertical[TAP_TOP_A] + vertical[TAP_TOP_B]) / 2;
    int32_t bottom = ((int32_t) vertical[TAP_BOTTOM_A] + vertical[TAP_BOTTOM_B]) / 2;

    int32_t x_span = right - left;
    int32_t y_span = bottom - top;

    // Plausibility, part two: too small a spread means the points were not
    // really hit, or a touch line is dangling.
    if (absolute(x_span) < 400 || absolute(y_span) < 400) {
        show_result(true);
        return;
    }

    int32_t x_pixels = POINT_X_RIGHT - POINT_X_LEFT;
    int32_t y_pixels = POINT_Y_BOTTOM - POINT_Y_TOP;

    xpt2046_calibration_t calibration = {
        .swap_xy = swap_xy,
        .raw_at_left = (int16_t) (left - x_span * POINT_X_LEFT / x_pixels),
        .raw_at_right =
            (int16_t) (left + x_span * (ILI9341_WIDTH - 1 - POINT_X_LEFT) / x_pixels),
        .raw_at_top = (int16_t) (top - y_span * POINT_Y_TOP / y_pixels),
        .raw_at_bottom =
            (int16_t) (top + y_span * (ILI9341_HEIGHT - 1 - POINT_Y_TOP) / y_pixels),
    };

    xpt2046_set_calibration(&calibration);

    // The ONLY flash write on this page, and it happens only after the
    // check above has passed.
    settings_store_save_calibration(&calibration);

    show_result(false);
}

static void capture_event_cb(lv_event_t *e)
{
    LV_UNUSED(e);

    if (!xpt2046_get_last_raw(&sampled_x[step], &sampled_y[step])) {
        return;
    }

    step++;

    if (step < TARGET_COUNT) {
        show_step();
        return;
    }

    step = 0;
    apply_calibration();
}

// A cross rather than a dot: the centre of two thin bars can be aimed at far
// more precisely than the middle of a filled circle, and the calibration is
// only as good as the aim. The ring around it marks the centre without
// covering it.
static lv_obj_t *create_cross(lv_obj_t *parent)
{
    lv_obj_t *cross = lv_obj_create(parent);
    ui_apply_transparent(cross);
    lv_obj_set_size(cross, CROSS_BOX, CROSS_BOX);

    for (uint8_t i = 0; i < 2; i++) {
        lv_obj_t *bar = lv_obj_create(cross);
        ui_apply_accent_fill(bar);
        lv_obj_set_size(bar, i == 0 ? CROSS_BOX : CROSS_THICK,
                        i == 0 ? CROSS_THICK : CROSS_BOX);
        lv_obj_center(bar);
        lv_obj_set_style_radius(bar, 0, 0);
        lv_obj_set_style_border_width(bar, 0, 0);
        lv_obj_clear_flag(bar, LV_OBJ_FLAG_CLICKABLE);
    }

    lv_obj_t *ring = lv_obj_create(cross);
    ui_apply_flat(ring);
    lv_obj_set_size(ring, 2 * CROSS_RADIUS, 2 * CROSS_RADIUS);
    lv_obj_center(ring);
    lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(ring, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ring, 2, 0);
    ui_apply_nav(ring);
    lv_obj_set_style_bg_opa(ring, LV_OPA_TRANSP, 0);
    lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
    lv_obj_clear_flag(ring, LV_OBJ_FLAG_CLICKABLE);

    return cross;
}

static void create_result_panel(lv_obj_t *screen)
{
    result_panel = lv_obj_create(screen);
    ui_apply_screen(result_panel);
    lv_obj_set_size(result_panel, UI_SCREEN_W, UI_SCREEN_H - UI_HEADER_H);
    lv_obj_set_pos(result_panel, 0, UI_HEADER_H);
    lv_obj_clear_flag(result_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(result_panel, LV_OBJ_FLAG_HIDDEN);

    result_icon = ui_label(result_panel, UI_FONT_28, 0, 0);
    ui_apply_accent_label(result_icon);
    lv_obj_align(result_icon, LV_ALIGN_TOP_MID, 0, 14);

    result_title = ui_label(result_panel, UI_FONT_20, 0, 0);
    ui_apply_text(result_title);
    lv_obj_align(result_title, LV_ALIGN_TOP_MID, 0, 50);

    result_hint = ui_label(result_panel, UI_FONT_14, 0, 0);
    ui_apply_muted(result_hint);
    lv_obj_align(result_hint, LV_ALIGN_TOP_MID, 0, 76);

    lv_obj_t *button = lv_btn_create(result_panel);
    ui_apply_nav(button);
    lv_obj_set_size(button, 160, UI_MIN_TOUCH_PX);
    lv_obj_align(button, LV_ALIGN_BOTTOM_MID, 0, -14);
    lv_obj_add_event_cb(button, result_button_cb, LV_EVENT_CLICKED, NULL);

    result_button_label = lv_label_create(button);
    lv_obj_set_style_text_font(result_button_label, UI_FONT_20, 0);
    ui_apply_accent_label(result_button_label);
    lv_obj_center(result_button_label);
}

void ui_calibration_create(void)
{
    lv_obj_t *screen = ui_screen_create(UI_SCREEN_CALIBRATION);
    // Back cancels and keeps the stored values. It returns to Settings,
    // which is where the calibration is now reached from - the menu tile it
    // used to have is the Functions tile.
    ui_header_create(screen, "Calibration", UI_SCREEN_SETTINGS);

    instruction_label = ui_label(screen, UI_FONT_16, 0, 0);
    ui_apply_text(instruction_label);
    lv_obj_set_width(instruction_label, UI_SCREEN_W);
    lv_obj_set_style_text_align(instruction_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(instruction_label, "Tap the center of the cross.");
    lv_obj_align(instruction_label, LV_ALIGN_TOP_MID, 0, UI_HEADER_H + 50);

    progress_label = ui_label(screen, UI_FONT_16, 0, 0);
    ui_apply_muted(progress_label);
    lv_obj_set_width(progress_label, UI_SCREEN_W);
    lv_obj_set_style_text_align(progress_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(progress_label, LV_ALIGN_TOP_MID, 0, UI_HEADER_H + 74);

    marker = create_cross(screen);

    // Catches every tap in the measurement area, created before the result
    // panel and therefore under it. The specification asks for a 48x48 area
    // around the cross; this is larger on purpose, and that is the safe
    // direction: with no stored calibration at all the mapped coordinate is
    // meaningless, so a small hit area would make the FIRST calibration -
    // the one that matters most - impossible to complete. The raw value is
    // what gets sampled either way, so a bigger overlay costs nothing.
    capture_overlay = lv_obj_create(screen);
    ui_apply_transparent(capture_overlay);
    lv_obj_set_size(capture_overlay, UI_SCREEN_W, UI_SCREEN_H - UI_HEADER_H);
    lv_obj_set_pos(capture_overlay, 0, UI_HEADER_H);
    lv_obj_add_flag(capture_overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(capture_overlay, capture_event_cb, LV_EVENT_CLICKED, NULL);

    create_result_panel(screen);
}

void ui_calibration_restart(void)
{
    step = 0;
    show_step();
}
