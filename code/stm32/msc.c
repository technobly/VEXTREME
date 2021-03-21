/*
 * This file is part of the libopencm3 project.
 *
 * Copyright (C) 2013 Weston Schmidt <weston_schmidt@alumni.purdue.edu>
 * Copyright (C) 2013 Pavol Rusnak <stick@gk2.sk>
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/spi.h>
#include <libopencm3/usb/dwc/otg_common.h>
#include <libopencm3/usb/dwc/otg_fs.h>
#include <libopencm3/usb/usbd.h>
#include <libopencm3/usb/msc.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/cm3/scb.h>
#include "flash.h"
#include "xprintf.h"
#include "delay.h"
#include "sys.h"
extern system_options sys_opt;

#include "msc.h"

static const struct usb_device_descriptor dev_descr = {
    .bLength = USB_DT_DEVICE_SIZE,
    .bDescriptorType = USB_DT_DEVICE,
    .bcdUSB = 0x0110,
    .bDeviceClass = 0,
    .bDeviceSubClass = 0,
    .bDeviceProtocol = 0,
    .bMaxPacketSize0 = 64,
    .idVendor = 0x0483,
    .idProduct = 0x5741,
    .bcdDevice = 0x0200,
    .iManufacturer = 1,
    .iProduct = 2,
    .iSerialNumber = 3,
    .bNumConfigurations = 1,
};

static const struct usb_endpoint_descriptor msc_endp[] = {{
    .bLength = USB_DT_ENDPOINT_SIZE,
    .bDescriptorType = USB_DT_ENDPOINT,
    .bEndpointAddress = 0x01,
    .bmAttributes = USB_ENDPOINT_ATTR_BULK,
    .wMaxPacketSize = 64,
    .bInterval = 0,
}, {
    .bLength = USB_DT_ENDPOINT_SIZE,
    .bDescriptorType = USB_DT_ENDPOINT,
    .bEndpointAddress = 0x82,
    .bmAttributes = USB_ENDPOINT_ATTR_BULK,
    .wMaxPacketSize = 64,
    .bInterval = 0,
}};

static const struct usb_interface_descriptor msc_iface[] = {{
    .bLength = USB_DT_INTERFACE_SIZE,
    .bDescriptorType = USB_DT_INTERFACE,
    .bInterfaceNumber = 0,
    .bAlternateSetting = 0,
    .bNumEndpoints = 2,
    .bInterfaceClass = USB_CLASS_MSC,
    .bInterfaceSubClass = USB_MSC_SUBCLASS_SCSI,
    .bInterfaceProtocol = USB_MSC_PROTOCOL_BBB,
    .iInterface = 0,
    .endpoint = msc_endp,
    .extra = NULL,
    .extralen = 0
}};

static const struct usb_interface ifaces[] = {{
    .num_altsetting = 1,
    .altsetting = msc_iface,
}};

static const struct usb_config_descriptor config_descr = {
    .bLength = USB_DT_CONFIGURATION_SIZE,
    .bDescriptorType = USB_DT_CONFIGURATION,
    .wTotalLength = 0,
    .bNumInterfaces = 1,
    .bConfigurationValue = 1,
    .iConfiguration = 0,
    .bmAttributes = 0x80,
    .bMaxPower = 0xFA, // Set USB power to 500mA (was set to 100mA)
    .interface = ifaces,
};

static const char *usb_strings[] = {
    "PlayVectrex.com",
    "VEXTREME",
    "0000",
};

static usbd_device *msc_dev;
/* Buffer to be used for control requests. */
static uint8_t usbd_control_buffer[128];
static uint8_t lastUsbDev = 0;
static bool ramdisk_init = false; // only init once
uint32_t nonBlockingRamdiskTimer = 0; // disabled by default
uint32_t nonBlockingRamdiskTimeout = 950UL;
int ramdiskmain(int blocking) {
    if (!ramdisk_init) {
        ramdisk_init = true;
        rcc_periph_clock_enable(RCC_GPIOA);
        rcc_periph_clock_enable(RCC_OTGFS);
        // gpio_mode_setup(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO11 | GPIO12);
        gpio_mode_setup(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO11);
        gpio_mode_setup(GPIOA, GPIO_MODE_AF, GPIO_PUPD_PULLUP, GPIO12);
        gpio_set_af(GPIOA, GPIO_AF10, GPIO11 | GPIO12);
        flashInit();
        msc_dev = usbd_init(&otgfs_usb_driver, &dev_descr, &config_descr,
                usb_strings, 3,
                usbd_control_buffer, sizeof(usbd_control_buffer));
        // OTG_FS_GCCFG &= ~OTG_GCCFG_VBUSBSEN;
        // OTG_FS_GCCFG |= OTG_GCCFG_NOVBUSSENS;
        usb_msc_init(msc_dev, 0x82, 64, 0x01, 64, "GPL3", "VEXTREME",
                "0", (16*1024*2)-16, flashReadBlk, flashWriteBlk);
    }

    if (blocking == RAMDISK_NON_BLOCKING) {
        if (lastUsbDev != sys_opt.usb_dev && sys_opt.usb_dev == USB_DEV_WAIT) {
            xprintf("Connecting USB\n");
            usbd_disconnect(msc_dev, 0); // Re-connect USB, if disconnected
        }
        nonBlockingRamdiskTimeout = 1000UL;
    } else {
        xprintf("Connecting USB\n");
        usbd_disconnect(msc_dev, 0); // Re-connect USB, if disconnected
    }

    // Disable VBUS sense
    // OTG_FS_GCCFG &= ~OTG_GCCFG_VBUSBSEN;
    // OTG_FS_GCCFG |= OTG_GCCFG_NOVBUSSENS;

    // Enable VBUS sense
    // OTG_FS_GCCFG &= ~OTG_GCCFG_NOVBUSSENS;
    // OTG_FS_GCCFG |= OTG_GCCFG_VBUSBSEN;

    bool exit = false;
    if (blocking == RAMDISK_NON_BLOCKING) nonBlockingRamdiskTimer = millis();
    uint32_t lastFlashTick = 0;
    uint32_t exitTimer = 0; // disabled by default
    while (!exit) {
        usbd_poll(msc_dev);
        if (millis() - lastFlashTick >= 20UL) {
            lastFlashTick = millis();
            flashTick();

            // TODO: Every 20ms here check a new GPIO that detects when the Vectrex
            // is powered on.  Use this to exit the ramdisk when sys_opt.usb_dev == USB_DEV_DISABLED
            // Use case: "Forgetful Developer"
            // Vectrex off, plug in USB, forget to compile/flash cart.bin/eject USB,
            // Power on Vectrex without unplugging USB, USB will unmount automatically.
        }

        // Host ejected USB, or Vectrex exited Dev Mode or wants to run cart.bin again
        // at any rate, let's bail from this loop soon!
        if (!exitTimer && (sys_opt.usb_dev == USB_DEV_EJECT ||
            sys_opt.usb_dev == USB_DEV_EXIT ||
            sys_opt.usb_dev == USB_DEV_RUN)) {

            nonBlockingRamdiskTimer = 0;
            exitTimer = millis();
        }

        // Give USB / flash some time to finish up, then exit
        if (exitTimer && (millis() - exitTimer >= 1000UL)) {
            exitTimer = 0; // disable again

            xprintf("Disconnecting USB\n");

            // Disable VBUS sense
            // OTG_FS_GCCFG &= ~OTG_GCCFG_VBUSBSEN;
            // OTG_FS_GCCFG |= OTG_GCCFG_NOVBUSSENS;

            // Enable VBUS sense
            // OTG_FS_GCCFG &= ~OTG_GCCFG_NOVBUSSENS;
            // OTG_FS_GCCFG |= OTG_GCCFG_VBUSBSEN;

            usbd_disconnect(msc_dev, 1); // Disconnect USB
            exit = true;
        }

        // Yield to Vectrex
        if (nonBlockingRamdiskTimer && (millis() - nonBlockingRamdiskTimer >= nonBlockingRamdiskTimeout)) {
            nonBlockingRamdiskTimer = 0; // disable again
            // xprintf("Y2V\n");
            exit = true;
        }
    }

    lastUsbDev = sys_opt.usb_dev;
    return 0;
}
