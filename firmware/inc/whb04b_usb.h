#ifndef WHB04B_USB_H
#define WHB04B_USB_H

#include "pendant_state.h"
#include "usb_descriptors.h"

void whb04b_usb_init(void);

// Service TinyUSB and update the link status. The host's display data also
// arrives through here (control transfers) and ends up in state->display.
// Should run more than once per loop pass - the less often it runs, the
// older the coordinates on screen are.
void whb04b_usb_task(pendant_state_t *state);

// Send the 8-byte IN report, at most every WHB04B_REPORT_PERIOD_MS.
// Must run after buttons_update(), otherwise the key codes are missing.
void whb04b_usb_send(pendant_state_t *state);

// ---------------------------------------------------------------------
// Raw traffic for the debug screen. Both buffers hold the last complete
// packet in the order it went over the wire, so the hex dump on screen can
// be compared one to one against what the host logs.
// ---------------------------------------------------------------------

// The 8 bytes of the IN report sent most recently. false until the first one
// has actually gone out.
// Number of IN reports successfully handed to TinyUSB since boot. Used to
// prove that a virtual key press was actually transmitted.
uint32_t whb04b_usb_report_count(void);

bool whb04b_usb_last_sent(uint8_t out[WHB04B_IN_REPORT_LEN]);

// The 21 bytes of display data received most recently, reassembled from the
// three blocks. false until a complete packet has arrived.
bool whb04b_usb_last_received(uint8_t out[WHB04B_OUT_PACKET_LEN]);

#endif
