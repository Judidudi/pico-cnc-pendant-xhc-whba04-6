#ifndef PENDANT_CONFIG_H
#define PENDANT_CONFIG_H

// ---------------------------------------------------------------------
// Central pin and timing constants of the pendant.
// GP23/24/25/29 are used internally by the CYW43 WiFi chip on the Pico 2 W
// and must never appear here.
// ---------------------------------------------------------------------

// --- Encoder (hand wheel) ---
#define PIN_ENCODER_CLK 2
#define PIN_ENCODER_DT  3
#define PIN_ENCODER_SW  4  // push button inside the encoder knob

// Quadrature edges per mechanical detent. Typical detented encoders (a
// KY-040 for instance) run through one full cycle per detent and therefore
// deliver 4. Without this division one detent would trigger four jog steps.
#define ENCODER_COUNTS_PER_DETENT 4

// --- Function keys 1-8 ---
// Deliberately a list rather than a contiguous range: on the final PCB keys
// 1-7 sit on GP16-GP22 but key 8 sits on GP26, because GP23-GP25 belong to
// the CYW43 chip. Order equals key number 1 to 8.
#define PENDANT_BUTTON_PINS  { 16, 17, 18, 19, 20, 21, 22, 26 }
#define PENDANT_BUTTON_COUNT 8

// --- Safety heartbeat (retriggerable monostable on the board) ---
#define PIN_HEARTBEAT 10

// --- Display (ILI9341) + touch (XPT2046), shared SPI1 bus ---
// Only MISO/SCK/MOSI go through the SPI hardware. Both chip selects and the
// touch IRQ stay plain GPIOs: the driver switches chip select itself because
// two devices share one bus.
#define DISPLAY_SPI_INSTANCE spi1
#define PIN_DISPLAY_CS       7
#define PIN_DISPLAY_RESET    8
#define PIN_DISPLAY_DC       9
#define PIN_TOUCH_CS         11
#define PIN_DISPLAY_MISO     12  // SPI1 RX: display SDO + touch DO
#define PIN_TOUCH_IRQ        13  // plain input, NOT switched to the SPI function
#define PIN_DISPLAY_SCK      14  // SPI1 SCK
#define PIN_DISPLAY_MOSI     15  // SPI1 TX: display SDI + touch DIN

// SPI clock towards the display. 40 MHz is requested; the hardware divides
// the 150 MHz system clock and ends up at 37.5 MHz in practice.
//
// This value is the decisive lever for the loop time: a measurement at
// 7.5 MHz gave 20.30 ms per flush against 20.48 ms calculated (320 x 30
// pixels x 2 bytes = 153,600 bits). A flush is therefore 99 % waiting on the
// bus with no meaningful compute share. The ILI9341 is only specified for
// around 10 MHz but tolerates considerably more over short traces.
//
// On picture defects (noise, shifted lines, colour garbage) step back to
// 25 MHz. The XPT2046 on the same bus cannot follow this clock and switches
// itself to 2 MHz for its own measurement (see xpt2046_touch.c).
#define DISPLAY_SPI_BAUD_HZ (40 * 1000 * 1000)

// --- Rotary switches through a resistor ladder on the ADC ---
// GP26 would be ADC0 but serves as a digital input for key 8 on the final
// PCB - there is no third rotary switch here.
//
// Pin AND ADC index have to match: the ADC channels are fixed at GP26=ADC0,
// GP27=ADC1, GP28=ADC2. rotary_switch.c uses the pin for adc_gpio_init() and
// the index for adc_select_input() - if the two disagree, one pin is enabled
// as an analog input while a different one is measured. Nothing reports an
// error; the switch just shows nonsense.
#define PIN_ROTARY_AXIS       27  // ADC1, 4 positions (X/Y/Z/A)
#define PIN_ROTARY_FEED       28  // ADC2, 6 positions (step size / override)
#define ADC_INPUT_ROTARY_AXIS 1
#define ADC_INPUT_ROTARY_FEED 2

// --- Timing ---
#define BUTTON_DEBOUNCE_MS      20
#define WHB04B_REPORT_PERIOD_MS 2  // polling interval of the original pendant

// Toggle interval of the heartbeat. The time constant of the monostable has
// to be considerably larger (rule of thumb: >= 200 ms) so that a single long
// loop pass - a full-screen redraw over SPI, say - does not open the safety
// circuit on its own.
#define HEARTBEAT_INTERVAL_MS 50

// Beyond this long without display data the host software counts as silent.
// This is purely a DISPLAY warning and never triggers the safety circuit
// (reasoning in heartbeat.h).
#define HOST_DATA_TIMEOUT_MS 2000

#endif
