#ifndef ROTARY_SWITCH_H
#define ROTARY_SWITCH_H

#include "pendant_state.h"

void rotary_switches_init(void);

// Classifies the two resistor-ladder voltages into discrete switch positions
// and writes the matching WHB04B_AXIS_*/WHB04B_FEED_* codes into the state.
void rotary_switches_update(pendant_state_t *state);

// Raw values for the diagnostics part of the settings screen
uint16_t rotary_axis_raw(void);
uint16_t rotary_feed_raw(void);

#endif
