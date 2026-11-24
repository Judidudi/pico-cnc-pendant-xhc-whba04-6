#include "ui_internal.h"

#include "pico/stdlib.h"

#include "whb04b_codes.h"

// ---------------------------------------------------------------------
// Screen Main: status bar (40 px), coordinates (144 px), footer (56 px).
//
// The axis rotary switch decides how the coordinate area looks: X, Y or Z
// shows three 48 px rows, A shows one 96 px row. Both layouts exist from the
// start and only one of them is visible, which is cheaper and far less
// error-prone than rebuilding widgets on every switch movement.
//
// Which host slot holds which value - checked in pendant.cc,
// Display::updateData(): the host keeps two axis groups. For X/Y/Z it fills
// row1/row2/row3 with X, Y, Z; for anything else (A included) it fills the
// SAME three slots with A, B, C. So in the single-axis view the A value sits
// in slot 0, the very slot that carries X otherwise.
//
// Nothing is cached across a group change: the values of the group that is
// not selected are not being updated by the host, so showing them would mean
// showing stale numbers.
// ---------------------------------------------------------------------

#define DRO_ROWS   3
#define DRO_TOP    UI_STATUS_BAR_H
#define DRO_AREA_H (DRO_ROWS * UI_DRO_ROW_H)
#define FOOTER_TOP (UI_STATUS_BAR_H + DRO_AREA_H)

#define PAD_SIDE   12
#define LETTER_BOX 40
#define LETTER_GAP 12
#define SINGLE_BOX 64

#define MENU_BTN_W 80
#define STATUS_DOT 10

// Three equally spaced cells between the left padding and the Menu button
#define CELL_COUNT 3
#define CELL_SPAN  (UI_SCREEN_W - 8 - MENU_BTN_W - PAD_SIDE)
#define CELL_X(i)  (PAD_SIDE + ((CELL_SPAN) * (i)) / CELL_COUNT)

static const char *const axis_row_name[DRO_ROWS] = { "X", "Y", "Z" };
static const uint8_t axis_row_code[DRO_ROWS] = { WHB04B_AXIS_X, WHB04B_AXIS_Y,
                                                 WHB04B_AXIS_Z };

static lv_obj_t *status_bar;
static lv_obj_t *machine_on_group;
static lv_obj_t *machine_off_badge;
static lv_obj_t *mode_label;
static lv_obj_t *coord_mode_label;

// Shown INSTEAD of everything else in the status bar when the host has gone
// quiet or USB is unplugged.
static lv_obj_t *fault_label;

static lv_obj_t *dro_row[DRO_ROWS];
static lv_obj_t *dro_letter_box[DRO_ROWS];
static lv_obj_t *dro_letter[DRO_ROWS];
static lv_obj_t *dro_value[DRO_ROWS];

static lv_obj_t *single_row;
static lv_obj_t *single_value;

static lv_obj_t *feed_value;
static lv_obj_t *spindle_value;
static lv_obj_t *step_caption;
static lv_obj_t *step_value;

static void menu_event_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    ui_goto(UI_SCREEN_MENU);
}

static lv_obj_t *bare_container(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                                lv_coord_t w, lv_coord_t h)
{
    lv_obj_t *obj = lv_obj_create(parent);
    ui_apply_transparent(obj);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_pos(obj, x, y);

    return obj;
}

static void create_status_bar(lv_obj_t *screen)
{
    // The status bar is the main screen's header, so it carries the same
    // surface and the same 2 px rule as every other page.
    lv_obj_t *bar = lv_obj_create(screen);
    ui_apply_header(bar);
    lv_obj_set_size(bar, UI_SCREEN_W, UI_STATUS_BAR_H);
    lv_obj_set_pos(bar, 0, 0);
    status_bar = bar;

    // Machine on: a small filled dot plus the words. Machine off: the badge.
    // Exactly one of the two is visible, so the left side never goes blank
    // and the state reads at a glance from across the machine.
    machine_on_group = bare_container(bar, PAD_SIDE, 0, 150, UI_STATUS_BAR_H);

    lv_obj_t *dot = lv_obj_create(machine_on_group);
    ui_apply_flat(dot);
    lv_obj_set_size(dot, STATUS_DOT, STATUS_DOT);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(dot, 0, 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    lv_obj_align(dot, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_clear_flag(dot, LV_OBJ_FLAG_CLICKABLE);
    // The dot is a filled shape, so it takes the TEXT colour as a background
    // and still follows a theme change.
    ui_apply_dot(dot); /* fill takes the TEXT colour, follows the theme */

    lv_obj_t *on_label = ui_label(machine_on_group, UI_FONT_16, 0, 0);
    ui_apply_text(on_label);
    lv_label_set_text(on_label, "MACHINE ON");
    lv_obj_align(on_label, LV_ALIGN_LEFT_MID, STATUS_DOT + 8, 0);

    machine_off_badge = lv_obj_create(bar);
    ui_apply_accent_fill(machine_off_badge);
    lv_obj_set_size(machine_off_badge, LV_SIZE_CONTENT, 24);
    lv_obj_set_style_pad_hor(machine_off_badge, 8, 0);
    lv_obj_align(machine_off_badge, LV_ALIGN_LEFT_MID, PAD_SIDE, 0);
    lv_obj_add_flag(machine_off_badge, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(machine_off_badge, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *badge_label = lv_label_create(machine_off_badge);
    lv_obj_set_style_text_font(badge_label, UI_FONT_14, 0);
    ui_apply_on_accent(badge_label);
    lv_label_set_text(badge_label, "MACHINE OFF");
    lv_obj_center(badge_label);

    // Two labels rather than one with lv_label_set_recolor(): a colour
    // written into the text would be a hardcoded value that ignores a theme
    // change. The muted part is placed first so the mode can hang off it.
    coord_mode_label = ui_label(bar, UI_FONT_16, 0, 0);
    ui_apply_muted(coord_mode_label);
    lv_obj_align(coord_mode_label, LV_ALIGN_RIGHT_MID, -PAD_SIDE, 0);

    mode_label = ui_label(bar, UI_FONT_16, 0, 0);
    ui_apply_text(mode_label);

    // Created last so it draws over the rest of the bar. It is a plain
    // label; the red surface is the bar itself, switched on with
    // ui_set_error_fill() so the whole strip turns red rather than a box
    // inside it.
    fault_label = ui_label(bar, UI_FONT_16, 0, 0);
    ui_apply_on_error(fault_label);
    lv_obj_align(fault_label, LV_ALIGN_LEFT_MID, PAD_SIDE, 0);
    lv_obj_add_flag(fault_label, LV_OBJ_FLAG_HIDDEN);
}

static void create_dro(lv_obj_t *screen)
{
    for (uint8_t row = 0; row < DRO_ROWS; row++) {
        lv_coord_t y = DRO_TOP + row * UI_DRO_ROW_H;

        dro_row[row] = lv_obj_create(screen);
        ui_apply_transparent(dro_row[row]);
        lv_obj_set_size(dro_row[row], UI_SCREEN_W, UI_DRO_ROW_H);
        lv_obj_set_pos(dro_row[row], 0, y);

        dro_letter_box[row] = lv_obj_create(dro_row[row]);
        ui_apply_transparent(dro_letter_box[row]);
        lv_obj_set_size(dro_letter_box[row], LETTER_BOX, LETTER_BOX);
        lv_obj_align(dro_letter_box[row], LV_ALIGN_LEFT_MID, PAD_SIDE, 0);
        lv_obj_set_style_radius(dro_letter_box[row], 6, 0);

        dro_letter[row] = lv_label_create(dro_letter_box[row]);
        lv_obj_set_style_text_font(dro_letter[row], UI_FONT_28, 0);
        // Muted underneath; the highlight lays the accent text colour on top
        ui_apply_muted(dro_letter[row]);
        lv_label_set_text(dro_letter[row], axis_row_name[row]);
        lv_obj_center(dro_letter[row]);

        dro_value[row] = lv_label_create(dro_row[row]);
        lv_obj_set_style_text_font(dro_value[row], UI_FONT_NUM_LARGE, 0);
        ui_apply_text(dro_value[row]);
        lv_obj_set_style_text_align(dro_value[row], LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_set_width(dro_value[row],
                         UI_SCREEN_W - (PAD_SIDE + LETTER_BOX + LETTER_GAP) - PAD_SIDE);
        lv_obj_align(dro_value[row], LV_ALIGN_RIGHT_MID, -PAD_SIDE, 0);
        lv_label_set_text(dro_value[row], "---");
    }

    // Single-axis view, vertically centred in the same 144 px area
    single_row = lv_obj_create(screen);
    ui_apply_panel_active(single_row);
    lv_obj_set_size(single_row, UI_SCREEN_W, UI_DRO_SINGLE_H);
    lv_obj_set_pos(single_row, 0, DRO_TOP + (DRO_AREA_H - UI_DRO_SINGLE_H) / 2);
    lv_obj_set_style_radius(single_row, 0, 0);
    lv_obj_set_style_pad_all(single_row, 0, 0);
    lv_obj_clear_flag(single_row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(single_row, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *single_box = lv_obj_create(single_row);
    ui_apply_accent_fill(single_box);
    lv_obj_set_size(single_box, SINGLE_BOX, SINGLE_BOX);
    lv_obj_align(single_box, LV_ALIGN_LEFT_MID, PAD_SIDE, 0);
    lv_obj_set_style_pad_all(single_box, 0, 0);
    lv_obj_clear_flag(single_box, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *single_letter = lv_label_create(single_box);
    lv_obj_set_style_text_font(single_letter, UI_FONT_44, 0);
    ui_apply_on_accent(single_letter);
    lv_label_set_text(single_letter, "A");
    lv_obj_center(single_letter);

    single_value = lv_label_create(single_row);
    lv_obj_set_style_text_font(single_value, UI_FONT_NUM_HUGE, 0);
    ui_apply_text(single_value);
    lv_obj_set_style_text_align(single_value, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_width(single_value,
                     UI_SCREEN_W - (PAD_SIDE + SINGLE_BOX + LETTER_GAP) - PAD_SIDE);
    lv_obj_align(single_value, LV_ALIGN_RIGHT_MID, -PAD_SIDE, 0);
    lv_label_set_text(single_value, "---");
}

static lv_obj_t *create_cell(lv_obj_t *footer, uint8_t index, const char *caption,
                             lv_obj_t **caption_out)
{
    lv_obj_t *label = ui_label(footer, UI_FONT_12, CELL_X(index), 6);
    ui_apply_muted(label);
    lv_label_set_text(label, caption);
    if (caption_out != NULL) {
        *caption_out = label;
    }

    lv_obj_t *value = ui_label(footer, UI_FONT_NUM_FOOTER, CELL_X(index), 24);
    ui_apply_text(value);
    lv_label_set_text(value, "--");

    return value;
}

static void create_footer(lv_obj_t *screen)
{
    lv_obj_t *footer = bare_container(screen, 0, FOOTER_TOP, UI_SCREEN_W, UI_FOOTER_H);

    feed_value = create_cell(footer, 0, "FEED", NULL);
    spindle_value = create_cell(footer, 1, "SPINDLE", NULL);
    step_value = create_cell(footer, 2, "STEP mm", &step_caption);

    lv_obj_t *menu = lv_btn_create(footer);
    ui_apply_accent_button(menu);
    lv_obj_set_size(menu, MENU_BTN_W, UI_MIN_TOUCH_PX);
    lv_obj_align(menu, LV_ALIGN_RIGHT_MID, -8, 0);
    lv_obj_add_event_cb(menu, menu_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *menu_label = lv_label_create(menu);
    lv_obj_set_style_text_font(menu_label, UI_FONT_20, 0);
    ui_apply_accent_label(menu_label);
    lv_label_set_text(menu_label, LV_SYMBOL_BARS " Menu");
    lv_obj_center(menu_label);
}

void ui_main_create(void)
{
    lv_obj_t *screen = ui_screen_create(UI_SCREEN_MAIN);

    create_status_bar(screen);
    create_dro(screen);
    create_footer(screen);
}

// ---------------------------------------------------------------------
// Updating.
//
// Every label is only rewritten on an actual change of value.
// lv_label_set_text_fmt() does NOT compare the text, it always invalidates,
// and each invalidation drags the redraw and transfer of a whole 30-line
// band behind it.
// ---------------------------------------------------------------------
static void format_coordinate(lv_obj_t *label, const pendant_coordinate_t *c, bool valid)
{
    if (!valid) {
        lv_label_set_text(label, "---");
    } else {
        lv_label_set_text_fmt(label, "%s%u.%04u", c->negative ? "-" : "", c->integer_part,
                              c->fraction_part);
    }
}

// STEP comes from the rotary switch on the pendant, not from the host, so
// it stays readable even when the host has gone silent - which is why it
// lives in its own function the fault path can call too.
//
// It only means something in STEP and MPG mode. In continuous and percent
// mode the switch position selects something else, so a figure there would
// be a lie.
static void update_step_width(const pendant_state_t *state,
                              const pendant_display_t *display, bool force)
{
    static uint8_t cached_feed_code = 0xFF, cached_mode_for_width = 0xFF;

    if (force || state->feed_selected != cached_feed_code ||
        display->step_mode != cached_mode_for_width) {
        cached_feed_code = state->feed_selected;
        cached_mode_for_width = display->step_mode;
        bool has_width = (display->step_mode == 1 || display->step_mode == 2);
        lv_label_set_text(step_value,
                          has_width ? ui_step_width_text(state->feed_selected) : "--");
    }
}

static void update_axis_highlight(uint8_t row, bool active)
{
    ui_set_axis_active(dro_row[row], dro_letter_box[row], dro_letter[row], active);
}

// ---------------------------------------------------------------------
// Missing host data.
//
// Two different faults, one appearance: USB not enumerated at all, or
// enumerated but no display packet for longer than HOST_DATA_TIMEOUT_MS
// (T_NOHOST, in pendant_config.h). USB is checked first because it is the
// more fundamental of the two - with the cable out there is no host to be
// silent.
//
// The point of the state is that NO stale numbers are left on screen. A
// coordinate that stopped updating two minutes ago looks exactly like a
// live one, and at a milling machine that is the dangerous kind of wrong.
// Everything the host feeds is replaced by dashes in the muted colour;
// STEP stays, because it comes from the rotary switch on the pendant and is
// still true.
//
// This is INFORMATION ONLY. The heartbeat, the monostable and the relay are
// untouched by it and run on their own.
// ---------------------------------------------------------------------
static const char *fault_text(const pendant_state_t *state)
{
    if (!state->usb_link_ok) {
        return LV_SYMBOL_WARNING "  USB NOT CONNECTED";
    }
    if (!state->host_data_ok) {
        return LV_SYMBOL_WARNING "  NO HOST DATA";
    }
    return NULL;
}

static void set_fault_visible(bool fault, const char *text)
{
    ui_set_error_fill(status_bar, fault);

    if (fault) {
        lv_label_set_text(fault_label, text);
        lv_obj_clear_flag(fault_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(machine_on_group, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(machine_off_badge, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(mode_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(coord_mode_label, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(fault_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(mode_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(coord_mode_label, LV_OBJ_FLAG_HIDDEN);
    }

    // The value labels grey out together with the dashes, so a reading that
    // is not current can never be mistaken for one that is.
    for (uint8_t row = 0; row < DRO_ROWS; row++) {
        ui_set_muted(dro_value[row], fault);
    }
    ui_set_muted(single_value, fault);
    ui_set_muted(feed_value, fault);
    ui_set_muted(spindle_value, fault);
}

void ui_main_update(void)
{
    const pendant_state_t *state = pendant_state();
    const pendant_display_t *display = &state->display;

    static bool primed = false;
    bool force = !primed;
    primed = true;

    // --- missing host data ---
    const char *fault = fault_text(state);
    static const char *cached_fault = (const char *) 1;  // never a real value
    bool fault_changed = force || fault != cached_fault;
    if (fault_changed) {
        cached_fault = fault;
        set_fault_visible(fault != NULL, fault);
    }

    if (fault != NULL) {
        // Dashes instead of the last known numbers. Written once per entry
        // into the state, not on every pass: lv_label_set_text() always
        // invalidates and would drag a full band redraw behind it at 30 Hz.
        if (fault_changed) {
            for (uint8_t row = 0; row < DRO_ROWS; row++) {
                lv_label_set_text(dro_value[row], "---.---");
            }
            lv_label_set_text(single_value, "---.---");
            lv_label_set_text(feed_value, "---");
            lv_label_set_text(spindle_value, "---");
        }
        update_step_width(state, display, force || fault_changed);
        return;
    }

    // Coming BACK from the fault state every value on screen is a dash, and
    // the caches still hold the numbers that were there before. Without this
    // an unchanged value would compare equal and never be redrawn - the
    // dashes would simply stay.
    force = force || fault_changed;

    // --- status bar ---
    // The host fills this bit from "machine is on", so the badge appears on
    // the inverted condition - and until display data has arrived we do not
    // know, which counts as off.
    static bool cached_machine_on = false;
    bool machine_on = display->valid && display->is_reset;
    if (force || machine_on != cached_machine_on) {
        cached_machine_on = machine_on;
        if (machine_on) {
            lv_obj_clear_flag(machine_on_group, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(machine_off_badge, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(machine_on_group, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(machine_off_badge, LV_OBJ_FLAG_HIDDEN);
        }
    }

    static uint8_t cached_step_mode = 0xFF;
    static bool cached_relative = false;
    if (force || display->step_mode != cached_step_mode ||
        display->is_relative != cached_relative) {
        cached_step_mode = display->step_mode;
        cached_relative = display->is_relative;

        lv_label_set_text_fmt(coord_mode_label, " " UI_SEPARATOR_DOT " %s",
                              display->is_relative ? "REL" : "ABS");
        lv_label_set_text(mode_label, ui_step_mode_text(display->step_mode));
        lv_obj_align_to(mode_label, coord_mode_label, LV_ALIGN_OUT_LEFT_MID, 0, 0);
    }

    // --- coordinates ---
    bool single_axis = (state->axis_selected == WHB04B_AXIS_A);

    static bool cached_single_axis = false;
    bool group_changed = force || single_axis != cached_single_axis;
    if (group_changed) {
        cached_single_axis = single_axis;
        for (uint8_t row = 0; row < DRO_ROWS; row++) {
            if (single_axis) {
                lv_obj_add_flag(dro_row[row], LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_clear_flag(dro_row[row], LV_OBJ_FLAG_HIDDEN);
            }
        }
        if (single_axis) {
            lv_obj_clear_flag(single_row, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(single_row, LV_OBJ_FLAG_HIDDEN);
        }
    }

    static bool cached_valid;
    static pendant_coordinate_t cached_coordinate[DRO_ROWS];
    static uint8_t cached_axis = 0xFF;

    if (single_axis) {
        // Slot 0 carries A in the ABC group - see the note at the top.
        const pendant_coordinate_t *now = &display->coordinate[0];
        bool changed = group_changed || display->valid != cached_valid ||
                       now->negative != cached_coordinate[0].negative ||
                       now->integer_part != cached_coordinate[0].integer_part ||
                       now->fraction_part != cached_coordinate[0].fraction_part;
        if (changed) {
            cached_coordinate[0] = *now;
            format_coordinate(single_value, now, display->valid);
        }
    } else {
        for (uint8_t row = 0; row < DRO_ROWS; row++) {
            const pendant_coordinate_t *now = &display->coordinate[row];
            bool changed = group_changed || display->valid != cached_valid ||
                           now->negative != cached_coordinate[row].negative ||
                           now->integer_part != cached_coordinate[row].integer_part ||
                           now->fraction_part != cached_coordinate[row].fraction_part;

            if (changed) {
                cached_coordinate[row] = *now;
                format_coordinate(dro_value[row], now, display->valid);
            }

            if (group_changed || state->axis_selected != cached_axis) {
                update_axis_highlight(row, state->axis_selected == axis_row_code[row]);
            }
        }
    }
    cached_valid = display->valid;
    cached_axis = state->axis_selected;

    // --- footer ---
    // FEED is the feed OVERRIDE in percent, hence the percent sign. SPINDLE
    // has no unit on purpose: the host puts the spindle override in percent
    // there, except while a speed change is running, when the same field
    // carries RPM instead - and the packet has no flag saying which.
    static uint16_t cached_feed_rate = 0xFFFF, cached_spindle = 0xFFFF;
    static bool cached_footer_valid;
    if (force || display->valid != cached_footer_valid ||
        display->feed_rate != cached_feed_rate ||
        display->spindle_feed_rate != cached_spindle) {
        cached_footer_valid = display->valid;
        cached_feed_rate = display->feed_rate;
        cached_spindle = display->spindle_feed_rate;

        if (!display->valid) {
            lv_label_set_text(feed_value, "--");
            lv_label_set_text(spindle_value, "--");
        } else {
            lv_label_set_text_fmt(feed_value, "%u %%", display->feed_rate);
            lv_label_set_text_fmt(spindle_value, "%u", display->spindle_feed_rate);
        }
    }

    // The unit is a LABEL only - nothing is converted. Its own cache because
    // it changes from the settings screen, not from host data.
    static uint8_t cached_unit = 0xFF;
    if (force || pendant_settings()->unit_inch != cached_unit) {
        cached_unit = pendant_settings()->unit_inch;
        lv_label_set_text(step_caption, cached_unit ? "STEP inch" : "STEP mm");
    }

    update_step_width(state, display, force);
}
