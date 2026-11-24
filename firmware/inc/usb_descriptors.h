#ifndef USB_DESCRIPTORS_H
#define USB_DESCRIPTORS_H

#include "tusb.h"

// ---------------------------------------------------------------------
// IMPORTANT: these IDs belong to the ORIGINAL XHC WHB04B-6 receiver. We
// deliberately present ourselves as that device so existing host drivers
// (LinuxCNC xhc-whb04b-6, the Beamicon2 plugin) recognise us without any
// change. Fine for a private student project, NOT to be used this way for
// publication or sale.
// ---------------------------------------------------------------------
#define WHB04B_VENDOR_ID  0x10CE
#define WHB04B_PRODUCT_ID 0xEB93

// Packet sizes - confirmed against the real LinuxCNC source
// (usb.cc, classes UsbInPackage / UsbOutPackageData):
// https://github.com/LinuxCNC/linuxcnc/blob/master/src/hal/user_comps/xhc-whb04b-6/usb.cc
#define WHB04B_IN_REPORT_LEN  8  // pendant -> host, over the interrupt IN endpoint
#define WHB04B_OUT_BLOCK_LEN  7  // host -> pendant: 3 blocks of 7 bytes
#define WHB04B_OUT_REPORT_ID  0x06
#define WHB04B_OUT_PACKET_LEN 21  // the three blocks reassembled

// IMPORTANT (confirmed from the source): the real device has only ONE
// endpoint (interrupt IN). Display data does NOT arrive over a second OUT
// endpoint but as a HID feature report over a control transfer (SET_REPORT,
// report type feature=3, report ID 6) in 3 blocks of 7 bytes. A pure IN
// report in the descriptor is therefore sufficient; the feature report is
// received through tud_hid_set_report_cb() without an endpoint of its own.
#define TUD_HID_REPORT_DESC_WHB04B(...)                                                                   \
    HID_USAGE_PAGE_N(HID_USAGE_PAGE_VENDOR, 2), HID_USAGE(0x01),                                          \
        HID_COLLECTION(                                                                                   \
            HID_COLLECTION_APPLICATION), /* Input report: pendant -> host (8 bytes, over interrupt IN) */ \
        HID_USAGE(0x02), HID_LOGICAL_MIN(0x00), HID_LOGICAL_MAX_N(0xFF, 2),                               \
        HID_REPORT_SIZE(8), HID_REPORT_COUNT(WHB04B_IN_REPORT_LEN),                                       \
        HID_INPUT(                                                                                        \
            HID_CONSTANT | HID_VARIABLE |                                                                 \
            HID_ABSOLUTE), /* Feature report: host -> pendant, report ID 6, 7 payload bytes */            \
        HID_REPORT_ID(WHB04B_OUT_REPORT_ID) HID_USAGE(0x03), HID_LOGICAL_MIN(0x00),                       \
        HID_LOGICAL_MAX_N(0xFF, 2), HID_REPORT_SIZE(8),                                                   \
        HID_REPORT_COUNT(WHB04B_OUT_BLOCK_LEN),                                                           \
        HID_FEATURE(HID_CONSTANT | HID_VARIABLE | HID_ABSOLUTE), HID_COLLECTION_END

#endif
