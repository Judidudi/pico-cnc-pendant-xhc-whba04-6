#include "pendant_state.h"

#include <string.h>

#include "pico/stdlib.h"

#include "whb04b_codes.h"

static pendant_state_t g_state;
static button_mapping_t g_mapping;

// The one table of assignable actions - see pendant_state.h for the layout
// the Functions screen relies on (None, then 16 keys, then 15 macros).
//
// The names are shared by the Functions screen, the key list and the picker,
// so a key and its macro always read the same way in all three.
// The columns of this table are aligned by hand. No clang-format setting
// reproduces that, so formatting is switched off for it.
// clang-format off
const pendant_action_t pendant_actions[] = {
    { "None",             "", PENDANT_ACTION_NONE },

    // --- the 16 protocol codes, 0x01..0x10 ---
    //
    // No second line: the tiles deliberately do NOT show the protocol code.
    // It would be noise on a screen meant for pressing things, and the Debug
    // screen already prints it next to every key slot of the live report.
    { "Reset",            "", WHB04B_BTN_RESET },
    { "Stop",             "", WHB04B_BTN_STOP },
    { "Start / Pause",    "", WHB04B_BTN_START_PAUSE },
    { "Feed +",           "", WHB04B_BTN_FEED_PLUS },
    { "Feed -",           "", WHB04B_BTN_FEED_MINUS },
    { "Spindle +",        "", WHB04B_BTN_SPINDLE_PLUS },
    { "Spindle -",        "", WHB04B_BTN_SPINDLE_MINUS },
    { "Machine home",     "", WHB04B_BTN_MACHINE_HOME },
    { "Safe Z",           "", WHB04B_BTN_SAFE_Z },
    { "Workpiece home",   "", WHB04B_BTN_WORKPIECE_HOME },
    { "Spindle on / off", "", WHB04B_BTN_SPINDLE_ON_OFF },
    { "Fn",               "", WHB04B_BTN_FUNCTION },
    { "Probe Z",          "", WHB04B_BTN_PROBE_Z },
    { "Mode continuous",  "", WHB04B_BTN_MODE_CONTINUOUS },
    { "Mode step",        "", WHB04B_BTN_MODE_STEP },
    { "ABS / REL",        "", WHB04B_BTN_MACRO10 },

    // --- the 15 Fn combinations ---
    //
    // Which number belongs to which key is NOT guesswork and not a free
    // choice: it is the altText column of ButtonsCode in pendant.cc, which
    // names the HAL pin the host raises. Hence the gap at 10 (that is ABS /
    // REL alone, above) and the jump from 9 to 11.
    //
    // The second line is abbreviated where the full name would not fit a
    // 97 px tile at 12 px.
    { "Macro 1",  "Fn + Feed +",    PENDANT_MACRO_FLAG | WHB04B_BTN_FEED_PLUS },
    { "Macro 2",  "Fn + Feed -",    PENDANT_MACRO_FLAG | WHB04B_BTN_FEED_MINUS },
    { "Macro 3",  "Fn + Spindle +", PENDANT_MACRO_FLAG | WHB04B_BTN_SPINDLE_PLUS },
    { "Macro 4",  "Fn + Spindle -", PENDANT_MACRO_FLAG | WHB04B_BTN_SPINDLE_MINUS },
    { "Macro 5",  "Fn+M-home",      PENDANT_MACRO_FLAG | WHB04B_BTN_MACHINE_HOME },
    { "Macro 6",  "Fn + Safe Z",    PENDANT_MACRO_FLAG | WHB04B_BTN_SAFE_Z },
    { "Macro 7",  "Fn+W-home",      PENDANT_MACRO_FLAG | WHB04B_BTN_WORKPIECE_HOME },
    { "Macro 8",  "Fn+S on/off",    PENDANT_MACRO_FLAG | WHB04B_BTN_SPINDLE_ON_OFF },
    { "Macro 9",  "Fn + Probe Z",   PENDANT_MACRO_FLAG | WHB04B_BTN_PROBE_Z },
    { "Macro 11", "Fn + Reset",     PENDANT_MACRO_FLAG | WHB04B_BTN_RESET },
    { "Macro 12", "Fn + Stop",      PENDANT_MACRO_FLAG | WHB04B_BTN_STOP },
    { "Macro 13", "Fn+Start",       PENDANT_MACRO_FLAG | WHB04B_BTN_START_PAUSE },
    { "Macro 14", "Fn + ABS / REL", PENDANT_MACRO_FLAG | WHB04B_BTN_MACRO10 },
    { "Macro 15", "Fn+Mode CON",    PENDANT_MACRO_FLAG | WHB04B_BTN_MODE_CONTINUOUS },
    { "Macro 16", "Fn+Mode STEP",   PENDANT_MACRO_FLAG | WHB04B_BTN_MODE_STEP },
};
// clang-format on

const size_t pendant_action_count = count_of(pendant_actions);

// The table layout is load-bearing for the two tabs of the Functions
// screen, so it is checked here rather than trusted.
_Static_assert(count_of(pendant_actions) == PENDANT_ACTION_COUNT,
               "action table does not match the Keys/Macros split");

// Default assignment in the order of the physical keys 1-8, matching the
// printed key caps. Used when the flash holds nothing valid.
static const uint8_t default_mapping[PENDANT_BUTTON_COUNT] = {
    WHB04B_BTN_RESET,           // key 1
    WHB04B_BTN_STOP,            // key 2
    WHB04B_BTN_START_PAUSE,     // key 3
    WHB04B_BTN_MACHINE_HOME,    // key 4
    WHB04B_BTN_SAFE_Z,          // key 5
    WHB04B_BTN_WORKPIECE_HOME,  // key 6
    WHB04B_BTN_SPINDLE_ON_OFF,  // key 7
    WHB04B_BTN_MACRO10,         // key 8: ABS / REL
};

pendant_state_t *pendant_state(void)
{
    return &g_state;
}

button_mapping_t *pendant_mapping(void)
{
    return &g_mapping;
}

void pendant_state_init(void)
{
    memset(&g_state, 0, sizeof(g_state));
    g_state.axis_selected = WHB04B_AXIS_OFF;
    g_state.feed_selected = WHB04B_FEED_UNDEFINED;
    g_state.key1 = WHB04B_BTN_UNDEFINED;
    g_state.key2 = WHB04B_BTN_UNDEFINED;

    pendant_set_default_mapping();
}

// Any action other than "None" may sit on only one key. Assigning one that
// another key already holds swaps the two rather than leaving a duplicate -
// with a duplicate the report would carry the same code twice and the second
// key would look dead. "None" is exempt, because several keys doing nothing
// is a perfectly ordinary state.
bool pendant_assign_action(uint8_t key_index, uint8_t value)
{
    if (key_index >= PENDANT_BUTTON_COUNT) {
        return false;
    }

    uint8_t previous = g_mapping.assigned_function[key_index];
    if (previous == value) {
        return false;
    }

    if (value != PENDANT_ACTION_NONE) {
        for (uint8_t i = 0; i < PENDANT_BUTTON_COUNT; i++) {
            if (i != key_index && g_mapping.assigned_function[i] == value) {
                g_mapping.assigned_function[i] = previous;
                break;
            }
        }
    }

    g_mapping.assigned_function[key_index] = value;
    return true;
}

const char *pendant_action_name(uint8_t value)
{
    size_t index = pendant_action_index(value);
    return index == PENDANT_ACTION_NOT_FOUND ? "--" : pendant_actions[index].name;
}

size_t pendant_action_index(uint8_t value)
{
    for (size_t i = 0; i < pendant_action_count; i++) {
        if (pendant_actions[i].value == value) {
            return i;
        }
    }
    return PENDANT_ACTION_NOT_FOUND;
}

void pendant_set_default_mapping(void)
{
    memcpy(g_mapping.assigned_function, default_mapping, sizeof(default_mapping));
}
