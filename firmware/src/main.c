#include <stdio.h>

#include "pico/stdlib.h"

#include "buttons.h"
#include "encoder.h"
#include "heartbeat.h"
#include "ili9341_driver.h"
#include "lvgl_port.h"
#include "pendant_settings.h"
#include "pendant_state.h"
#include "rotary_switch.h"
#include "settings_store.h"
#include "ui.h"
#include "version.h"
#include "virtual_key.h"
#include "whb04b_usb.h"

// ---------------------------------------------------------------------
// Cooperative superloop: every module is tapped once per pass and none of
// them blocks indefinitely. There is deliberately NO overarching state
// machine - local state lives in the modules that need it.
//
// The order of the calls is load-bearing in two places, both marked below.
// The heartbeat runs last so it measures exactly what it is there to
// protect: one complete pass through all modules.
// ---------------------------------------------------------------------
// Registered with the LVGL port; see lvgl_port_set_wait_hook(). Only USB is
// serviced here, deliberately: the heartbeat belongs to the superloop and
// must stop when the loop stops, which is the whole point of it.
static void usb_wait_hook(void)
{
    whb04b_usb_task(pendant_state());
}

int main(void)
{
    stdio_init_all();  // debug output over the physical UART (GP0/GP1)

    pendant_state_init();
    pendant_settings_set_defaults();

    // Display first: it is the only feedback the device itself can give.
    // Interrupts are armed only afterwards - if the device hangs later on,
    // at least there is a picture on screen.
    lvgl_port_init();
    ui_init();

    // Loads calibration AND key assignment. Without a valid record go
    // straight to the calibration: with a wrong mapping that screen cannot
    // be reached any other way. The assignment then stays at the default
    // that pendant_state_init() already put in place.
    if (!settings_store_load()) {
        ui_show_calibration();
    }

    // After loading, because the stored theme and rotation have to reach the
    // display and touch layers before the first frame is shown.
    pendant_settings_apply();

    // Deliberately BEFORE heartbeat_init(): this may erase a flash sector,
    // which blocks for 100+ ms. Here that is harmless, because the heartbeat
    // is not running yet and the machine cannot be enabled. Afterwards it
    // would be exactly the stall the two-sector layout exists to avoid.
    settings_store_boot_maintenance();

    heartbeat_init();
    encoder_init();
    buttons_init();
    rotary_switches_init();
    virtual_key_init();
    whb04b_usb_init();

    // LVGL waits inside lv_timer_handler() for each band of a redraw to go
    // out. Without this hook USB would be starved for the whole redraw,
    // which is exactly the lag the DMA flush is meant to remove - the
    // transfer was never the slow part, the starvation was.
    lvgl_port_set_wait_hook(usb_wait_hook);

    printf("%s %s ready (WHB04B-6 compatible)\n", CNC_PENDANT_NAME,
           CNC_PENDANT_VERSION_STR);

    pendant_state_t *state = pendant_state();

    while (true) {
        // Service USB twice per pass, once before and once after drawing:
        // the LVGL pass can block on SPI for several milliseconds, and the
        // host's display data piles up meanwhile - the DRO then visibly lags
        // behind.
        whb04b_usb_task(state);

        encoder_update(state);
        buttons_update(state, pendant_mapping());
        rotary_switches_update(state);

        if (buttons_take_home_request()) {
            ui_show_main();
        }

        ui_update();
        lvgl_port_task();  // the touch events fire in here

        // Load-bearing position: after buttons_update() has put the physical
        // keys into the slots, after lvgl_port_task() so a tap made in this
        // pass can start right away, and before the report goes out.
        virtual_key_apply(state);

        whb04b_usb_task(state);
        whb04b_usb_send(state);

        heartbeat_update(state->usb_link_ok);
    }
}
