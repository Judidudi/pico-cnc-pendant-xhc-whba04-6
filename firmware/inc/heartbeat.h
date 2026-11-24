#ifndef HEARTBEAT_H
#define HEARTBEAT_H

#include <stdbool.h>
#include <stdint.h>

// ---------------------------------------------------------------------
// Firmware side of the safety circuit.
//
// On the pendant board sits a retriggerable monostable that, through a relay
// (closed-circuit principle, energize-to-run), is wired in series with the
// mushroom-head button in the emergency stop loop. If the edges on
// PIN_HEARTBEAT stop arriving, the monostable times out and the circuit
// opens.
//
// Deliberately EDGES, not a level: a frozen pin would still hold a valid
// level and could not be told apart from "everything is fine".
//
// heartbeat_update() MUST be called directly from the main loop, NEVER from
// a timer interrupt. An interrupt would keep running even though some other
// module has blocked the loop - which is exactly the case the watchdog is
// meant to cover. If any part of the main loop hangs, the heartbeat has to
// stop with it.
// ---------------------------------------------------------------------

void heartbeat_init(void);

// link_ok = false stops the heartbeat and so deliberately opens the safety
// circuit (USB no longer enumerated). Loss of supply voltage or a broken
// cable already act through the wiring, with no logic involved.
void heartbeat_update(bool link_ok);

// Longest measured pause between two calls, which is the worst-case duration
// of one loop pass. This is the figure the monostable time constant has to be
// dimensioned against; it is shown on the settings screen.
uint32_t heartbeat_max_loop_gap_us(void);

#endif
