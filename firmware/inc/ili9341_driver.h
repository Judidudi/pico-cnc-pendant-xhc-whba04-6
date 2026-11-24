#ifndef ILI9341_DRIVER_H
#define ILI9341_DRIVER_H

#include <stdbool.h>

#include "lvgl.h"

#define ILI9341_WIDTH  320
#define ILI9341_HEIGHT 240

/* Initialises SPI1, the GPIOs and the ILI9341 panel (reset + init sequence) */
void ili9341_init(void);

/* Rotates the panel by 180 degrees through MADCTL. One SPI command, no CPU
 * cost and no extra RAM - unlike LVGL's software rotation, which would turn
 * every pixel of every flush. The touch layer has to be told separately
 * (xpt2046_set_rotated), because the controller sits on the glass and does
 * not rotate with the panel. */
void ili9341_set_rotated(bool rotated);

/* LVGL flush callback: starts a DMA transfer of the rendered area and
 * returns immediately. LVGL is told the flush is done only once
 * ili9341_flush_poll() has seen the transfer finish. */
void ili9341_flush_cb(lv_disp_drv_t *disp_drv, const lv_area_t *area,
                      lv_color_t *color_p);

/* Blocks until the bus is free again. Anything that wants to use SPI1 for
 * something else - the touch controller changing the clock, a MADCTL write -
 * has to call this first, because display and touch share the bus and a DMA
 * transfer may be in flight. */
void ili9341_bus_wait(void);

/* Fill the whole panel, bypassing LVGL entirely. Used at the end of
 * ili9341_init() to clear the panel - after that a white display means
 * unambiguously "the init never reached the panel" rather than merely "LVGL
 * has not drawn anything yet". Colour in the panel's RGB565 format, so
 * 0xF800 for red for instance. */
void ili9341_fill_screen(uint16_t color_rgb565);

#endif /* ILI9341_DRIVER_H */
