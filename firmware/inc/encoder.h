#ifndef ENCODER_H
#define ENCODER_H

#include "pendant_state.h"

void encoder_init(void);

// Transfers the detents turned since the last call into state->jog_delta
// (adding to it, not overwriting).
void encoder_update(pendant_state_t *state);

// Detents counted since power-up, signed. Diagnostics for the debug screen:
// state->jog_delta is cleared by every report and is therefore useless for
// checking whether the encoder counts at all, and in which direction.
int32_t encoder_total_detents(void);

#endif
