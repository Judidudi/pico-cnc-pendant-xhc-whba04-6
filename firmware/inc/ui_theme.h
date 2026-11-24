#ifndef UI_THEME_H
#define UI_THEME_H

#include "lvgl.h"

// ---------------------------------------------------------------------
// Colours, dimensions and font mapping of the user interface.
//
// Two palettes that can be swapped at RUNTIME. That is why no screen may
// set a colour of its own: everything goes through the shared styles below.
// Changing a palette rewrites those styles and calls
// lv_obj_report_style_change(), so every widget already on screen picks the
// new colours up without being rebuilt.
//
// The rule in one line: if a widget calls lv_obj_set_style_*_color()
// directly, it will NOT follow a theme change. Add a style here instead.
//
// The accent means one thing throughout: navigation (Back, More/Prev, the
// border of navigation elements), the active tab, a pressed tile and the
// current selection. It is amber on dark and blue on light - both read
// equally well as a line, as text and as a fill, which is why there are no
// longer separate "stroke" and "on light" variants.
// ---------------------------------------------------------------------

typedef enum { UI_THEME_DARK = 0, UI_THEME_LIGHT, UI_THEME_COUNT } ui_theme_t;

// Smallest edge of anything meant to be hit with a finger.
#define UI_MIN_TOUCH_PX 48

// Fixed geometry from the layout specification
#define UI_STATUS_BAR_H 40
#define UI_DRO_ROW_H    48
#define UI_DRO_SINGLE_H 96
#define UI_FOOTER_H     56
#define UI_HEADER_H     56
#define UI_SCROLLBAR_W  8

// Thickness of the rule under the header, and the gap every page leaves
// between that rule and its first row of content.
#define UI_HEADER_LINE_H 2
#define UI_CONTENT_GAP   10

#define UI_SCREEN_W 320
#define UI_SCREEN_H 240

// ---------------------------------------------------------------------
// Font mapping.
//
// LVGL ships Montserrat in ONE weight only - there is no bold variant, and
// no monospace face beyond the 8/16 px bitmap font unscii. Everything the
// specification calls bold or monospace therefore uses the regular
// Montserrat face at the nearest available size. The aliases exist so that
// swapping in generated bold/monospace fonts later is a change in this one
// header instead of in every screen.
// ---------------------------------------------------------------------
#define UI_FONT_12 &lv_font_montserrat_12
#define UI_FONT_14 &lv_font_montserrat_14
#define UI_FONT_15 &lv_font_montserrat_14  // 15 px does not exist in LVGL
#define UI_FONT_16 &lv_font_montserrat_16
#define UI_FONT_17 &lv_font_montserrat_16  // 17 px does not exist in LVGL
#define UI_FONT_18 &lv_font_montserrat_18
#define UI_FONT_20 &lv_font_montserrat_20
#define UI_FONT_22 &lv_font_montserrat_22
#define UI_FONT_28 &lv_font_montserrat_28
#define UI_FONT_32 &lv_font_montserrat_32
#define UI_FONT_40 &lv_font_montserrat_40
#define UI_FONT_44 &lv_font_montserrat_44

// Numeric readouts: would be monospace, and partly bold, if either existed
#define UI_FONT_NUM_TINY   UI_FONT_12  // debug values, 11 px in the spec
#define UI_FONT_NUM_SMALL  UI_FONT_14  // hex dumps
#define UI_FONT_NUM_FOOTER UI_FONT_18  // feed / spindle / step
#define UI_FONT_NUM_LARGE  UI_FONT_32  // DRO, three-row view
#define UI_FONT_NUM_HUGE   UI_FONT_40  // DRO, single-axis view

// The middle dot U+00B7 is NOT part of the Montserrat glyph set LVGL builds
// (its second cmap range is sparse and holds only the symbol font), so it
// would render as a placeholder box. U+2022 is included and is used instead.
#define UI_SEPARATOR_DOT "\xE2\x80\xA2"

// ---------------------------------------------------------------------
// Setup and switching
// ---------------------------------------------------------------------
void ui_theme_init(void);
void ui_theme_set(ui_theme_t theme);

// ---------------------------------------------------------------------
// Style helpers. Every screen builds its widgets out of these.
//
// Helpers that come in add/remove PAIRS (ui_set_*) exist because
// lv_obj_add_style() APPENDS: re-applying a style on every state change
// would grow the object's style list without limit. lv_obj_remove_style()
// with NULL is not an alternative - that strips the theme styles too.
// ---------------------------------------------------------------------

// Shadow and gradient of the default theme off. The theme recomputes a
// shadow on every redraw (LV_SHADOW_CACHE_SIZE is 0), which on 320x240
// without acceleration was the most expensive item in the rendering path.
void ui_apply_flat(lv_obj_t *obj);

void ui_apply_screen(lv_obj_t *obj);        // background surface
void ui_apply_transparent(lv_obj_t *obj);   // grouping container, no paint
void ui_apply_header(lv_obj_t *obj);        // header surface + 2 px rule below
void ui_apply_panel(lv_obj_t *obj);         // PANEL + 1 px BORDER, radius 8
void ui_apply_nav(lv_obj_t *obj);           // NAV surface + 1 px ACCENT border
void ui_apply_panel_active(lv_obj_t *obj);  // NAV fill, no border
void ui_apply_selected(lv_obj_t *obj);      // NAV fill + 2 px ACCENT border
void ui_clear_selected(lv_obj_t *obj);
void ui_apply_accent_fill(lv_obj_t *obj);  // ACCENT surface (badges, Reset)
void ui_set_accent_fill(lv_obj_t *obj, bool on);
void ui_apply_accent_button(lv_obj_t *obj);  // NAV fill + accent border
void ui_apply_separator_row(lv_obj_t *obj);  // thin rule along the bottom

// A tile that reacts to being pressed: panel surface normally, accent fill
// while the finger is down. The pressed state also carries the text colour,
// and LVGL inherits text colour, so a child label that sets NO colour of its
// own turns dark along with the fill. A label that does set its own colour
// (an amber detail line) needs ui_set_on_accent() on press instead.
void ui_apply_tile(lv_obj_t *obj);

// Error surface, identical in both themes: red fill, darker red border,
// white text. Used for the status bar when the host has gone quiet.
void ui_apply_error_fill(lv_obj_t *obj);
void ui_set_error_fill(lv_obj_t *obj, bool on);
void ui_apply_on_error(lv_obj_t *obj);

void ui_apply_text(lv_obj_t *obj);          // TEXT
void ui_apply_muted(lv_obj_t *obj);         // MUTED
void ui_set_muted(lv_obj_t *obj, bool on);  // pair, for values that grey out
void ui_apply_accent_label(lv_obj_t *obj);  // ACCENT
void ui_apply_on_accent(lv_obj_t *obj);     // text colour for use on a fill
void ui_set_on_accent(lv_obj_t *obj, bool on);

// A filled dot that takes the TEXT colour (machine-on indicator)
void ui_apply_dot(lv_obj_t *obj);

// Highlight of the selected DRO row, as ONE call for the three objects it
// touches.
void ui_set_axis_active(lv_obj_t *row, lv_obj_t *box, lv_obj_t *letter, bool active);

// Scrollbar colouring for a list container
void ui_apply_scrollbar(lv_obj_t *obj);

#endif
