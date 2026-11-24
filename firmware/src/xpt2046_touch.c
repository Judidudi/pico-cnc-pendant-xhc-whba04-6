#include "xpt2046_touch.h"

#include "hardware/spi.h"
#include "hardware/gpio.h"

#include "ili9341_driver.h"
#include "pendant_config.h"

#define XPT2046_SPI DISPLAY_SPI_INSTANCE
#define XPT2046_SPI_BAUD_HZ \
    (2 * 1000 * 1000) /* the XPT2046 needs a far lower clock than the display */

#define PIN_CS  PIN_TOUCH_CS
#define PIN_IRQ PIN_TOUCH_IRQ

#define CMD_READ_X 0xD0
#define CMD_READ_Y 0x90

/* Starting values - replaced by measured ones through Settings -> Calibration */
static xpt2046_calibration_t calibration = {
    .swap_xy = true,
    .raw_at_left = 200,
    .raw_at_right = 3900,
    .raw_at_top = 200,
    .raw_at_bottom = 3900,
};

// Set from the settings; see xpt2046_set_rotated()
static bool rotated = false;

static uint16_t last_raw_x = 0;
static uint16_t last_raw_y = 0;
static bool last_raw_valid = false;

static inline void cs_select(void)
{
    gpio_put(PIN_CS, 0);
}
static inline void cs_deselect(void)
{
    gpio_put(PIN_CS, 1);
}

static uint16_t read_channel(uint8_t cmd)
{
    /* The display may have a DMA transfer in flight on this very bus. Taking
     * it over mid-transfer - and the baud rate change below would do exactly
     * that - would corrupt the band being sent. Cheap when nothing is
     * running: one flag test. */
    ili9341_bus_wait();

    /* Two devices share this bus and the touch controller cannot follow the
     * display clock. Saving and restoring the rate here keeps that detail
     * local, so the display clock can be raised freely without touching the
     * touch path. */
    uint32_t prev_baud = spi_get_baudrate(XPT2046_SPI);
    spi_set_baudrate(XPT2046_SPI, XPT2046_SPI_BAUD_HZ);

    uint8_t tx[3] = { cmd, 0x00, 0x00 };
    uint8_t rx[3] = { 0 };

    cs_select();
    spi_write_read_blocking(XPT2046_SPI, tx, rx, 3);
    cs_deselect();

    spi_set_baudrate(XPT2046_SPI, prev_baud);

    return (((uint16_t) rx[1] << 8) | rx[2]) >> 3;
}

static uint16_t read_channel_median(uint8_t cmd)
{
    /* Resistive touchscreens often deliver single outliers - without a
     * filter the reported point jumps too far, LVGL reads that as a drag
     * rather than a clean click and the touch is discarded */
    uint16_t s0 = read_channel(cmd);
    uint16_t s1 = read_channel(cmd);
    uint16_t s2 = read_channel(cmd);

    if (s0 > s1) {
        uint16_t t = s0;
        s0 = s1;
        s1 = t;
    }
    if (s1 > s2) {
        uint16_t t = s1;
        s1 = s2;
        s2 = t;
    }
    if (s0 > s1) {
        uint16_t t = s0;
        s0 = s1;
        s1 = t;
    }

    return s1;
}

static lv_coord_t map_axis(uint16_t raw, int16_t raw_at_zero, int16_t raw_at_max,
                           lv_coord_t size)
{
    int32_t span = (int32_t) raw_at_max - (int32_t) raw_at_zero;
    if (span == 0) {
        return 0;
    }

    int32_t value = ((int32_t) raw - (int32_t) raw_at_zero) * (size - 1) / span;

    if (value < 0)
        value = 0;
    if (value > size - 1)
        value = size - 1;

    return (lv_coord_t) value;
}

void xpt2046_init(void)
{
    gpio_init(PIN_CS);
    gpio_set_dir(PIN_CS, GPIO_OUT);
    cs_deselect();

    /* PENIRQ is held by the internal pull-up only, which is around 50-80 kOhm
     * and therefore fairly high-impedance. On this board the trace runs
     * between MISO (GP12) and SCK (GP14), so it has little noise margin; an
     * external 10 kOhm pull-up is the robust fix and the recommendation for
     * the next board revision. */
    gpio_init(PIN_IRQ);
    gpio_set_dir(PIN_IRQ, GPIO_IN);
    gpio_pull_up(PIN_IRQ);
}

void xpt2046_read_cb(lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
    LV_UNUSED(indev_drv);

    if (gpio_get(PIN_IRQ) != 0) {
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }

    uint16_t raw_x = read_channel_median(CMD_READ_X);
    uint16_t raw_y = read_channel_median(CMD_READ_Y);

    /* Check again: if the finger was lifted during the ADC reads (contact
     * bounce is normal on resistive panels), discard this read rather than
     * report a wrong or unstable point */
    if (gpio_get(PIN_IRQ) != 0) {
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }

    last_raw_x = raw_x;
    last_raw_y = raw_y;
    last_raw_valid = true;

    /* Axis swap: the MADCTL value only rotates the panel, while the XPT2046
     * sits physically on the glass and does not rotate with it. Which raw
     * axis runs along the screen width is therefore not assumed but measured
     * during calibration - as is the direction, which follows from the order
     * of raw_at_left/right. */
    uint16_t raw_horizontal = calibration.swap_xy ? raw_y : raw_x;
    uint16_t raw_vertical = calibration.swap_xy ? raw_x : raw_y;

    lv_coord_t x = map_axis(raw_horizontal, calibration.raw_at_left,
                            calibration.raw_at_right, ILI9341_WIDTH);
    lv_coord_t y = map_axis(raw_vertical, calibration.raw_at_top,
                            calibration.raw_at_bottom, ILI9341_HEIGHT);

    /* Mirrored only after the calibration has been applied, so the stored
     * calibration keeps its meaning when the rotation is switched. */
    if (rotated) {
        x = (lv_coord_t) (ILI9341_WIDTH - 1 - x);
        y = (lv_coord_t) (ILI9341_HEIGHT - 1 - y);
    }

    data->point.x = x;
    data->point.y = y;
    data->state = LV_INDEV_STATE_PRESSED;
}

void xpt2046_set_calibration(const xpt2046_calibration_t *new_calibration)
{
    calibration = *new_calibration;
}

void xpt2046_set_rotated(bool is_rotated)
{
    rotated = is_rotated;
}

xpt2046_calibration_t xpt2046_get_calibration(void)
{
    return calibration;
}

bool xpt2046_get_last_raw(uint16_t *raw_x, uint16_t *raw_y)
{
    if (!last_raw_valid) {
        return false;
    }

    *raw_x = last_raw_x;
    *raw_y = last_raw_y;
    return true;
}
