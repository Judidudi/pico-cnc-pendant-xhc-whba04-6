#include "usb_descriptors.h"

#include <string.h>

// ---------------------------------------------------------------------
// Device descriptor
// ---------------------------------------------------------------------
tusb_desc_device_t const desc_device = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0110,

    // The HID class is declared by the interface, not here
    .bDeviceClass = 0x00,
    .bDeviceSubClass = 0x00,
    .bDeviceProtocol = 0x00,
    .bMaxPacketSize0 = 64,

    .idVendor = WHB04B_VENDOR_ID,
    .idProduct = WHB04B_PRODUCT_ID,
    .bcdDevice = 0x0000,

    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x03,

    .bNumConfigurations = 0x01
};

uint8_t const *tud_descriptor_device_cb(void)
{
    return (uint8_t const *) &desc_device;
}

// ---------------------------------------------------------------------
// HID report descriptor
// ---------------------------------------------------------------------
uint8_t const desc_hid_report[] = { TUD_HID_REPORT_DESC_WHB04B() };

uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance)
{
    (void) instance;
    return desc_hid_report;
}

// ---------------------------------------------------------------------
// Configuration descriptor
// ---------------------------------------------------------------------
enum { ITF_NUM_HID, ITF_NUM_TOTAL };

#define EPNUM_HID        0x81
#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN)

uint8_t const desc_configuration[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN,
                          TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),

    // Only ONE endpoint (interrupt IN), as confirmed for the original
    // device. Display data arrives as a feature report over control
    // endpoint 0, not over a second OUT endpoint. Polling interval 2 ms.
    TUD_HID_DESCRIPTOR(ITF_NUM_HID, 0, HID_ITF_PROTOCOL_NONE, sizeof(desc_hid_report),
                       EPNUM_HID, CFG_TUD_HID_EP_BUFSIZE, 2)
};

uint8_t const *tud_descriptor_configuration_cb(uint8_t index)
{
    (void) index;
    return desc_configuration;
}

// ---------------------------------------------------------------------
// String descriptors
// ---------------------------------------------------------------------
char const *string_desc_arr[] = {
    (const char[]) { 0x09, 0x04 },  // 0: language (English)
    "KTURT.LTD",                    // 1: manufacturer (same as the original device)
    "XHC WHB04B-6",                 // 2: product name
    "000001",                       // 3: serial number
};

static uint16_t _desc_str[32];

uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
    (void) langid;
    uint8_t chr_count;

    if (index == 0) {
        memcpy(&_desc_str[1], string_desc_arr[0], 2);
        chr_count = 1;
    } else {
        if (index >= sizeof(string_desc_arr) / sizeof(string_desc_arr[0])) {
            return NULL;
        }
        const char *str = string_desc_arr[index];
        chr_count = (uint8_t) strlen(str);
        if (chr_count > 31)
            chr_count = 31;

        for (uint8_t i = 0; i < chr_count; i++) {
            _desc_str[1 + i] = str[i];
        }
    }

    _desc_str[0] = (uint16_t) ((TUSB_DESC_STRING << 8) | (2 * chr_count + 2));
    return _desc_str;
}
