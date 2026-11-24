#ifndef LVGL_PORT_H
#define LVGL_PORT_H

/* One-time: lv_init(), initialise and register display and touch, start the
 * 1 ms tick timer */
void lvgl_port_init(void);

/* Call from the superloop: lv_timer_handler() */
void lvgl_port_task(void);

/* ---------------------------------------------------------------------
 * With a single draw buffer LVGL waits INSIDE lv_timer_handler() until the
 * flush is finished ("In single buffered mode wait here until the buffer is
 * freed", lv_refr.c). A full redraw is eight bands, so that wait adds up to
 * tens of milliseconds in one call and the superloop does not get control
 * back in between.
 *
 * The hook registered here is what LVGL calls while it waits. It is the only
 * place that can keep USB alive during a redraw, which is what the DMA
 * flush is for in the first place - the transfer itself was never the slow
 * part, the starvation was.
 *
 * Deliberately a hook rather than a direct call: lvgl_port has no business
 * knowing about the USB backend. main.c registers it, where the order of
 * the superloop is decided anyway.
 * --------------------------------------------------------------------- */
void lvgl_port_set_wait_hook(void (*hook)(void));

#endif /* LVGL_PORT_H */
