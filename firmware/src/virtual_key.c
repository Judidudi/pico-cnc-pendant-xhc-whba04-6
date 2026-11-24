#include "virtual_key.h"

#include <stdio.h>

#include "pico/stdlib.h"

#include "whb04b_codes.h"
#include "whb04b_usb.h"

// How long a tap is held down, and the quiet time afterwards. The gap is
// what makes two taps of the same tile two events: without a report in
// which the slot is empty again, the host sees one continuous press.
#define PRESS_MS 120
#define GAP_MS   40

// A press that has seen no report at all by this point is given up on. With
// USB unplugged nothing is ever sent, and without this the state machine
// would hold the first tap forever and silently swallow every later one.
#define ABANDON_MS 500

typedef enum { VK_IDLE = 0, VK_PRESS, VK_GAP } vk_phase_t;

static uint8_t queue[VIRTUAL_KEY_QUEUE_LEN];
static uint8_t queue_head;
static uint8_t queue_tail;
static uint8_t queue_count;

static vk_phase_t phase;
static absolute_time_t phase_start;
static uint32_t phase_start_reports;
static uint8_t active_value;

// Tickets. One outcome is kept at a time: the screen polls every frame, far
// faster than a 120 ms press can complete, so a queue of results would never
// hold more than one anyway.
static uint32_t ticket_sequence;
static uint32_t queue_ticket[VIRTUAL_KEY_QUEUE_LEN];
static uint32_t active_ticket;
static uint32_t result_ticket;
static bool result_sent;
static bool result_pending;

void virtual_key_init(void)
{
    queue_head = 0;
    queue_tail = 0;
    queue_count = 0;
    phase = VK_IDLE;
    result_pending = false;
    ticket_sequence = VIRTUAL_KEY_NO_TICKET;
}

uint32_t virtual_key_push(uint8_t value)
{
    if (value == PENDANT_ACTION_NONE) {
        return VIRTUAL_KEY_NO_TICKET;
    }
    if (queue_count >= VIRTUAL_KEY_QUEUE_LEN) {
        return VIRTUAL_KEY_NO_TICKET;  // four taps already pending - drop this one
    }

    // Never hand out 0, that is the "rejected" answer
    if (++ticket_sequence == VIRTUAL_KEY_NO_TICKET) {
        ticket_sequence++;
    }

    queue[queue_head] = value;
    queue_ticket[queue_head] = ticket_sequence;
    queue_head = (uint8_t) ((queue_head + 1u) % VIRTUAL_KEY_QUEUE_LEN);
    queue_count++;

    // One short line per tap, deliberately terse. stdio over UART blocks
    // once the 32-byte TX FIFO is full, so the line is kept under that: at
    // 115200 baud a full FIFO would cost around 2.8 ms, which has no
    // business in a pass of the main loop. Nothing is allocated.
    printf("vkey %02X fn%u %lu\n", pendant_action_code(value),
           pendant_action_is_macro(value) ? 1u : 0u,
           (unsigned long) to_ms_since_boot(get_absolute_time()));

    return ticket_sequence;
}

bool virtual_key_take_result(uint32_t *ticket, bool *sent)
{
    if (!result_pending) {
        return false;
    }

    result_pending = false;
    *ticket = result_ticket;
    *sent = result_sent;
    return true;
}

static void finish_press(bool sent)
{
    result_ticket = active_ticket;
    result_sent = sent;
    result_pending = true;
}

static void enter_phase(vk_phase_t next)
{
    phase = next;
    phase_start = get_absolute_time();
    phase_start_reports = whb04b_usb_report_count();
}

static uint32_t reports_since_phase_start(void)
{
    return whb04b_usb_report_count() - phase_start_reports;
}

// A phase ends on time AND on evidence: at least one report has to have
// gone out while it lasted.
//
// Time alone is not enough. One pass of the main loop has been measured at
// over 100 ms, so a 120 ms window can contain as few as one opportunity to
// send - and if the endpoint happened to be busy for that one, the press
// would be released without ever having been transmitted. That is exactly
// how the earlier time-window pulse lost key presses, where only the very
// first tap reached the mill.
static bool phase_complete(uint32_t duration_ms)
{
    if (absolute_time_diff_us(phase_start, get_absolute_time()) <
        (int64_t) duration_ms * 1000) {
        return false;
    }

    return reports_since_phase_start() >= 1u;
}

static bool phase_expired(void)
{
    return absolute_time_diff_us(phase_start, get_absolute_time()) >=
           (int64_t) ABANDON_MS * 1000;
}

static void drop_front(void)
{
    queue_tail = (uint8_t) ((queue_tail + 1u) % VIRTUAL_KEY_QUEUE_LEN);
    queue_count--;
}

void virtual_key_apply(pendant_state_t *state)
{
    // The slots as the physical keys left them this pass. buttons_update()
    // runs earlier in the loop, so this is current.
    bool physical_held =
        (state->key1 != WHB04B_BTN_UNDEFINED) || (state->key2 != WHB04B_BTN_UNDEFINED);

    if (phase == VK_IDLE) {
        // A virtual tap has NO precedence over a physical key: the report
        // has two slots, and if a key owns one the tap waits.
        if (queue_count == 0 || physical_held) {
            return;
        }
        active_value = queue[queue_tail];
        active_ticket = queue_ticket[queue_tail];
        enter_phase(VK_PRESS);
    }

    if (phase == VK_PRESS) {
        // A physical key must never be masked - it is the actual machine
        // control, a tile is a convenience. If one goes down inside the
        // press window the virtual press is abandoned rather than stretched
        // or queued again: half a press is better dropped than held for an
        // unbounded time.
        if (physical_held) {
            finish_press(reports_since_phase_start() >= 1u);
            drop_front();
            enter_phase(VK_GAP);
            return;
        }

        if (pendant_action_is_macro(active_value)) {
            // One report carries both codes - see the note in
            // whb04b_codes.h, which is where the slot assignment lives.
            whb04b_fill_fn_combination(&state->key1, &state->key2,
                                       pendant_action_code(active_value));
        } else {
            state->key1 = pendant_action_code(active_value);
            state->key2 = WHB04B_BTN_UNDEFINED;
        }

        // Normal end: long enough AND proven transmitted. Second exit: no
        // report at all within ABANDON_MS, which is what happens with USB
        // unplugged - give the tap up and report it as NOT sent, rather than
        // holding the slots forever and swallowing everything behind it.
        if (phase_complete(PRESS_MS)) {
            finish_press(true);
            drop_front();
            enter_phase(VK_GAP);
        } else if (phase_expired()) {
            finish_press(false);
            drop_front();
            state->key1 = WHB04B_BTN_UNDEFINED;
            state->key2 = WHB04B_BTN_UNDEFINED;
            enter_phase(VK_GAP);
        }
        return;
    }

    // VK_GAP: the slots are left exactly as the physical keys set them, so
    // the host gets its release.
    if (phase_complete(GAP_MS) || phase_expired()) {
        phase = VK_IDLE;
    }
}
