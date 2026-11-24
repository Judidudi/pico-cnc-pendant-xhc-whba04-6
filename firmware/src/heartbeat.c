#include "heartbeat.h"

#include "hardware/gpio.h"
#include "pico/stdlib.h"

#include "pendant_config.h"

static absolute_time_t next_toggle;
static absolute_time_t last_call;
static uint32_t max_loop_gap_us = 0;
static bool level = false;
static bool measuring = false;

void heartbeat_init(void)
{
    gpio_init(PIN_HEARTBEAT);
    gpio_set_dir(PIN_HEARTBEAT, GPIO_OUT);
    gpio_put(PIN_HEARTBEAT, 0);

    next_toggle = get_absolute_time();
}

void heartbeat_update(bool link_ok)
{
    absolute_time_t now = get_absolute_time();

    if (measuring) {
        int64_t gap = absolute_time_diff_us(last_call, now);
        if (gap > 0 && (uint32_t) gap > max_loop_gap_us) {
            max_loop_gap_us = (uint32_t) gap;
        }
    }
    last_call = now;
    measuring = true;

    if (!link_ok) {
        level = false;
        gpio_put(PIN_HEARTBEAT, 0);
        next_toggle = now;
        return;
    }

    if (absolute_time_diff_us(now, next_toggle) > 0) {
        return;
    }

    level = !level;
    gpio_put(PIN_HEARTBEAT, level);

    // Measured from now, not from the old due time: after a long loop pass
    // the heartbeat should not catch up on the edges it missed but simply
    // carry on at its normal rate
    next_toggle = delayed_by_ms(now, HEARTBEAT_INTERVAL_MS);
}

uint32_t heartbeat_max_loop_gap_us(void)
{
    return max_loop_gap_us;
}
