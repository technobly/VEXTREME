# Firmware Updates

## Overview

VEXTREME firmware is updated over USB with a UF2 bootloader. No programmer or special software is needed. When the bootloader is active, VEXTREME shows up as a USB drive named **VX-BOOT**. Copy a `VEXTREME.uf2` firmware image to that drive and it flashes itself and reboots.

There are two ways to get into the bootloader: from the VEXTREME menu, or by copying `VEXTREME.uf2` to the normal VEXTREME drive.

## Getting VEXTREME.uf2

Download `VEXTREME.uf2` from a release, or build it yourself:

```
cd code/stm32
make clean all uf2 USE_HW=v1.0 USE_UF2=1
```

This writes `code/stm32/VEXTREME.uf2`.

## Method 1: From the VEXTREME Menu

1. Turn on the Vectrex with VEXTREME installed.
2. Open the settings: in the game list, move the cursor up onto the VEXTREME logo and press button 4.
3. Select **SYSTEM → UPDATE FIRMWARE → PROCEED?** (button 4 selects each item). VEXTREME immediately reboots into the UF2 bootloader, and the screen shows these instructions:
   - "PLUG IN USB": connect VEXTREME to your computer with a USB-C cable.
   - "OPEN VX-BOOT DRIVE": open the **VX-BOOT** drive that appears on your computer.
   - "COPY VEXTREME.UF2": drag and drop `VEXTREME.uf2` onto the **VX-BOOT** drive.
4. The firmware is flashed and VEXTREME reboots. The VX-BOOT drive disappears on its own; that's expected, and you don't need to eject it. Once the new firmware is running, the Vectrex resets by itself.
5. Check the version shown on the VEXTREME boot screen to confirm the update.

## Method 2: Copy VEXTREME.uf2 to the VEXTREME Drive

You can also start a firmware update without using the menu:

1. Connect VEXTREME to your computer with the Vectrex off. It mounts as the normal **VEXTREME** drive.
2. Drag and drop `VEXTREME.uf2` onto the root of the **VEXTREME** drive.
3. Eject the drive, or turn on the Vectrex.
4. VEXTREME detects the file, deletes it, and reboots into the UF2 bootloader.
5. Open the **VX-BOOT** drive and copy `VEXTREME.uf2` to it again, then continue from step 4 of Method 1.

The file has to be copied twice: the first copy only tells VEXTREME to enter the bootloader, and the second copy (to VX-BOOT) is the one that gets flashed.

## Troubleshooting

- **No VX-BOOT drive appears:** check the USB cable (some cables are charge-only) and try another USB port.
- **The boot screen still shows the old version:** make sure you copied the file to the **VX-BOOT** drive, not the **VEXTREME** drive, and that it is a `.uf2` file, not a `.bin`.
