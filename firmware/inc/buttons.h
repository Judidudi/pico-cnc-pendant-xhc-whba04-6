#ifndef BUTTONS_H
#define BUTTONS_H

#include "pendant_state.h"

void buttons_init(void);

// Debounces the 8 function keys and writes the active function codes into
// state->key1/key2 (the report has only two slots).
void buttons_update(pendant_state_t *state, const button_mapping_t *mapping);

// Bit mask of the function keys currently held down, bit 0 = key 1.
// Diagnostics only, for the settings screen - like rotary_axis_raw(). The
// machine functions still travel through state->key1/key2.
//
// The mask shows the PHYSICAL state, not the derived function. A key shows up
// even when it has no function assigned, or when the two report slots are
// already taken - which is exactly what is needed to check the wiring.
uint8_t buttons_pressed_mask(void);

// Encoder push button: a one-shot jump back to the home screen. Deliberately
// carries no machine function - accidentally pressing the knob you hold in
// your hand while jogging must not trigger anything.
bool buttons_take_home_request(void);

#endif
