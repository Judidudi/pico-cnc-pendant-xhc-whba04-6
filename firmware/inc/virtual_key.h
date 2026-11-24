#ifndef VIRTUAL_KEY_H
#define VIRTUAL_KEY_H

#include <stdbool.h>
#include <stdint.h>

#include "pendant_state.h"

// ---------------------------------------------------------------------
// Virtual key presses from the touch screen.
//
// A tile on the Functions screen produces exactly the same thing a physical
// key does: a code in the two key slots of the 8-byte IN report. There is no
// second USB path - whb04b_usb.c neither knows nor cares where a code came
// from.
//
// The queue exists because the report has only two key slots and physical
// keys own them. A tap that cannot be sent right away waits its turn
// instead of being dropped, and the order of taps is preserved.
// ---------------------------------------------------------------------

#define VIRTUAL_KEY_QUEUE_LEN 4

void virtual_key_init(void);

// Queues one tap. The value is a stored action byte, so PENDANT_MACRO_FLAG
// may be set for an Fn combination.
//
// Returns a TICKET, or VIRTUAL_KEY_NO_TICKET when the queue is full and the
// tap was dropped - four pending taps already means the user is tapping
// faster than the host can be told. The ticket comes back from
// virtual_key_take_result() once the press is over, which is how the screen
// knows whether that particular tap was really transmitted.
//
// Called from an LVGL event callback, so it must not block: it writes a few
// words and prints one short line.
#define VIRTUAL_KEY_NO_TICKET 0u

uint32_t virtual_key_push(uint8_t value);

// Collects the outcome of a finished press, at most one per call. `sent` is
// true only if a report carrying the code actually went out to the host -
// false when USB is not connected, or when the press had to be abandoned.
// The screen uses that to decide whether to confirm the tap; confirming a
// key that never left the device would be a lie at the machine.
bool virtual_key_take_result(uint32_t *ticket, bool *sent);

// Folds the current virtual press into the report, if any. Must run AFTER
// buttons_update() (which fills the slots from the physical keys) and BEFORE
// whb04b_usb_send(), so the order in main.c is load-bearing.
void virtual_key_apply(pendant_state_t *state);

#endif
