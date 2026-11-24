#include "ui_internal.h"

#include "pico/stdlib.h"

#include "virtual_key.h"

// ---------------------------------------------------------------------
// Screen "Functions": every key code and every Fn combination as a tile
// that can be triggered by hand.
//
// Two tabs, two pages each, a fixed 3x3 grid and NO scrolling. The ninth
// cell is the page control ("More >" / "< Prev"), so eight actions fit on a
// page: 16 key codes and 15 macros come out as four pages exactly, with one
// empty cell on the last macro page.
//
// All four pages are built once and three of them carry LV_OBJ_FLAG_HIDDEN.
// Switching a flag is cheaper than rebuilding 32 tiles, and it keeps the tab
// and page change free of side effects - per the specification neither
// sends anything.
//
// A tile triggers on LV_EVENT_CLICKED and nothing else, which is exactly
// the behaviour asked for: LVGL sends CLICKED only when the release happens
// on the same object, so a finger that slides off sends nothing.
// ---------------------------------------------------------------------

#define TAB_W      96
#define TAB_H      UI_MIN_TOUCH_PX
#define HEADER_PAD 8
#define HEADER_GAP 8

// How long the confirmation stays on the tile after a tap really went out
#define SENT_FEEDBACK_MS 400

typedef enum { TAB_KEYS = 0, TAB_MACROS, TAB_COUNT } functions_tab_t;

#define PAGES_PER_TAB 2
#define PAGE_COUNT    (TAB_COUNT * PAGES_PER_TAB)

static lv_obj_t *tab_button[TAB_COUNT];
static lv_obj_t *tab_label[TAB_COUNT];
static lv_obj_t *page_obj[PAGE_COUNT];

static functions_tab_t active_tab = TAB_KEYS;
static uint8_t active_page[TAB_COUNT];

// Every action tile on every page, so the confirmation can find the one
// that was tapped without searching the object tree.
static ui_grid_tile_t action_tile[PAGE_COUNT][UI_GRID_PER_PAGE];
static const pendant_action_t *tile_action[PAGE_COUNT][UI_GRID_PER_PAGE];

// The tile currently showing "Sent", if any
static ui_grid_tile_t *feedback_tile;
static const pendant_action_t *feedback_action;
static absolute_time_t feedback_until;
static uint32_t pending_ticket = VIRTUAL_KEY_NO_TICKET;
static ui_grid_tile_t *pending_tile;
static const pendant_action_t *pending_action;

static uint8_t page_index(functions_tab_t tab, uint8_t page)
{
    return (uint8_t) (tab * PAGES_PER_TAB + page);
}

// ---------------------------------------------------------------------
// Sending
// ---------------------------------------------------------------------

static void clear_feedback(void)
{
    if (feedback_tile == NULL) {
        return;
    }

    ui_set_accent_fill(feedback_tile->obj, false);
    ui_set_on_accent(feedback_tile->name, false);
    lv_label_set_text(feedback_tile->name, feedback_action->name);
    if (feedback_tile->detail != NULL) {
        lv_obj_clear_flag(feedback_tile->detail, LV_OBJ_FLAG_HIDDEN);
    }

    feedback_tile = NULL;
}

// The tile turns into the confirmation itself rather than gaining an
// overlay: no extra widget per tile, and the text it has to restore is in
// the action table anyway.
static void show_feedback(ui_grid_tile_t *tile, const pendant_action_t *action)
{
    clear_feedback();

    feedback_tile = tile;
    feedback_action = action;
    feedback_until = delayed_by_ms(get_absolute_time(), SENT_FEEDBACK_MS);

    ui_set_accent_fill(tile->obj, true);
    ui_set_on_accent(tile->name, true);
    lv_label_set_text(tile->name, LV_SYMBOL_OK " Sent");
    if (tile->detail != NULL) {
        lv_obj_add_flag(tile->detail, LV_OBJ_FLAG_HIDDEN);
    }
}

static void tile_press_cb(lv_event_t *e)
{
    ui_grid_tile_t *tile = (ui_grid_tile_t *) lv_event_get_user_data(e);
    ui_grid_tile_set_pressed(tile, lv_event_get_code(e) == LV_EVENT_PRESSED);
}

static void tile_click_cb(lv_event_t *e)
{
    ui_grid_tile_t *tile = (ui_grid_tile_t *) lv_event_get_user_data(e);

    if (!ui_grid_released_inside(e)) {
        return;  // the finger slid off - nothing is sent
    }

    // Find which action this tile carries. Four pages of eight is a short
    // enough walk to beat storing a second pointer on every tile.
    const pendant_action_t *action = NULL;
    for (uint8_t p = 0; p < PAGE_COUNT && action == NULL; p++) {
        for (uint8_t i = 0; i < UI_GRID_PER_PAGE; i++) {
            if (&action_tile[p][i] == tile) {
                action = tile_action[p][i];
                break;
            }
        }
    }
    if (action == NULL) {
        return;
    }

    // Fire and forget. Nothing blocks and nothing is written to flash -
    // triggering a key is not a setting. The ticket comes back later with
    // the answer to "did that actually go out?".
    uint32_t ticket = virtual_key_push(action->value);
    if (ticket == VIRTUAL_KEY_NO_TICKET) {
        return;  // queue full, the tap is gone - and must NOT be confirmed
    }

    pending_ticket = ticket;
    pending_tile = tile;
    pending_action = action;
}

// Polled from ui_update() while this screen is on. Two jobs: collect the
// outcome of a tap, and take the confirmation away again after its time.
void ui_functions_update(void)
{
    uint32_t ticket;
    bool sent;
    if (virtual_key_take_result(&ticket, &sent)) {
        // Only confirm what really left the device. With USB unplugged the
        // press is abandoned and reported as not sent, and then no tick
        // appears - a confirmation for a key the machine never saw would be
        // worse than no feedback at all.
        if (sent && ticket == pending_ticket && pending_tile != NULL) {
            show_feedback(pending_tile, pending_action);
        }
        if (ticket == pending_ticket) {
            pending_ticket = VIRTUAL_KEY_NO_TICKET;
            pending_tile = NULL;
        }
    }

    if (feedback_tile != NULL &&
        absolute_time_diff_us(get_absolute_time(), feedback_until) <= 0) {
        clear_feedback();
    }
}

// ---------------------------------------------------------------------
// Pages and tabs
// ---------------------------------------------------------------------

static lv_obj_t *page_control_label;
static lv_obj_t *page_count_label;

static void show_active_page(void)
{
    uint8_t shown = page_index(active_tab, active_page[active_tab]);

    for (uint8_t p = 0; p < PAGE_COUNT; p++) {
        if (p == shown) {
            lv_obj_clear_flag(page_obj[p], LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(page_obj[p], LV_OBJ_FLAG_HIDDEN);
        }
    }

    bool first = (active_page[active_tab] == 0);
    lv_label_set_text(page_control_label,
                      first ? "More " LV_SYMBOL_RIGHT : LV_SYMBOL_LEFT " Prev");
    lv_label_set_text_fmt(page_count_label, "%u / %u", active_page[active_tab] + 1u,
                          PAGES_PER_TAB);

    for (uint8_t i = 0; i < TAB_COUNT; i++) {
        bool selected = (i == active_tab);
        ui_set_accent_fill(tab_button[i], selected);
        ui_set_on_accent(tab_label[i], selected);
    }
}

static void page_event_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    clear_feedback();
    active_page[active_tab] = (uint8_t) ((active_page[active_tab] + 1u) % PAGES_PER_TAB);
    show_active_page();
}

static void tab_event_cb(lv_event_t *e)
{
    functions_tab_t tab = (functions_tab_t) (uintptr_t) lv_event_get_user_data(e);
    if (tab >= TAB_COUNT || tab == active_tab) {
        return;
    }

    clear_feedback();
    active_tab = tab;
    // Back to the first page of the tab being opened, so the pair of tabs
    // always starts from the same place rather than from wherever it was
    // left. A tab change sends nothing.
    active_page[tab] = 0;
    show_active_page();
}

static void create_tab(lv_obj_t *screen, functions_tab_t tab, const char *text,
                       lv_coord_t x)
{
    lv_obj_t *btn = lv_btn_create(screen);
    ui_apply_panel(btn);
    lv_obj_set_size(btn, TAB_W, TAB_H);
    lv_obj_set_pos(btn, x, (UI_HEADER_H - UI_HEADER_LINE_H - TAB_H) / 2);
    lv_obj_add_event_cb(btn, tab_event_cb, LV_EVENT_CLICKED, (void *) (uintptr_t) tab);

    lv_obj_t *label = lv_label_create(btn);
    lv_obj_set_style_text_font(label, UI_FONT_18, 0);
    ui_apply_text(label);
    lv_label_set_text(label, text);
    lv_obj_center(label);

    tab_button[tab] = btn;
    tab_label[tab] = label;
}

// The ninth cell. Only one of the four pages needs to own it, so it is
// built on the screen rather than inside a page container and simply stays
// put while the pages behind it change.
static void create_page_control(lv_obj_t *screen)
{
    lv_obj_t *cell = lv_btn_create(screen);
    ui_apply_nav(cell);
    lv_obj_set_size(cell, UI_GRID_TILE_W, UI_GRID_TILE_H);
    lv_obj_set_pos(cell, ui_grid_cell_x(UI_GRID_CELLS - 1),
                   ui_grid_cell_y(UI_GRID_CELLS - 1));
    lv_obj_set_style_pad_all(cell, 4, 0);
    lv_obj_add_event_cb(cell, page_event_cb, LV_EVENT_CLICKED, NULL);

    page_control_label = lv_label_create(cell);
    lv_obj_set_style_text_font(page_control_label, UI_FONT_16, 0);
    ui_apply_accent_label(page_control_label);
    lv_obj_align(page_control_label, LV_ALIGN_TOP_MID, 0, 0);

    page_count_label = lv_label_create(cell);
    lv_obj_set_style_text_font(page_count_label, UI_FONT_NUM_TINY, 0);
    ui_apply_muted(page_count_label);
    lv_obj_align(page_count_label, LV_ALIGN_BOTTOM_MID, 0, 0);
}

static void create_page(lv_obj_t *screen, uint8_t page, size_t first, size_t count)
{
    // A transparent container over the WHOLE screen: hiding it hides its
    // eight tiles in one go. Full height on purpose - the tile positions
    // from ui_grid_cell_y() already include the header offset, so a holder
    // sized to the content area alone would sit at y=0 and clip the third
    // row off the bottom. It is transparent and not clickable, so it does
    // not come between the finger and the header.
    lv_obj_t *holder = lv_obj_create(screen);
    ui_apply_transparent(holder);
    lv_obj_set_size(holder, UI_SCREEN_W, UI_SCREEN_H);
    lv_obj_set_pos(holder, 0, 0);
    page_obj[page] = holder;

    for (uint8_t i = 0; i < UI_GRID_PER_PAGE; i++) {
        if (i >= count) {
            // The last macro page holds seven; the eighth cell stays empty.
            tile_action[page][i] = NULL;
            continue;
        }

        const pendant_action_t *action = &pendant_actions[first + i];
        ui_grid_tile_t *tile = &action_tile[page][i];

        tile->obj = ui_grid_tile_create(holder, i);
        ui_grid_tile_fill(tile, action);
        tile_action[page][i] = action;

        lv_obj_add_event_cb(tile->obj, tile_click_cb, LV_EVENT_RELEASED, tile);
        lv_obj_add_event_cb(tile->obj, tile_press_cb, LV_EVENT_PRESSED, tile);
        lv_obj_add_event_cb(tile->obj, tile_press_cb, LV_EVENT_RELEASED, tile);
        lv_obj_add_event_cb(tile->obj, tile_press_cb, LV_EVENT_PRESS_LOST, tile);
    }
}

void ui_functions_create(void)
{
    lv_obj_t *screen = ui_screen_create(UI_SCREEN_FUNCTIONS);

    // Back only, no title: the two tabs take the space a title would have.
    ui_header_surface_create(screen);
    ui_back_button_create(screen, UI_SCREEN_MENU);

    create_tab(screen, TAB_KEYS, "Keys", HEADER_PAD + TAB_W + HEADER_GAP);
    create_tab(screen, TAB_MACROS, "Macros", HEADER_PAD + 2 * (TAB_W + HEADER_GAP));

    create_page(screen, page_index(TAB_KEYS, 0), PENDANT_ACTION_KEY_FIRST,
                UI_GRID_PER_PAGE);
    create_page(screen, page_index(TAB_KEYS, 1),
                PENDANT_ACTION_KEY_FIRST + UI_GRID_PER_PAGE,
                PENDANT_ACTION_KEY_COUNT - UI_GRID_PER_PAGE);
    create_page(screen, page_index(TAB_MACROS, 0), PENDANT_ACTION_MACRO_FIRST,
                UI_GRID_PER_PAGE);
    create_page(screen, page_index(TAB_MACROS, 1),
                PENDANT_ACTION_MACRO_FIRST + UI_GRID_PER_PAGE,
                PENDANT_ACTION_MACRO_COUNT - UI_GRID_PER_PAGE);

    create_page_control(screen);
    show_active_page();
}
