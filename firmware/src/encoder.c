#include "encoder.h"

#include "hardware/gpio.h"
#include "hardware/sync.h"
#include "pico/stdlib.h"

// ---------------------------------------------------------------------
// Quadrature decoding through a GPIO interrupt rather than polling, so that
// fast turns between two superloop passes are not lost.
// Lookup over (previous state, new state), 2 bits each from CLK/DT.
// ---------------------------------------------------------------------
static const int8_t transition_table[16] = { 0,  -1, 1, 0, 1, 0, 0,  -1,
                                             -1, 0,  0, 1, 0, 1, -1, 0 };

static volatile int32_t raw_accum = 0;
static volatile uint8_t last_state = 0;

// Leftover edges that do not add up to a full detent yet
static int32_t detent_residual = 0;

// Diagnostics only, see encoder_total_detents()
static int32_t total_detents = 0;

static void gpio_irq_handler(uint gpio, uint32_t events)
{
    (void) gpio;
    (void) events;

    uint8_t new_state =
        (uint8_t) ((gpio_get(PIN_ENCODER_CLK) << 1) | gpio_get(PIN_ENCODER_DT));
    raw_accum += transition_table[(last_state << 2) | new_state];
    last_state = new_state;
}

void encoder_init(void)
{
    // Internal pull-ups even though the encoder module brings its own: they
    // only sit in parallel (the encoder switches to GND and so pulls against
    // both) and keep the pins defined high while NO encoder is plugged in.
    // Without them CLK/DT float, pick up noise from the neighbouring SPI
    // lines and fire a constant barrage of edge interrupts that slows the
    // main loop down. The firmware must not depend on an encoder being
    // present.
    gpio_init(PIN_ENCODER_CLK);
    gpio_set_dir(PIN_ENCODER_CLK, GPIO_IN);
    gpio_pull_up(PIN_ENCODER_CLK);

    gpio_init(PIN_ENCODER_DT);
    gpio_set_dir(PIN_ENCODER_DT, GPIO_IN);
    gpio_pull_up(PIN_ENCODER_DT);

    last_state = (uint8_t) ((gpio_get(PIN_ENCODER_CLK) << 1) | gpio_get(PIN_ENCODER_DT));

    gpio_set_irq_enabled_with_callback(PIN_ENCODER_CLK,
                                       GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true,
                                       &gpio_irq_handler);
    gpio_set_irq_enabled(PIN_ENCODER_DT, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);
}

int32_t encoder_total_detents(void)
{
    return total_detents;
}

void encoder_update(pendant_state_t *state)
{
    uint32_t irq_state = save_and_disable_interrupts();
    int32_t raw = raw_accum;
    raw_accum = 0;
    restore_interrupts(irq_state);

    detent_residual += raw;

    // Division truncates towards zero, so the remainder keeps its sign -
    // forward and reverse therefore behave symmetrically
    int32_t detents = detent_residual / ENCODER_COUNTS_PER_DETENT;
    detent_residual -= detents * ENCODER_COUNTS_PER_DETENT;

    if (detents == 0) {
        return;
    }

    total_detents += detents;

    // Add to the jog_delta that has not been sent yet instead of overwriting
    // it, otherwise detents arriving between two reports would be lost
    int32_t combined = (int32_t) state->jog_delta + detents;
    if (combined > INT8_MAX)
        combined = INT8_MAX;
    if (combined < INT8_MIN)
        combined = INT8_MIN;
    state->jog_delta = (int8_t) combined;
}
