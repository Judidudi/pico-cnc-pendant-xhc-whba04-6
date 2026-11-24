#ifndef PENDANT_SETTINGS_H
#define PENDANT_SETTINGS_H

#include <stdbool.h>
#include <stdint.h>

// Deliberately does NOT include ui_theme.h. These are firmware settings, and
// pulling the whole of LVGL in through a header that settings_store.c needs
// would tie persistence to the user interface for the sake of one enum. The
// theme is kept as a plain number here; ui_theme.h gives it meaning.

// ---------------------------------------------------------------------
// User settings. Stored in flash together with the key assignment, in one
// record with a version and a checksum (see settings_store.h).
//
// NOTHING about the safety function is settable here, and nothing may be
// added: the heartbeat and the hardware watchdog are not allowed to depend
// on anything a user can change from the screen.
// ---------------------------------------------------------------------

#define PENDANT_AUTO_RETURN_OPTIONS 4

typedef struct {
    uint8_t theme;               // index into the palettes, see ui_theme.h
    uint8_t auto_return_index;   // index into pendant_auto_return_seconds[]
    uint8_t handwheel_reversed;  // flips the sign of Step count in the report
    uint8_t unit_inch;           // label only - nothing is converted
    uint8_t rotate_180;          // panel and touch rotated by 180 degrees
    uint8_t reserved[3];         // keeps the record a round size for later use
} pendant_settings_t;

// 0 means off. Index, not the value itself, so the segmented control and the
// stored record stay in step with one table.
extern const uint16_t pendant_auto_return_seconds[PENDANT_AUTO_RETURN_OPTIONS];

pendant_settings_t *pendant_settings(void);

// Light, 30 s, Normal, mm, 0 degrees. The theme default is part of
// SETTINGS_VERSION: changing it means a stored record has to be discarded,
// otherwise the old default survives the update.

void pendant_settings_set_defaults(void);

// Rejects a record with out-of-range fields - a corrupted sector would
// otherwise select a theme that does not exist or an auto-return index past
// the end of the table.
bool pendant_settings_are_sane(const pendant_settings_t *settings);

// Pushes every setting into the modules that act on it (theme, rotation,
// and so on). Called once at boot and by the settings screen on a change.
void pendant_settings_apply(void);

// Auto-return delay in milliseconds, 0 when switched off
uint32_t pendant_settings_auto_return_ms(void);

#endif
