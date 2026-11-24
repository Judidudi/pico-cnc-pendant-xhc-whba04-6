#ifndef PENDANT_STATE_H
#define PENDANT_STATE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "pendant_config.h"

// ---------------------------------------------------------------------
// Shared state. Modules only ever read and write here, never directly from
// one another (see docs/ARCHITECTURE.md).
// ---------------------------------------------------------------------

// One assignment for all eight keys - there are no operating modes.
typedef struct {
    uint8_t assigned_function[PENDANT_BUTTON_COUNT];
} button_mapping_t;

// A DRO coordinate exactly as the protocol delivers it: integer part,
// fractional part in units of 1/10000 and a separate sign.
typedef struct {
    bool negative;
    uint16_t integer_part;
    uint16_t fraction_part;
} pendant_coordinate_t;

// Display data received from the host (21-byte packet, already decoded).
//
// The names of the two rate fields follow the protocol, but their CONTENT is
// not what the names suggest. Both come from Display::updateData() in
// pendant.cc:
//
//   feed_rate         = (uint16_t)(feedOverrideValue * 100)
//   spindle_feed_rate = (uint16_t)(spindleOverrideValue * 100), except while
//                       a spindle speed change is active, when the host puts
//                       the commanded spindle speed in RPM there instead
//
// So feed_rate is a PERCENTAGE, not a feed in mm/min, and spindle_feed_rate
// carries either a percentage or an RPM figure depending on host state - the
// packet contains no flag telling us which. The original pendant has exactly
// the same ambiguity.
//
// The host truncates both values instead of rounding (static_cast, no rint).
// A feed override the GUI shows as 95% therefore arrives here as 94 whenever
// the float sits a hair below 0.95. That one-percent gap is produced on the
// host; this firmware passes both numbers through untouched.
typedef struct {
    bool valid;
    uint32_t last_rx_ms;
    pendant_coordinate_t coordinate[3];  // X, Y, Z
    uint16_t feed_rate;                  // feed override in percent
    uint16_t spindle_feed_rate;          // spindle override in percent, or RPM
    uint8_t step_mode;                   // 0=CON, 1=STEP, 2=MPG, 3=PERCENT
    bool is_reset;                       // host fills this from "machine is on"
    bool is_relative;
} pendant_display_t;

typedef struct {
    int8_t jog_delta;       // encoder detents since the last report
    uint8_t axis_selected;  // WHB04B_AXIS_*
    uint8_t feed_selected;  // WHB04B_FEED_*
    uint8_t key1;           // WHB04B_BTN_* or WHB04B_BTN_UNDEFINED
    uint8_t key2;

    bool usb_link_ok;   // USB enumerated and configured
    bool host_data_ok;  // host is sending display data (display only)

    pendant_display_t display;
} pendant_state_t;

pendant_state_t *pendant_state(void);
button_mapping_t *pendant_mapping(void);

void pendant_state_init(void);

// ---------------------------------------------------------------------
// The one table of assignable actions.
//
// This is the SINGLE source: the Functions screen, the key assignment
// screen, the picker and the default mapping all read from here. A second
// copy anywhere would drift from the protocol codes sooner or later.
//
// Layout of the table, relied on by the two tabs of the Functions screen:
//
//   index 0                    "None"
//   index 1 .. 16              the 16 protocol codes, in order 0x01..0x10
//   index 17 .. 31             the 15 Fn combinations (macros)
//
// There are 15 macros and not 16 because macro-10 is not a combination at
// all: the host raises that pin for ABS / REL (0x10) pressed ALONE, so it
// lives in the Keys tab. See the altText column of ButtonsCode in
// pendant.cc.
// ---------------------------------------------------------------------

// Stored value of one assignment - a single byte in the flash record, so
// the record keeps its size:
//
//   0x00         nothing assigned, the key does nothing
//   0x01 .. 0x10 a plain key, the protocol code itself
//   0x80 | code  a macro: that code sent with Fn held
//
// 0x80 is free because the protocol only ever uses 0x01..0x10.
#define PENDANT_ACTION_NONE 0x00u
#define PENDANT_MACRO_FLAG  0x80u

typedef struct {
    const char *name;    // "Feed +", "Macro 1", "None"
    const char *detail;  // "0x04", "Fn + Feed +", ""
    uint8_t value;       // the stored byte, see above
} pendant_action_t;

extern const pendant_action_t pendant_actions[];
extern const size_t pendant_action_count;

#define PENDANT_ACTION_KEY_FIRST   1u
#define PENDANT_ACTION_KEY_COUNT   16u
#define PENDANT_ACTION_MACRO_FIRST (PENDANT_ACTION_KEY_FIRST + PENDANT_ACTION_KEY_COUNT)
#define PENDANT_ACTION_MACRO_COUNT 15u

// Compile-time total, so a screen can size its page count from it. The
// static assert in pendant_state.c keeps it honest against the table.
#define PENDANT_ACTION_COUNT (1u + PENDANT_ACTION_KEY_COUNT + PENDANT_ACTION_MACRO_COUNT)

// Returned by pendant_action_index() for a byte that is not in the table
#define PENDANT_ACTION_NOT_FOUND ((size_t) -1)

size_t pendant_action_index(uint8_t value);
const char *pendant_action_name(uint8_t value);

static inline bool pendant_action_is_macro(uint8_t value)
{
    return (value & PENDANT_MACRO_FLAG) != 0u;
}

static inline uint8_t pendant_action_code(uint8_t value)
{
    return (uint8_t) (value & (uint8_t) ~PENDANT_MACRO_FLAG);
}

// Assigns an action to a key. Any action other than "None" may sit on only
// ONE key, so if it is already assigned elsewhere the two keys swap - with a
// duplicate the report would carry the same code twice and the second key
// would look dead. "None" is exempt: several keys may do nothing.
// Returns true if anything actually changed, which the caller uses to avoid
// a flash write for a selection that picks what was already there.
bool pendant_assign_action(uint8_t key_index, uint8_t value);

// Restores the factory assignment. Used when the flash holds nothing valid.
void pendant_set_default_mapping(void);

#endif
