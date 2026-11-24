#ifndef UI_H
#define UI_H

// Builds all screens. Requires an initialised LVGL (lvgl_port_init()).
//
// The on-screen texts are English.
void ui_init(void);

// Call from the main loop: refreshes the display of the active screen.
// lv_timer_handler() runs separately through lvgl_port_task().
void ui_update(void);

// Jump back to the main screen (encoder push button)
void ui_show_main(void);

// Force the calibration screen - main.c calls this when the flash holds no
// valid calibration
void ui_show_calibration(void);

#endif
