#include "whb04b_usb.h"

#include <string.h>

#include "pico/stdlib.h"
#include "tusb.h"

#include "pendant_settings.h"
#include "usb_descriptors.h"
#include "whb04b_codes.h"

// ---------------------------------------------------------------------
// Display data layout - confirmed BYTE FOR BYTE against usb.h of the real
// LinuxCNC component (UsbOutPackageAxisCoordinate, DisplayIndicatorBitFields,
// UsbOutPackageData). 21 bytes, sent by the host as three 7-byte feature
// reports (report ID 6) over control transfers.
//
// The bit field order below matches the original declaration exactly. That
// matters: C leaves bit field placement to the implementation, and both ends
// only agree because both are GCC on a little-endian target, which fills
// from the least significant bit upwards. Porting either side to a different
// compiler means re-checking this.
// ---------------------------------------------------------------------
typedef struct __attribute__((packed)) {
    uint16_t integer_value;
    uint16_t fraction_value : 15;
    uint16_t coordinate_sign : 1;
} whb04b_axis_coordinate_t;  // 4 bytes

typedef struct __attribute__((packed)) {
    uint8_t step_mode : 2;  // 0=CON, 1=STEP, 2=MPG, 3=PERCENT
    uint8_t unknown : 4;    // meaning unknown in the original source too
    uint8_t is_reset : 1;
    uint8_t is_relative_coordinate : 1;
} whb04b_display_flags_t;  // 1 byte

typedef struct __attribute__((packed)) {
    uint16_t header;  // constant 0xfdfe
    uint8_t seed;     // feeds into the CRC of the IN report
    whb04b_display_flags_t display_mode_flags;
    whb04b_axis_coordinate_t row1_coordinate;  // X
    whb04b_axis_coordinate_t row2_coordinate;  // Y
    whb04b_axis_coordinate_t row3_coordinate;  // Z
    uint16_t feed_rate;
    uint16_t spindle_feed_rate;
    uint8_t padding;
} whb04b_out_package_data_t;

_Static_assert(sizeof(whb04b_out_package_data_t) == 21, "layout does not match usb.h");

#define WHB04B_OUT_HEADER 0xFDFEu

// Collects the individually arriving 7-byte blocks back into the complete
// 21-byte structure.
static union {
    whb04b_out_package_data_t fields;
    uint8_t raw[sizeof(whb04b_out_package_data_t)];
} out_buffer;
static uint8_t out_write_offset = 0;

// Initial value as in the original (UsbOutPackageData::clear())
static uint8_t last_seed = 0xFE;

static absolute_time_t next_report_time;

// Copies of the last complete packet in each direction, for the debug
// screen. Written after the fact and never read by the protocol itself.
static uint8_t last_sent[WHB04B_IN_REPORT_LEN];
static bool last_sent_valid = false;
static uint8_t last_received[WHB04B_OUT_PACKET_LEN];
static bool last_received_valid = false;

// Counts reports that really went out. virtual_key.c uses it to tell a
// press that was transmitted from one that only existed for a while in the
// shared state - see the note on phase_complete() there.
static uint32_t report_count = 0;

void whb04b_usb_init(void)
{
    tusb_init();
    next_report_time = get_absolute_time();
}

// ---------------------------------------------------------------------
// 8-byte IN report, layout confirmed against usb.h (UsbInPackage):
//   [0] header (constant 0x04)
//   [1] randomByte
//   [2] buttonKeyCode1
//   [3] buttonKeyCode2
//   [4] rotaryButtonFeedKeyCode
//   [5] rotaryButtonAxisKeyCode
//   [6] stepCount (int8_t, signed)
//   [7] crc
//
// The CRC is NOT a safety feature: xhc-whb04b6.cc processes the packet
// regardless of the result and only logs a mismatch. For the idle case the
// host expects crc == (randomByte & seed); with randomByte = 0xFF that holds
// for any seed, so a constant 0xFF keeps the check trivially satisfied.
//
// Returns true only once the report has actually been handed to TinyUSB.
// ---------------------------------------------------------------------
static bool send_report(pendant_state_t *state)
{
    if (!tud_hid_ready()) {
        return false;
    }

    const uint8_t random_byte = 0xFF;
    uint8_t report[WHB04B_IN_REPORT_LEN];

    report[0] = 0x04;
    report[1] = random_byte;
    report[2] = state->key1;
    report[3] = state->key2;
    report[4] = state->feed_selected;
    report[5] = state->axis_selected;
    // Handwheel direction reverses the sign here and nowhere else, so the
    // encoder keeps counting in its own direction and the diagnostics stay
    // readable. INT8_MIN has no positive counterpart, hence the clamp.
    int8_t step_count = state->jog_delta;
    if (pendant_settings()->handwheel_reversed) {
        step_count = (step_count == INT8_MIN) ? INT8_MAX : (int8_t) -step_count;
    }
    report[6] = (uint8_t) step_count;
    report[7] = random_byte & last_seed;

    if (!tud_hid_report(0, report, sizeof(report))) {
        return false;
    }

    memcpy(last_sent, report, sizeof(report));
    last_sent_valid = true;
    report_count++;

    // Only clear after a successful handover, otherwise the detents counted
    // into this report would be lost
    state->jog_delta = 0;
    return true;
}

void whb04b_usb_task(pendant_state_t *state)
{
    tud_task();

    state->usb_link_ok = tud_mounted() && !tud_suspended();

    uint32_t now_ms = to_ms_since_boot(get_absolute_time());
    state->host_data_ok = state->display.valid &&
                          (now_ms - state->display.last_rx_ms) < HOST_DATA_TIMEOUT_MS;
}

void whb04b_usb_send(pendant_state_t *state)
{
    if (absolute_time_diff_us(get_absolute_time(), next_report_time) > 0) {
        return;
    }

    // Only move the schedule forward once a report really went out. If the
    // endpoint was busy, retrying on the next loop pass is better than
    // sitting out another full period with jog detents already pending.
    if (send_report(state)) {
        next_report_time = delayed_by_ms(get_absolute_time(), WHB04B_REPORT_PERIOD_MS);
    }
}

// ---------------------------------------------------------------------
// TinyUSB callbacks
//
// These run from tud_task(), which the superloop calls - not from the USB
// interrupt. Writing into the shared state from here therefore needs no
// locking against the main loop.
// ---------------------------------------------------------------------

// We push reports ourselves over the interrupt IN endpoint, so GET_REPORT
// stays unused
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id,
                               hid_report_type_t report_type, uint8_t *buffer,
                               uint16_t reqlen)
{
    (void) instance;
    (void) report_id;
    (void) report_type;
    (void) buffer;
    (void) reqlen;
    return 0;
}

static void store_coordinate(pendant_coordinate_t *dst,
                             const whb04b_axis_coordinate_t *src)
{
    dst->negative = src->coordinate_sign != 0;
    dst->integer_part = src->integer_value;
    dst->fraction_part = src->fraction_value;  // unit 1/10000
}

// Appends one 7-byte block and reports whether the packet is now complete.
//
// The host sends the three blocks in order, but nothing guarantees we see
// all of them: the device may start listening mid-sequence, and a control
// transfer can be lost. Without a check, one missing block would shift every
// following byte by seven positions - permanently, because the offset never
// finds its way back. Verifying the header on the first block bounds the
// damage to a single garbled packet, after which the stream re-synchronises
// on its own.
static bool append_block(const uint8_t *block)
{
    if (out_write_offset == 0) {
        uint16_t header = (uint16_t) (block[0] | ((uint16_t) block[1] << 8));
        if (header != WHB04B_OUT_HEADER) {
            return false;  // not the start of a packet - keep waiting
        }
    }

    if ((size_t) out_write_offset + WHB04B_OUT_BLOCK_LEN > sizeof(out_buffer.raw)) {
        out_write_offset = 0;
        return false;
    }

    memcpy(&out_buffer.raw[out_write_offset], block, WHB04B_OUT_BLOCK_LEN);
    out_write_offset += WHB04B_OUT_BLOCK_LEN;

    if (out_write_offset < sizeof(out_buffer.raw)) {
        return false;
    }

    out_write_offset = 0;
    return true;
}

// The host sends the display data as a HID feature report (SET_REPORT,
// report ID 6) in three 7-byte blocks - confirmed against usb.cc. TinyUSB
// strips the report ID, so bufsize is 7 and not 8.
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id,
                           hid_report_type_t report_type, uint8_t const *buffer,
                           uint16_t bufsize)
{
    (void) instance;

    if (report_type != HID_REPORT_TYPE_FEATURE || report_id != WHB04B_OUT_REPORT_ID ||
        bufsize != WHB04B_OUT_BLOCK_LEN) {
        return;
    }

    if (!append_block(buffer)) {
        return;
    }

    memcpy(last_received, out_buffer.raw, sizeof(last_received));
    last_received_valid = true;

    last_seed = out_buffer.fields.seed;

    pendant_display_t *display = &pendant_state()->display;
    store_coordinate(&display->coordinate[0], &out_buffer.fields.row1_coordinate);
    store_coordinate(&display->coordinate[1], &out_buffer.fields.row2_coordinate);
    store_coordinate(&display->coordinate[2], &out_buffer.fields.row3_coordinate);
    display->feed_rate = out_buffer.fields.feed_rate;
    display->spindle_feed_rate = out_buffer.fields.spindle_feed_rate;
    display->step_mode = out_buffer.fields.display_mode_flags.step_mode;
    display->is_reset = out_buffer.fields.display_mode_flags.is_reset;
    display->is_relative = out_buffer.fields.display_mode_flags.is_relative_coordinate;
    display->last_rx_ms = to_ms_since_boot(get_absolute_time());
    display->valid = true;
}

uint32_t whb04b_usb_report_count(void)
{
    return report_count;
}

bool whb04b_usb_last_sent(uint8_t out[WHB04B_IN_REPORT_LEN])
{
    if (!last_sent_valid) {
        return false;
    }

    memcpy(out, last_sent, sizeof(last_sent));
    return true;
}

bool whb04b_usb_last_received(uint8_t out[WHB04B_OUT_PACKET_LEN])
{
    if (!last_received_valid) {
        return false;
    }

    memcpy(out, last_received, sizeof(last_received));
    return true;
}
