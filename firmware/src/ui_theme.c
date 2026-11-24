#include "ui_theme.h"

// ---------------------------------------------------------------------
// The two palettes and the shared styles built from them.
//
// Both accents read well as a line, as text and as a fill on their own
// background - amber on near-black, blue on near-white. The earlier
// "stroke" and "on light" variants existed only because the light theme
// used the amber accent as well, where a #FFB224 line all but vanished on
// #F4F6F8. With a blue light accent that problem is gone and there is one
// accent per theme again.
// ---------------------------------------------------------------------
typedef struct {
    uint32_t bg;
    uint32_t panel;
    uint32_t border;
    uint32_t nav;
    uint32_t text;
    uint32_t muted;
    uint32_t accent;
    uint32_t accent_text;
    uint32_t header_bg;
    uint32_t header_line;
    uint32_t separator;
} ui_palette_t;

static const ui_palette_t palettes[UI_THEME_COUNT] = {
    [UI_THEME_DARK] = {
        .bg          = 0x0A0E13,
        .panel       = 0x131A22,
        .border      = 0x3A4856,
        .nav         = 0x1B2430,
        .text        = 0xEAF0F6,
        .muted       = 0x9AA8B6,
        .accent      = 0xFFB224,
        .accent_text = 0x0A0E13,
        .header_bg   = 0x18212C,
        .header_line = 0x3A4856,
        .separator   = 0x1C2530,
    },
    [UI_THEME_LIGHT] = {
        .bg          = 0xF4F6F8,
        .panel       = 0xFFFFFF,
        .border      = 0xB8C2CC,
        .nav         = 0xFFFFFF,
        .text        = 0x0F1720,
        .muted       = 0x46525F,
        .accent      = 0x0A5BD3,
        .accent_text = 0xFFFFFF,
        .header_bg   = 0xE1E8EF,
        .header_line = 0x9FADBB,
        // No light separator is specified; the border colour is the closest
        // thing that still reads as a hairline on white.
        .separator   = 0xB8C2CC,
    },
};

// Identical in both themes: an error has to look the same wherever it
// appears, and both of these sit on their own fill anyway.
#define ERROR_FILL 0xC62828
#define ERROR_LINE 0x8E1C1C
#define ERROR_TEXT 0xFFFFFF

#define SCROLLBAR_COLOUR 0x5B6B7B

// Must match the default in pendant_settings_set_defaults(). ui_init() runs
// before the stored record is read, so this is the palette of the very first
// frame; with the wrong value the device would flash the other theme for a
// moment at every boot.
static ui_theme_t active_theme = UI_THEME_LIGHT;

static lv_style_t s_flat;
static lv_style_t s_screen;
static lv_style_t s_transparent;
static lv_style_t s_header;
static lv_style_t s_panel;
static lv_style_t s_nav;
static lv_style_t s_panel_active;
static lv_style_t s_selected;
static lv_style_t s_accent_fill;
static lv_style_t s_accent_button;
static lv_style_t s_separator;
static lv_style_t s_tile;
static lv_style_t s_tile_pressed;
static lv_style_t s_error_fill;
static lv_style_t s_text;
static lv_style_t s_muted;
static lv_style_t s_accent_label;
static lv_style_t s_on_accent;
static lv_style_t s_on_error;
static lv_style_t s_scrollbar;
static lv_style_t s_dot;
static lv_style_t s_row_active;
static lv_style_t s_box_active;

static void fill_styles(const ui_palette_t *p)
{
    lv_style_set_bg_color(&s_screen, lv_color_hex(p->bg));

    lv_style_set_bg_color(&s_header, lv_color_hex(p->header_bg));
    lv_style_set_border_color(&s_header, lv_color_hex(p->header_line));

    lv_style_set_bg_color(&s_panel, lv_color_hex(p->panel));
    lv_style_set_border_color(&s_panel, lv_color_hex(p->border));

    lv_style_set_bg_color(&s_nav, lv_color_hex(p->nav));
    lv_style_set_border_color(&s_nav, lv_color_hex(p->accent));

    lv_style_set_bg_color(&s_panel_active, lv_color_hex(p->nav));

    lv_style_set_bg_color(&s_selected, lv_color_hex(p->nav));
    lv_style_set_border_color(&s_selected, lv_color_hex(p->accent));

    lv_style_set_bg_color(&s_accent_fill, lv_color_hex(p->accent));
    lv_style_set_border_color(&s_accent_fill, lv_color_hex(p->accent));

    lv_style_set_bg_color(&s_accent_button, lv_color_hex(p->nav));
    lv_style_set_border_color(&s_accent_button, lv_color_hex(p->accent));

    lv_style_set_border_color(&s_separator, lv_color_hex(p->separator));

    lv_style_set_bg_color(&s_tile, lv_color_hex(p->panel));
    lv_style_set_border_color(&s_tile, lv_color_hex(p->border));
    lv_style_set_text_color(&s_tile, lv_color_hex(p->text));

    lv_style_set_bg_color(&s_tile_pressed, lv_color_hex(p->accent));
    lv_style_set_border_color(&s_tile_pressed, lv_color_hex(p->accent));
    lv_style_set_text_color(&s_tile_pressed, lv_color_hex(p->accent_text));

    lv_style_set_text_color(&s_text, lv_color_hex(p->text));
    lv_style_set_text_color(&s_muted, lv_color_hex(p->muted));
    lv_style_set_text_color(&s_accent_label, lv_color_hex(p->accent));
    lv_style_set_text_color(&s_on_accent, lv_color_hex(p->accent_text));

    lv_style_set_bg_color(&s_dot, lv_color_hex(p->text));
    lv_style_set_bg_color(&s_row_active, lv_color_hex(p->nav));
    lv_style_set_bg_color(&s_box_active, lv_color_hex(p->accent));
}

void ui_theme_init(void)
{
    lv_style_init(&s_flat);
    lv_style_set_shadow_width(&s_flat, 0);
    lv_style_set_bg_grad_dir(&s_flat, LV_GRAD_DIR_NONE);

    lv_style_init(&s_screen);
    lv_style_set_bg_opa(&s_screen, LV_OPA_COVER);
    lv_style_set_border_width(&s_screen, 0);
    lv_style_set_pad_all(&s_screen, 0);
    lv_style_set_radius(&s_screen, 0);

    lv_style_init(&s_transparent);
    lv_style_set_bg_opa(&s_transparent, LV_OPA_TRANSP);
    lv_style_set_border_width(&s_transparent, 0);
    lv_style_set_pad_all(&s_transparent, 0);
    lv_style_set_radius(&s_transparent, 0);

    // The rule under the header is the bottom border of the surface itself,
    // not a separate object: one fewer widget on every page, and the two can
    // never drift apart.
    lv_style_init(&s_header);
    lv_style_set_bg_opa(&s_header, LV_OPA_COVER);
    lv_style_set_border_width(&s_header, UI_HEADER_LINE_H);
    lv_style_set_border_side(&s_header, LV_BORDER_SIDE_BOTTOM);
    lv_style_set_border_opa(&s_header, LV_OPA_COVER);
    lv_style_set_radius(&s_header, 0);
    lv_style_set_pad_all(&s_header, 0);

    lv_style_init(&s_panel);
    lv_style_set_bg_opa(&s_panel, LV_OPA_COVER);
    lv_style_set_border_width(&s_panel, 1);
    lv_style_set_border_opa(&s_panel, LV_OPA_COVER);
    lv_style_set_radius(&s_panel, 8);

    lv_style_init(&s_nav);
    lv_style_set_bg_opa(&s_nav, LV_OPA_COVER);
    lv_style_set_border_width(&s_nav, 1);
    lv_style_set_border_opa(&s_nav, LV_OPA_COVER);
    lv_style_set_radius(&s_nav, 8);

    lv_style_init(&s_panel_active);
    lv_style_set_bg_opa(&s_panel_active, LV_OPA_COVER);
    lv_style_set_border_width(&s_panel_active, 0);

    lv_style_init(&s_selected);
    lv_style_set_bg_opa(&s_selected, LV_OPA_COVER);
    lv_style_set_border_width(&s_selected, 2);
    lv_style_set_border_opa(&s_selected, LV_OPA_COVER);
    lv_style_set_radius(&s_selected, 8);

    lv_style_init(&s_accent_fill);
    lv_style_set_bg_opa(&s_accent_fill, LV_OPA_COVER);
    lv_style_set_border_width(&s_accent_fill, 1);
    lv_style_set_border_opa(&s_accent_fill, LV_OPA_COVER);
    lv_style_set_radius(&s_accent_fill, 8);

    lv_style_init(&s_accent_button);
    lv_style_set_bg_opa(&s_accent_button, LV_OPA_COVER);
    lv_style_set_border_width(&s_accent_button, 1);
    lv_style_set_border_opa(&s_accent_button, LV_OPA_COVER);
    lv_style_set_radius(&s_accent_button, 8);

    lv_style_init(&s_separator);
    lv_style_set_border_width(&s_separator, 1);
    lv_style_set_border_side(&s_separator, LV_BORDER_SIDE_BOTTOM);
    lv_style_set_border_opa(&s_separator, LV_OPA_COVER);

    lv_style_init(&s_tile);
    lv_style_set_bg_opa(&s_tile, LV_OPA_COVER);
    lv_style_set_border_width(&s_tile, 1);
    lv_style_set_border_opa(&s_tile, LV_OPA_COVER);
    lv_style_set_radius(&s_tile, 8);

    lv_style_init(&s_tile_pressed);
    lv_style_set_bg_opa(&s_tile_pressed, LV_OPA_COVER);
    lv_style_set_border_width(&s_tile_pressed, 1);
    lv_style_set_border_opa(&s_tile_pressed, LV_OPA_COVER);
    lv_style_set_radius(&s_tile_pressed, 8);

    // The only colours in this file that do not come from a palette.
    lv_style_init(&s_error_fill);
    lv_style_set_bg_opa(&s_error_fill, LV_OPA_COVER);
    lv_style_set_bg_color(&s_error_fill, lv_color_hex(ERROR_FILL));
    lv_style_set_border_width(&s_error_fill, 1);
    lv_style_set_border_opa(&s_error_fill, LV_OPA_COVER);
    lv_style_set_border_color(&s_error_fill, lv_color_hex(ERROR_LINE));
    lv_style_set_radius(&s_error_fill, 0);

    lv_style_init(&s_on_error);
    lv_style_set_text_color(&s_on_error, lv_color_hex(ERROR_TEXT));

    lv_style_init(&s_text);
    lv_style_init(&s_muted);
    lv_style_init(&s_accent_label);
    lv_style_init(&s_on_accent);

    lv_style_init(&s_dot);
    lv_style_set_bg_opa(&s_dot, LV_OPA_COVER);
    lv_style_set_border_width(&s_dot, 0);
    lv_style_set_radius(&s_dot, LV_RADIUS_CIRCLE);

    lv_style_init(&s_row_active);
    lv_style_set_bg_opa(&s_row_active, LV_OPA_COVER);

    lv_style_init(&s_box_active);
    lv_style_set_bg_opa(&s_box_active, LV_OPA_COVER);
    lv_style_set_radius(&s_box_active, 6);

    lv_style_init(&s_scrollbar);
    lv_style_set_width(&s_scrollbar, UI_SCROLLBAR_W);
    lv_style_set_bg_color(&s_scrollbar, lv_color_hex(SCROLLBAR_COLOUR));
    lv_style_set_bg_opa(&s_scrollbar, LV_OPA_COVER);
    lv_style_set_radius(&s_scrollbar, 4);

    fill_styles(&palettes[active_theme]);
}

void ui_theme_set(ui_theme_t theme)
{
    if (theme >= UI_THEME_COUNT || theme == active_theme) {
        return;
    }

    active_theme = theme;
    fill_styles(&palettes[theme]);

    // NULL means "every object that uses any style": LVGL walks the tree and
    // invalidates what is affected. This is what makes the switch work
    // without rebuilding a single screen.
    lv_obj_report_style_change(NULL);
}

void ui_apply_flat(lv_obj_t *obj)
{
    lv_obj_add_style(obj, &s_flat, 0);
}
void ui_apply_scrollbar(lv_obj_t *obj)
{
    lv_obj_add_style(obj, &s_scrollbar, LV_PART_SCROLLBAR);
}
void ui_apply_text(lv_obj_t *obj)
{
    lv_obj_add_style(obj, &s_text, 0);
}
void ui_apply_muted(lv_obj_t *obj)
{
    lv_obj_add_style(obj, &s_muted, 0);
}
void ui_apply_accent_label(lv_obj_t *obj)
{
    lv_obj_add_style(obj, &s_accent_label, 0);
}
void ui_apply_on_accent(lv_obj_t *obj)
{
    lv_obj_add_style(obj, &s_on_accent, 0);
}
void ui_apply_on_error(lv_obj_t *obj)
{
    lv_obj_add_style(obj, &s_on_error, 0);
}

void ui_apply_screen(lv_obj_t *obj)
{
    ui_apply_flat(obj);
    lv_obj_add_style(obj, &s_screen, 0);
}

void ui_apply_transparent(lv_obj_t *obj)
{
    ui_apply_flat(obj);
    lv_obj_add_style(obj, &s_transparent, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE);
}

void ui_apply_header(lv_obj_t *obj)
{
    ui_apply_flat(obj);
    lv_obj_add_style(obj, &s_header, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE);
}

void ui_apply_panel(lv_obj_t *obj)
{
    ui_apply_flat(obj);
    lv_obj_add_style(obj, &s_panel, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

void ui_apply_nav(lv_obj_t *obj)
{
    ui_apply_flat(obj);
    lv_obj_add_style(obj, &s_nav, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

void ui_apply_panel_active(lv_obj_t *obj)
{
    ui_apply_flat(obj);
    lv_obj_add_style(obj, &s_panel_active, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

void ui_apply_selected(lv_obj_t *obj)
{
    lv_obj_add_style(obj, &s_selected, 0);
}
void ui_clear_selected(lv_obj_t *obj)
{
    lv_obj_remove_style(obj, &s_selected, 0);
}

void ui_apply_tile(lv_obj_t *obj)
{
    ui_apply_flat(obj);
    lv_obj_add_style(obj, &s_tile, 0);
    lv_obj_add_style(obj, &s_tile_pressed, LV_STATE_PRESSED);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

void ui_apply_error_fill(lv_obj_t *obj)
{
    ui_apply_flat(obj);
    lv_obj_add_style(obj, &s_error_fill, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

void ui_set_error_fill(lv_obj_t *obj, bool on)
{
    if (on) {
        lv_obj_add_style(obj, &s_error_fill, 0);
    } else {
        lv_obj_remove_style(obj, &s_error_fill, 0);
    }
}

void ui_set_on_accent(lv_obj_t *obj, bool on)
{
    if (on) {
        lv_obj_add_style(obj, &s_on_accent, 0);
    } else {
        lv_obj_remove_style(obj, &s_on_accent, 0);
    }
}

void ui_set_muted(lv_obj_t *obj, bool on)
{
    if (on) {
        lv_obj_add_style(obj, &s_muted, 0);
    } else {
        lv_obj_remove_style(obj, &s_muted, 0);
    }
}

void ui_apply_dot(lv_obj_t *obj)
{
    ui_apply_flat(obj);
    lv_obj_add_style(obj, &s_dot, 0);
}

void ui_set_axis_active(lv_obj_t *row, lv_obj_t *box, lv_obj_t *letter, bool active)
{
    if (active) {
        lv_obj_add_style(row, &s_row_active, 0);
        lv_obj_add_style(box, &s_box_active, 0);
        lv_obj_add_style(letter, &s_on_accent, 0);
    } else {
        lv_obj_remove_style(row, &s_row_active, 0);
        lv_obj_remove_style(box, &s_box_active, 0);
        lv_obj_remove_style(letter, &s_on_accent, 0);
    }
}

void ui_apply_accent_fill(lv_obj_t *obj)
{
    ui_apply_flat(obj);
    lv_obj_add_style(obj, &s_accent_fill, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

void ui_set_accent_fill(lv_obj_t *obj, bool on)
{
    if (on) {
        lv_obj_add_style(obj, &s_accent_fill, 0);
    } else {
        lv_obj_remove_style(obj, &s_accent_fill, 0);
    }
}

void ui_apply_accent_button(lv_obj_t *obj)
{
    ui_apply_flat(obj);
    lv_obj_add_style(obj, &s_accent_button, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

void ui_apply_separator_row(lv_obj_t *obj)
{
    lv_obj_add_style(obj, &s_separator, 0);
}
