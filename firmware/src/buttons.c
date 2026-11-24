#include "buttons.h"

#include "hardware/gpio.h"
#include "pico/stdlib.h"

#include "whb04b_codes.h"

typedef struct {
    uint pin;
    bool stable_pressed;
    bool raw_prev;
    absolute_time_t last_change;
} debounced_button_t;

static debounced_button_t function_buttons[PENDANT_BUTTON_COUNT];
static debounced_button_t encoder_button;

static bool home_request = false;

// Diagnostics display only, see buttons_pressed_mask()
static uint8_t pressed_mask;

static bool debounce(debounced_button_t *btn)
{
    bool raw = !gpio_get(btn->pin);  // active = LOW (internal pull-up)
    absolute_time_t now = get_absolute_time();

    if (raw != btn->raw_prev) {
        btn->raw_prev = raw;
        btn->last_change = now;
    } else if (raw != btn->stable_pressed &&
               absolute_time_diff_us(btn->last_change, now) >=
                   BUTTON_DEBOUNCE_MS * 1000) {
        btn->stable_pressed = raw;
    }

    return btn->stable_pressed;
}

static void input_gpio_init(uint pin)
{
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_IN);
    gpio_pull_up(pin);  // keys switch to GND
}

void buttons_init(void)
{
    static const uint8_t button_pins[PENDANT_BUTTON_COUNT] = PENDANT_BUTTON_PINS;

    for (uint8_t i = 0; i < PENDANT_BUTTON_COUNT; i++) {
        function_buttons[i].pin = button_pins[i];
        input_gpio_init(function_buttons[i].pin);
    }

    encoder_button.pin = PIN_ENCODER_SW;
    input_gpio_init(encoder_button.pin);
}

// ---------------------------------------------------------------------
// Report building.
//
// Two phases, deliberately separate: first every key is debounced, then the
// stable result is turned into the two key slots the report has. Mixing the
// two made the Fn handling below hard to follow, because the modifier can
// only be recognised once the state of ALL keys is known.
//
// A key carries one of three kinds of assignment (see pendant_state.h):
// nothing, a plain code, or a macro - a code sent with Fn held. A macro key
// therefore produces the same two-slot pattern as physically holding Fn and
// pressing that key, which is what makes the assignment and the Functions
// screen behave identically.
//
//   - Fn alone                 -> key1 = Fn,    key2 = none
//   - Fn held + another key    -> key1 = other, key2 = Fn
//   - a macro key alone        -> key1 = code,  key2 = Fn
//   - no Fn, one key           -> key1 = key,   key2 = none
//   - no Fn, several keys      -> the first two, the rest ignored
//
// The slot assignment for an Fn combination lives in ONE place,
// whb04b_fill_fn_combination() in whb04b_codes.h, together with the lines of
// xhc-whb04b6.cc that establish it. Worth knowing about the last case: if
// both slots hold non-Fn keys the host keeps only the one in key2 ("last
// key press overrules last but one"). Two ordinary keys at once are
// therefore never two actions, whatever we send.
//
// A macro key held together with another key is not representable - there
// are only two slots and Fn occupies one. The macro wins, because it is the
// more specific request.
// ---------------------------------------------------------------------
static void build_report_keys(pendant_state_t *state, const button_mapping_t *mapping)
{
    uint8_t ordinary[2] = { WHB04B_BTN_UNDEFINED, WHB04B_BTN_UNDEFINED };
    uint8_t ordinary_count = 0;
    uint8_t macro_code = WHB04B_BTN_UNDEFINED;
    bool function_held = false;

    for (uint8_t i = 0; i < PENDANT_BUTTON_COUNT; i++) {
        if (!function_buttons[i].stable_pressed) {
            continue;
        }

        uint8_t value = mapping->assigned_function[i];
        if (value == PENDANT_ACTION_NONE) {
            continue;
        }

        if (pendant_action_is_macro(value)) {
            if (macro_code == WHB04B_BTN_UNDEFINED) {
                macro_code = pendant_action_code(value);
            }
            continue;
        }

        uint8_t code = pendant_action_code(value);

        if (code == WHB04B_BTN_FUNCTION) {
            function_held = true;
        } else if (ordinary_count < 2) {
            ordinary[ordinary_count++] = code;
        }
    }

    if (macro_code != WHB04B_BTN_UNDEFINED) {
        whb04b_fill_fn_combination(&state->key1, &state->key2, macro_code);
    } else if (function_held) {
        // Fn is the modifier, so it belongs in the second slot and the key
        // it modifies in the first. Fn on its own has no partner and stays
        // in the first slot, which is what the host asserts for that case.
        if (ordinary_count > 0) {
            whb04b_fill_fn_combination(&state->key1, &state->key2, ordinary[0]);
        } else {
            state->key1 = WHB04B_BTN_FUNCTION;
            state->key2 = WHB04B_BTN_UNDEFINED;
        }
    } else {
        state->key1 = ordinary[0];
        state->key2 = ordinary[1];
    }
}

void buttons_update(pendant_state_t *state, const button_mapping_t *mapping)
{
    uint8_t mask = 0;

    for (uint8_t i = 0; i < PENDANT_BUTTON_COUNT; i++) {
        if (debounce(&function_buttons[i])) {
            mask |= (uint8_t) (1u << i);
        }
    }

    pressed_mask = mask;
    build_report_keys(state, mapping);

    bool encoder_was_pressed = encoder_button.stable_pressed;
    if (debounce(&encoder_button) && !encoder_was_pressed) {
        home_request = true;
    }
}

uint8_t buttons_pressed_mask(void)
{
    return pressed_mask;
}

bool buttons_take_home_request(void)
{
    bool requested = home_request;
    home_request = false;
    return requested;
}
