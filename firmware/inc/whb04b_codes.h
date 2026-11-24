#ifndef WHB04B_CODES_H
#define WHB04B_CODES_H

#include <stdint.h>

// ---------------------------------------------------------------------
// Every code here is taken VERBATIM from the LinuxCNC source
// (pendant.cc, constructors AxisRotaryButtonCodes, FeedRotaryButtonCodes,
// ButtonsCode). Nothing in this file is guesswork.
// https://github.com/LinuxCNC/linuxcnc/blob/master/src/hal/user_comps/xhc-whb04b-6/pendant.cc
// ---------------------------------------------------------------------

// --- Axis selector switch (rotaryButtonAxisKeyCode) ---
#define WHB04B_AXIS_OFF       0x06
#define WHB04B_AXIS_X         0x11
#define WHB04B_AXIS_Y         0x12
#define WHB04B_AXIS_Z         0x13
#define WHB04B_AXIS_A         0x14
#define WHB04B_AXIS_B         0x15
#define WHB04B_AXIS_C         0x16
#define WHB04B_AXIS_UNDEFINED 0x00

// --- Step size selector switch (rotaryButtonFeedKeyCode) ---
// The second meaning applies in MPG/continuous mode, where the same switch
// position selects a percentage instead of a step size.
#define WHB04B_FEED_0_001     0x0d  // "2%" in MPG/continuous mode
#define WHB04B_FEED_0_01      0x0e  // "5%"
#define WHB04B_FEED_0_1       0x0f  // "10%"
#define WHB04B_FEED_1         0x10  // "30%"
#define WHB04B_FEED_5         0x1a  // "60%"
#define WHB04B_FEED_10        0x1b  // "100%"
#define WHB04B_FEED_LEAD      0x1c  // some clones expect 0x9B instead of 0x1c
#define WHB04B_FEED_UNDEFINED 0x00

// --- Buttons (buttonKeyCode1 / buttonKeyCode2) ---
#define WHB04B_BTN_RESET           0x01
#define WHB04B_BTN_STOP            0x02
#define WHB04B_BTN_START_PAUSE     0x03
#define WHB04B_BTN_FEED_PLUS       0x04
#define WHB04B_BTN_FEED_MINUS      0x05
#define WHB04B_BTN_SPINDLE_PLUS    0x06
#define WHB04B_BTN_SPINDLE_MINUS   0x07
#define WHB04B_BTN_MACHINE_HOME    0x08
#define WHB04B_BTN_SAFE_Z          0x09
#define WHB04B_BTN_WORKPIECE_HOME  0x0a
#define WHB04B_BTN_SPINDLE_ON_OFF  0x0b
#define WHB04B_BTN_FUNCTION        0x0c  // "Fn" modifier key
#define WHB04B_BTN_PROBE_Z         0x0d
#define WHB04B_BTN_MODE_CONTINUOUS 0x0e
#define WHB04B_BTN_MODE_STEP       0x0f
#define WHB04B_BTN_MACRO10         0x10  // toggles the DRO between absolute and relative
#define WHB04B_BTN_UNDEFINED       0x00

// ---------------------------------------------------------------------
// Fn combination: ONE report, target code in key1, Fn in key2
//
// This is the single place the slot assignment is defined. Confirmed
// against onInputDataReceived() in xhc-whb04b6.cc (LinuxCNC master), which
// normalises the two slots before anything else looks at them:
//
//   if (keyCode == undefined)            { keyCode = modifierCode; modifierCode = undefined; }
//   if (keyCode == function && modifierCode != undefined)
//                                        { keyCode = modifierCode; modifierCode = function; }
//   if (keyCode != undefined && modifierCode != undefined && modifierCode != function)
//                                        { keyCode = modifierCode; modifierCode = undefined; }
//
// Three things follow, and they settle the open question in the UI spec:
//
//  1. ONE report is enough. The host does not need Fn alone first and the
//     combination second - it reads both codes out of a single packet. No
//     two-step send sequence is implemented anywhere.
//  2. BOTH orders are accepted. key1=target/key2=Fn passes the third rule
//     untouched (it excludes modifierCode == function); key1=Fn/key2=target
//     is swapped into the same shape by the second rule. We send the first
//     form because that is the shape the host ends up with either way.
//  3. Two ORDINARY codes in one report are never two actions: the third
//     rule drops key1 and keeps key2 ("last key press overrules last but
//     one"). So a second non-Fn code may never be packed alongside.
//
// Which macro pin a combination drives is the host's business, from the
// altText column of ButtonsCode in pendant.cc - e.g. reset(0x01, "reset",
// "macro-11") means Fn+Reset raises whb.button.macro-11.
// ---------------------------------------------------------------------
static inline void whb04b_fill_fn_combination(uint8_t *key1, uint8_t *key2, uint8_t code)
{
    *key1 = code;
    *key2 = WHB04B_BTN_FUNCTION;
}

#endif
