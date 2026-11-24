#include "lvgl_port.h"

#include "lvgl.h"
#include "pico/time.h"

#include "ili9341_driver.h"
#include "xpt2046_touch.h"

#define DISP_BUF_LINES \
    30 /* partial buffer: 30 lines instead of a full frame, saves RAM */

// Two partial buffers, 19 KB each. With only one, lv_refr.c blocks before
// rendering the next band ("In single buffered mode wait here until the
// buffer is freed"), so drawing and transferring run strictly one after the
// other. With two, LVGL renders the next band into the free buffer while the
// DMA is still emptying the other one, and the rendering time disappears
// behind the transfer instead of adding to it.
//
// What it does NOT shorten is the transfer itself: eight bands of 4.1 ms are
// serial on one SPI bus no matter how many buffers there are. A full redraw
// therefore stays in the order of 33 ms.
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf1[ILI9341_WIDTH * DISP_BUF_LINES];
static lv_color_t buf2[ILI9341_WIDTH * DISP_BUF_LINES];

static lv_disp_drv_t disp_drv;
static lv_indev_drv_t indev_drv;

static repeating_timer_t tick_timer;

static void (*wait_hook)(void) = NULL;

void lvgl_port_set_wait_hook(void (*hook)(void))
{
    wait_hook = hook;
}

// Called by LVGL while it waits for a flush to finish. Keeping this short
// matters: it runs inside LVGL's refresh, so anything heavy here would
// stretch the very redraw it is meant to make bearable.
static void wait_cb(lv_disp_drv_t *drv)
{
    LV_UNUSED(drv);

    if (wait_hook != NULL) {
        wait_hook();
    }
}

static bool tick_timer_cb(repeating_timer_t *rt)
{
    LV_UNUSED(rt);
    lv_tick_inc(1);
    return true;
}

void lvgl_port_init(void)
{
    /* Panel first, LVGL second: as soon as the display driver is registered
     * LVGL may want to flush at any moment, and the hardware has to be up by
     * then. This exact order is what brought the display up. */
    ili9341_init();
    xpt2046_init();

    lv_init();

    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, ILI9341_WIDTH * DISP_BUF_LINES);

    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = ILI9341_WIDTH;
    disp_drv.ver_res = ILI9341_HEIGHT;
    disp_drv.flush_cb = ili9341_flush_cb;
    disp_drv.wait_cb = wait_cb;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = xpt2046_read_cb;
    lv_indev_drv_register(&indev_drv);

    /* Negative period: the timer fires every 1 ms regardless of how long the
     * callback itself takes */
    add_repeating_timer_ms(-1, tick_timer_cb, NULL, &tick_timer);
}

void lvgl_port_task(void)
{
    lv_timer_handler();
}
