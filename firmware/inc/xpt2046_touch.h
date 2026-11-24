#ifndef XPT2046_TOUCH_H
#define XPT2046_TOUCH_H

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

// Raw values at the four screen edges. Deliberately named after SCREEN sides
// rather than raw axes: in landscape the raw Y axis supplies the screen X
// coordinate (see the axis swap in xpt2046_touch.c).
//
// If "at_right" is smaller than "at_left", the axis runs the other way round
// - that falls out of the calibration by itself and needs no extra inversion
// flag.
typedef struct {
    // true: the raw Y axis runs along the screen width (landscape). This is
    // measured during calibration, not assumed - which raw axis points where
    // depends on the panel AND on the MADCTL value.
    bool swap_xy;
    int16_t raw_at_left;
    int16_t raw_at_right;
    int16_t raw_at_top;
    int16_t raw_at_bottom;
} xpt2046_calibration_t;

/* Initialises the touch CS/IRQ GPIOs (SPI1 itself is set up by ili9341_init()) */
void xpt2046_init(void);

/* LVGL indev read callback: supplies calibrated panel coordinates and press state */
void xpt2046_read_cb(lv_indev_drv_t *indev_drv, lv_indev_data_t *data);

void xpt2046_set_calibration(const xpt2046_calibration_t *calibration);
xpt2046_calibration_t xpt2046_get_calibration(void);

/* Mirrors the mapped coordinates, to be used together with
 * ili9341_set_rotated(). Applied AFTER the calibration, so a stored
 * calibration stays valid when the rotation changes. */
void xpt2046_set_rotated(bool rotated);

/* Last raw values measured - the basis of the calibration screen */
bool xpt2046_get_last_raw(uint16_t *raw_x, uint16_t *raw_y);

#endif /* XPT2046_TOUCH_H */
