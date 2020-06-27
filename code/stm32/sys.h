#ifndef SYSTEM_H
#define SYSTEM_H
#include <assert.h>

enum RGB_TYPE {
    RGB_TYPE_NONE = 0,
    RGB_TYPE_10 = 1,
    RGB_TYPE_4 = 2,
};

enum USB_DEV {
    USB_DEV_DISABLED = 0, // Disable Developer Mode, erase cart.bin
    USB_DEV_CHECK = 1,    // Check for USB to be plugged in
    USB_DEV_EXIT = 2,     // Exit Developer Mode, erase cart.bin
    USB_DEV_WAIT = 3,     // Start the ramdisk for the first time, wait for (cart.bin/exit/run)
    USB_DEV_EJECT = 4,    // USB host ejected the USB device
    USB_DEV_RUN = 5,      // Look for cart.bin and load, or disable
};

typedef struct {
    uint16_t size; // size of this struct
    uint16_t ver; // system_options version
    uint16_t hw_ver; // 0x0014 = 0.20
    uint16_t sw_ver; // 0x0016 = 0.22
    uint8_t  rgb_type; // RGB_TYPE
    uint8_t  usb_dev; // USB_DEV
    uint8_t  reserved[128-10];
} system_options;
static_assert(sizeof(system_options)==128, "system_options should be 128 bytes, check the reserved field.");

#endif