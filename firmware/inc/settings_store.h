#ifndef SETTINGS_STORE_H
#define SETTINGS_STORE_H

#include <stdbool.h>

#include "pendant_settings.h"
#include "pendant_state.h"
#include "xpt2046_touch.h"

// ---------------------------------------------------------------------
// Persistent settings in the last two flash sectors: touch calibration, key
// assignment and user settings.
//
// They live in ONE record, so every write carries all three. The module
// keeps the current record in RAM for that reason - writing only part of it
// would lose the rest.
//
// A save programs the next free slot and does NOT erase. That is a safety
// property, not a performance one: an erase freezes the main loop and with
// it the heartbeat for 100+ ms. See the comment at the top of
// settings_store.c for the layout.
// ---------------------------------------------------------------------

// Loads the stored record and applies calibration and key assignment.
// false means nothing valid was found (factory-fresh device, erased flash,
// different version); the caller then keeps the defaults.
bool settings_store_load(void);

// Each of these writes the WHOLE record, with the other part taken from what
// was loaded or saved before. Both block for some tens of milliseconds with
// interrupts disabled - so only call them on a deliberate user action, never
// cyclically during operation.
//
// They return true when the flash content matches afterwards. A call that
// would not change anything is skipped and also returns true.
bool settings_store_save_calibration(const xpt2046_calibration_t *calibration);

// Writes the key assignment AND the user settings as they currently stand in
// pendant_mapping()/pendant_settings(). Called when leaving a menu screen or
// right after a choice - never from the main screen, because the write
// freezes the main loop and with it the safety heartbeat.
bool settings_store_save_user_data(void);

// Erases whatever is left over from earlier writes, so the spare sector is
// ready and no save during operation has to erase.
//
// MUST be called after settings_store_load() and BEFORE heartbeat_init():
// an erase blocks for 100+ ms, and this is the one place where that is
// harmless - the heartbeat is not running yet and the machine cannot be
// enabled. Calling it later would put exactly the stall into the loop that
// the whole layout exists to avoid.
void settings_store_boot_maintenance(void);

#endif
