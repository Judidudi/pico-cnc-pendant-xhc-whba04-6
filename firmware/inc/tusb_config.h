#ifndef _TUSB_CONFIG_H_
#define _TUSB_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

// OPT_MCU_RP2040 is correct on the RP2350 as well: the Pico SDK routes both
// chips through the same TinyUSB port, and there is no separate RP2350
// option to select.
#define CFG_TUSB_MCU          OPT_MCU_RP2040
#define CFG_TUSB_OS           OPT_OS_PICO
#define CFG_TUSB_RHPORT0_MODE (OPT_MODE_DEVICE)

// We are ONLY a HID device, no CDC or serial port, so the USB port stays
// reserved exclusively for the WHB04B emulation. Debug output goes over the
// physical UART instead (see CMakeLists.txt).
#define CFG_TUD_ENABLED 1
#define CFG_TUD_CDC     0
#define CFG_TUD_MSC     0
#define CFG_TUD_HID     1
#define CFG_TUD_MIDI    0
#define CFG_TUD_VENDOR  0

// Endpoint buffer size. Only the 8-byte IN report travels over an endpoint;
// the display data arrives as a feature report over control transfers and
// does not need room here. 64 bytes is simply the default full-speed packet
// size and leaves headroom.
#define CFG_TUD_HID_EP_BUFSIZE 64

#define CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_ALIGN __attribute__((aligned(4)))

#ifdef __cplusplus
}
#endif

#endif
