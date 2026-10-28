# VXE: loadable co-op game payloads for VEXTREME

VXE supports the loading and execution of  **co-op games** without having to reflash the VEXTREME firmware. In a co-op game, 
the STM32 on the cartridge cooperates with the Vectrex 6809: the STM32 runs the game's logic and builds every frame as a
list of vectors, and the Vectrex's 6809 reads the controllers and draws that list, and plays sound. Co-op games leverages 
the STM32 together with the 6809, tasking the Vectrex's 6809 to perform reading player input, drawing vectors lines and 
playing game sounds.

With VXE:

* the firmware (the **kernel**) is the same file for every game and contains no game code;
* each game is one **`.vxe` file** on the card contianing: a 6809 (Vectrex side) program plus the STM32 code (the **payload**);
* when a user selects a `.vxe` file in the menu, the kernel writes the co-op game payload into spare internal flash 
(skipped if that exact payload is already there) and starts it. Switching co-op games takes about a second (for loading the payload); 
relaunching the same game is instant.

Plain ROMs, the menu and every other feature of the firmware work as before.

## Flashing the kernel (DFU commands)

Put the board in DFU mode first: seat the BOOT0 jumper, then power the board on. `dfu-util -l` should list a
`0483:df11` device. Run the commands from the repository root. The kernels are in `out/vxe-<layout>/` after a build.

**UF2 layout** (UF2 bootloader plus kernel, 128 KB game slot):

```
# once per board: the bootloader
dfu-util -a 0 -d 0483:df11 -s 0x08000000 -D code/uf2-stm32f/bootloader.bin
# then the kernel
dfu-util -a 0 -d 0483:df11 -s 0x08004000:leave -D out/vxe-uf2/stm32_kernel.bin
```

With the bootloader in place, later kernel updates need no jumper and no `dfu-util`: copy `out/vxe-uf2/VEXTREME.uf2`
to the VX-BOOT drive.

**DFU layout** (no bootloader, 192 KB game slot):

```
dfu-util -a 0 -d 0483:df11 -s 0x08000000:leave -D out/vxe-dfu/stm32_kernel.bin
```

* `:leave` makes the board exit DFU mode and start the new kernel. Remove the BOOT0 jumper before the next power cycle,
  or the board goes back into DFU mode.
* To move a board from one layout to the other, put it in DFU mode and flash the other layout's files as above. The DFU
  layout overwrites the bootloader, so going back to the UF2 layout means flashing the bootloader again.
* A `.vxe` file does not need re-copying when you change layout; the same file runs on either kernel, except a game that
  is too big for the 128 KB slot, which only runs on the DFU layout.

## VXE as a compile option

`make ... VXE=1` builds the VXE kernel, and so does any `make vxe-...` target. A plain `make` (`VXE=0`, the default) builds the regular VEXTREME
firmware, **byte for byte the same file as the upstream firmware** - `tools/check_vxe_off.sh` builds both and
compares their checksums. All VXE hooks in the firmware's own source are behind `VXE_ENABLED`, which only the
`VXE=1` build turns on. See [SYNC_WITH_UPSTREAM.md](SYNC_WITH_UPSTREAM.md) for the short list of upstream files this
repository touches and how it follows upstream.

## Two flash layouts

The STM32F411 has 256 KB of flash in six sectors, and a sector can only be erased whole. Choose at build time
(`VXE_LAYOUT=uf2|dfu`):

|                     | `uf2` (default)                                                      | `dfu`                            |
|---------------------|----------------------------------------------------------------------|----------------------------------|
| Bootloader          | VEXTREME 1.0 UF2 bootloader in sector 0                              | none (flash with `dfu-util`)     |
| Kernel              | from `0x08004000`, up to 112 KB, built like upstream (`-O3`)         | sectors 0-3 (64 KB), built `-Os` |
| Game payload        | **128 KB** (sector 5)                                                | **192 KB** (sectors 4-5)         |
| Updating the kernel | drag `VEXTREME.uf2` onto the VX-BOOT drive; installed games are kept | `dfu-util`                       |

A `.vxe` carries one payload for each layout the game supports, and a kernel installs the one for its own layout. A game
that fits both slots is a single file that runs on either kernel; a game that only fits the `dfu` slot carries only that
payload, and a `uf2` kernel refuses it with a message. Details, sizes and flashing
steps are in [SDK_GUIDE.md](SDK_GUIDE.md), section 2 and section 8.

## Test compilation

```
cd code/stm32
make vxe-hello USE_HW=v0.3          # kernel + the HELLO template game -> ../../out/vxe-uf2/
```

Flash `stm32_kernel.bin` once, copy `HELLO.vxe` to the card, pick it in the menu. Build every example with
`make vxe-all USE_HW=v0.3`. `USE_HW` must match your board (it is chosen at compile time).

## Contents of this repository

| Path                                     | What                                                                                                                                                                          |
|------------------------------------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `code/stm32/vxe/`                        | the kernel/payload boundary: the API table, the `.vxe` installer, the payload entry and its linker scripts, the host tests                                                    |
| `code/stm32/vxt/`, `code/stm32/gamelib/` | the VXT toolkit a game links: input, sound, music, the SmartList draw engine and text, calibration loaders; drawing math, 3D projection, window clipping and backface culling |
| `code/stm32/game/`, `code/stm32/games/`  | the example games and their one-file registrations: `hello` (a small template), `cal` (the calibration rig), `vxcoop` (a toolkit demonstration)                               |
| `code/app6809/`                          | the 6809 side: `vxt/` (the engine) and a stub for each example                                                                                                                |
| `tools/`                                 | the `.vxe` packer, the host tests, a convex-mesh culling tool, the VXE-off proof and the leak check                                                                           |

## Additional Info

* [SDK_GUIDE.md](SDK_GUIDE.md) - how to build a game.
* [CONVENTIONS.md](CONVENTIONS.md) - the engineering rules.
* [API_REFERENCE.md](API_REFERENCE.md) - generated from the headers.
* [SYNC_WITH_UPSTREAM.md](SYNC_WITH_UPSTREAM.md) - following the upstream firmware.
* The pre-VXE background (the cooperative CPU design, the wire format) is described in the earlier
  [VEXTREME-VXCOOP](https://github.com/jonskiv/VEXTREME-VXCOOP) repository; the loader described here supersedes its
  "game compiled into the firmware" approach.

## Status and limits

Tested on a v0.3 board in both layouts. Other board revisions build but have not been tested. The complete list is at the end of
[SDK_GUIDE.md](SDK_GUIDE.md).

## Licence

GPLv3, like the firmware it extends. A game linked with the toolkit is distributed under it. A game's own artwork,
music and names have their own rights, which the licence here does not cover. This repository contains example and
template games only.
