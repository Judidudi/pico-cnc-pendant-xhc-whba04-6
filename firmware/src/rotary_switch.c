#include "rotary_switch.h"

#include "hardware/adc.h"
#include "pico/stdlib.h"

#include "whb04b_codes.h"

#define ADC_MAX 4095

// Fraction of half a zone width that a reading has to clear beyond the zone
// centre before the switch position changes. Prevents chatter at the zone
// boundaries when the switch sits between two detents or the line picks up
// noise.
#define HYSTERESIS_PERCENT 40

// Order equals the physical switch positions 1..n, the way the caps are
// labelled. That the ladder is wired the opposite way round is compensated
// ONCE in classify() and not hidden here behind a reversed table.
static const uint8_t axis_codes[] = { WHB04B_AXIS_X, WHB04B_AXIS_Y, WHB04B_AXIS_Z,
                                      WHB04B_AXIS_A };

static const uint8_t feed_codes[] = { WHB04B_FEED_0_001, WHB04B_FEED_0_01,
                                      WHB04B_FEED_0_1,   WHB04B_FEED_1,
                                      WHB04B_FEED_5,     WHB04B_FEED_10 };

typedef struct {
    uint8_t adc_input;
    uint8_t zone_count;
    uint8_t current_zone;
    bool initialised;
    uint16_t last_raw;
} rotary_switch_t;

static rotary_switch_t axis_switch = { .adc_input = ADC_INPUT_ROTARY_AXIS,
                                       .zone_count = count_of(axis_codes) };
static rotary_switch_t feed_switch = { .adc_input = ADC_INPUT_ROTARY_FEED,
                                       .zone_count = count_of(feed_codes) };

static uint16_t adc_read_averaged(uint8_t input)
{
    adc_select_input(input);
    adc_read();  // the first value after switching the mux is unreliable

    uint32_t sum = 0;
    for (uint8_t i = 0; i < 4; i++) {
        sum += adc_read();
    }
    return (uint16_t) (sum / 4);
}

// Nominal ladder value for position zone: evenly spread across the ADC
// range, because all resistors in the chain have the same value
static uint16_t zone_nominal(const rotary_switch_t *sw, uint8_t zone)
{
    return (uint16_t) (((uint32_t) zone * ADC_MAX) / (sw->zone_count - 1));
}

static uint8_t nearest_zone(const rotary_switch_t *sw, uint16_t raw)
{
    uint8_t best = 0;
    int32_t best_distance = INT32_MAX;

    for (uint8_t zone = 0; zone < sw->zone_count; zone++) {
        int32_t distance = (int32_t) raw - (int32_t) zone_nominal(sw, zone);
        if (distance < 0)
            distance = -distance;
        if (distance < best_distance) {
            best_distance = distance;
            best = zone;
        }
    }
    return best;
}

static uint8_t classify(rotary_switch_t *sw)
{
    uint16_t raw = adc_read_averaged(sw->adc_input);
    sw->last_raw = raw;

    // Both resistor ladders are wired the opposite way round from the
    // labelling on this board: the first position (X on the axis switch,
    // 0.001 on the feed switch) sits at the TOP of the ADC range, not at the
    // bottom. Without this mirroring the switch reads exactly backwards -
    // where X should be, A appears.
    //
    // Only the zone lookup is mirrored, not last_raw: the diagnostics display
    // on the settings screen should show the value actually measured,
    // otherwise one ends up hunting a wiring fault in a number the software
    // has already turned around.
    uint16_t value = (uint16_t) (ADC_MAX - raw);

    uint8_t candidate = nearest_zone(sw, value);

    if (!sw->initialised) {
        sw->initialised = true;
        sw->current_zone = candidate;
        return sw->current_zone;
    }

    if (candidate == sw->current_zone) {
        return sw->current_zone;
    }

    // Only switch once the value sits well inside the new zone
    int32_t half_step = (ADC_MAX / (sw->zone_count - 1)) / 2;
    int32_t accept_window = half_step - (half_step * HYSTERESIS_PERCENT) / 100;
    int32_t distance = (int32_t) value - (int32_t) zone_nominal(sw, candidate);
    if (distance < 0)
        distance = -distance;

    if (distance <= accept_window) {
        sw->current_zone = candidate;
    }

    return sw->current_zone;
}

void rotary_switches_init(void)
{
    adc_init();
    adc_gpio_init(PIN_ROTARY_AXIS);
    adc_gpio_init(PIN_ROTARY_FEED);
}

void rotary_switches_update(pendant_state_t *state)
{
    state->axis_selected = axis_codes[classify(&axis_switch)];
    state->feed_selected = feed_codes[classify(&feed_switch)];
}

uint16_t rotary_axis_raw(void)
{
    return axis_switch.last_raw;
}

uint16_t rotary_feed_raw(void)
{
    return feed_switch.last_raw;
}
