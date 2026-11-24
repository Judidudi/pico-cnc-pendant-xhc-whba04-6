#include "ili9341_driver.h"

#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"

#include "pendant_config.h"

#define ILI9341_SPI         DISPLAY_SPI_INSTANCE
#define ILI9341_SPI_BAUD_HZ DISPLAY_SPI_BAUD_HZ

#define PIN_SCK   PIN_DISPLAY_SCK
#define PIN_MOSI  PIN_DISPLAY_MOSI
#define PIN_MISO  PIN_DISPLAY_MISO
#define PIN_CS    PIN_DISPLAY_CS
#define PIN_DC    PIN_DISPLAY_DC
#define PIN_RESET PIN_DISPLAY_RESET

#define CMD_SWRESET 0x01
#define CMD_SLPOUT  0x11
#define CMD_PIXFMT  0x3A
#define CMD_MADCTL  0x36
#define CMD_DISPON  0x29
#define CMD_CASET   0x2A
#define CMD_PASET   0x2B
#define CMD_RAMWR   0x2C

/* Orientation and colour order, both landscape (MV swaps rows and columns,
 * 320 pixels horizontally), differing only by the 180 degree rotation that
 * MY|MX add. BASE is how the display sits in the enclosure; FLIPPED is the
 * same panel upside down, selectable from the settings screen.
 *
 * The axis swap in the touch layer is the same for both and only the
 * direction reverses. Since xpt2046_set_rotated() mirrors the mapped
 * coordinates to match, an existing calibration survives the change - it did
 * not when this value was fixed at build time.
 *
 * If the colours are swapped (red for blue), drop the BGR bit 0x08 from
 * both. Not to be confused with LV_COLOR_16_SWAP, which is about byte order
 * rather than channel order - see the note on the flush below. */
#define MADCTL_BASE    0xE8
#define MADCTL_FLIPPED 0x28

// ---------------------------------------------------------------------
// DMA flush state.
//
// The transfer is completed from the DMA interrupt, which is what LVGL
// expects: lv_hal_disp.h notes that draw_buf->flushing is a plain int rather
// than a bit field precisely "because when it's cleared from IRQ a
// Read-Modify-Write issue might occur".
//
// Finishing it from the superloop instead does NOT work. With a single draw
// buffer lv_refr.c waits inside lv_timer_handler() - "In single buffered
// mode wait here until the buffer is freed" - so control never returns to
// the loop and a poll placed there would never run. That deadlocks on the
// first flush.
//
// volatile because the IRQ writes them while the main loop reads them.
// ---------------------------------------------------------------------
static int dma_channel = -1;
static volatile bool transfer_active = false;
static lv_disp_drv_t *pending_drv = NULL;

static void dma_irq_handler(void);

static inline void cs_select(void)
{
    gpio_put(PIN_CS, 0);
}
static inline void cs_deselect(void)
{
    gpio_put(PIN_CS, 1);
}
static inline void dc_command(void)
{
    gpio_put(PIN_DC, 0);
}
static inline void dc_data(void)
{
    gpio_put(PIN_DC, 1);
}

static void write_cmd(uint8_t cmd)
{
    dc_command();
    cs_select();
    spi_write_blocking(ILI9341_SPI, &cmd, 1);
    cs_deselect();
}

static void write_data(const uint8_t *data, size_t len)
{
    dc_data();
    cs_select();
    spi_write_blocking(ILI9341_SPI, data, len);
    cs_deselect();
}

static void write_data_byte(uint8_t b)
{
    write_data(&b, 1);
}

static void set_addr_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    uint8_t buf[4];

    write_cmd(CMD_CASET);
    buf[0] = x0 >> 8;
    buf[1] = x0 & 0xFF;
    buf[2] = x1 >> 8;
    buf[3] = x1 & 0xFF;
    write_data(buf, 4);

    write_cmd(CMD_PASET);
    buf[0] = y0 >> 8;
    buf[1] = y0 & 0xFF;
    buf[2] = y1 >> 8;
    buf[3] = y1 & 0xFF;
    write_data(buf, 4);

    write_cmd(CMD_RAMWR);
}

void ili9341_init(void)
{
    spi_init(ILI9341_SPI, ILI9341_SPI_BAUD_HZ);

    dma_channel = dma_claim_unused_channel(true);
    dma_channel_set_irq0_enabled(dma_channel, true);
    irq_set_exclusive_handler(DMA_IRQ_0, dma_irq_handler);
    irq_set_enabled(DMA_IRQ_0, true);
    gpio_set_function(PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);

    gpio_init(PIN_CS);
    gpio_set_dir(PIN_CS, GPIO_OUT);
    cs_deselect();

    gpio_init(PIN_DC);
    gpio_set_dir(PIN_DC, GPIO_OUT);

    gpio_init(PIN_RESET);
    gpio_set_dir(PIN_RESET, GPIO_OUT);

    gpio_put(PIN_RESET, 1);
    sleep_ms(10);
    gpio_put(PIN_RESET, 0);
    sleep_ms(10);
    gpio_put(PIN_RESET, 1);
    sleep_ms(120);

    write_cmd(CMD_SWRESET);
    sleep_ms(120);

    write_cmd(CMD_SLPOUT);
    sleep_ms(120);

    write_cmd(CMD_PIXFMT);
    write_data_byte(0x55); /* 16 bits per pixel, RGB565 on the SPI interface */

    write_cmd(CMD_MADCTL);
    write_data_byte(MADCTL_BASE);

    write_cmd(CMD_DISPON);
    sleep_ms(20);

    ili9341_fill_screen(0x0000);
}

void ili9341_fill_screen(uint16_t color_rgb565)
{
    /* Line buffer static, not on the stack: even with PICO_STACK_SIZE the
     * core 0 stack is small, and LVGL needs a lot of it itself */
    static uint8_t line[ILI9341_WIDTH * 2];

    for (uint16_t x = 0; x < ILI9341_WIDTH; x++) {
        line[x * 2] = (uint8_t) (color_rgb565 >> 8);
        line[x * 2 + 1] = (uint8_t) (color_rgb565 & 0xFF);
    }

    set_addr_window(0, 0, ILI9341_WIDTH - 1, ILI9341_HEIGHT - 1);

    dc_data();
    cs_select();
    for (uint16_t y = 0; y < ILI9341_HEIGHT; y++) {
        spi_write_blocking(ILI9341_SPI, line, sizeof(line));
    }
    cs_deselect();
}

void ili9341_set_rotated(bool rotated)
{
    // Changing MADCTL is an ordinary command on the same bus, so a transfer
    // in flight has to be let finish first. The rotation is switched from
    // the settings screen, where a flush can well be running.
    ili9341_bus_wait();

    write_cmd(CMD_MADCTL);
    write_data_byte(rotated ? MADCTL_FLIPPED : MADCTL_BASE);
}

// Runs when the DMA has handed over its last byte.
//
// Two clean-up steps that are easy to miss, both copied from what
// spi_write_blocking() does in the SDK - the DMA path has to do them itself,
// because nothing else will:
//
// 1. WAIT FOR THE SHIFT REGISTER. The DMA is finished once the last byte is
//    in the TX FIFO; FIFO and shift register still hold data. Releasing CS
//    here would cut the last pixels off the band.
//
// 2. DRAIN THE RX FIFO. The PL022 pushes a received byte for EVERY byte
//    transmitted. Over a 19,200-byte band nobody reads them, so the 8-entry
//    RX FIFO fills up and the overrun flag latches. The next user of the bus
//    is the touch controller, and its spi_write_read_blocking() would then
//    read that leftover rubbish instead of the XPT2046's answer - which is
//    exactly how this showed up: raw touch coordinates turned to nonsense
//    once the flush moved to DMA. Drained twice, because bytes keep arriving
//    until BSY clears.
static void dma_irq_handler(void)
{
    dma_channel_acknowledge_irq0(dma_channel);

    while (spi_is_readable(ILI9341_SPI)) {
        (void) spi_get_hw(ILI9341_SPI)->dr;
    }
    while (spi_is_busy(ILI9341_SPI)) {
        tight_loop_contents();
    }
    while (spi_is_readable(ILI9341_SPI)) {
        (void) spi_get_hw(ILI9341_SPI)->dr;
    }
    spi_get_hw(ILI9341_SPI)->icr = SPI_SSPICR_RORIC_BITS;

    cs_deselect();
    transfer_active = false;

    // Releases the draw buffer. LVGL is built for this to come from an
    // interrupt; see the note on the state variables above.
    lv_disp_flush_ready(pending_drv);
}

/* The pixel buffer goes to the panel untouched, which only works because
 * LV_COLOR_16_SWAP is 1: LVGL then stores each 16-bit pixel with the high
 * byte first, which is the order the ILI9341 expects. With the setting at 0
 * the RP2350 would write its little-endian halves straight out and every
 * colour would come back scrambled - a near-black background turns muddy
 * brown, white turns light green. Black and white survive it unharmed,
 * which is why a monochrome layout hides the fault completely.
 *
 * The transfer runs on DMA and this function returns at once. One 30-line
 * band is 320 x 30 x 16 bits = 153,600 bits, about 4.1 ms at 37.5 MHz, and a
 * measurement at 7.5 MHz gave 20.30 ms against 20.48 ms calculated - so a
 * flush is essentially pure bus waiting with no compute share to optimise.
 * Blocking on it meant the main loop, and with it tud_task() and the safety
 * heartbeat, stood still for those milliseconds. With DMA the loop keeps
 * running and only the pixel buffer stays reserved until the transfer ends. */
void ili9341_flush_cb(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p)
{
    uint32_t pixel_count =
        (uint32_t) (area->x2 - area->x1 + 1) * (area->y2 - area->y1 + 1);

    /* The window commands are a few bytes each and still go out blocking:
     * setting up a DMA for them would cost more than it saves. */
    set_addr_window(area->x1, area->y1, area->x2, area->y2);

    dc_data();
    cs_select();

    dma_channel_config config = dma_channel_get_default_config(dma_channel);
    channel_config_set_transfer_data_size(&config, DMA_SIZE_8);
    channel_config_set_read_increment(&config, true);
    channel_config_set_write_increment(&config, false);
    channel_config_set_dreq(&config, spi_get_dreq(ILI9341_SPI, true));

    pending_drv = disp_drv;
    transfer_active = true;

    dma_channel_configure(
        dma_channel, &config, &spi_get_hw(ILI9341_SPI)->dr, /* always the same register */
        color_p, pixel_count * sizeof(lv_color_t), true);   /* start now */

    /* No lv_disp_flush_ready() here - the DMA interrupt calls it. Until then
     * LVGL leaves the buffer alone, which is what keeps a single buffer safe
     * while the DMA is still reading out of it. */
}

void ili9341_bus_wait(void)
{
    /* The interrupt clears the flag, so this only has to wait. Spinning is
     * fine here: the longest possible transfer is one band, about 4.1 ms. */
    while (transfer_active) {
        tight_loop_contents();
    }
}
