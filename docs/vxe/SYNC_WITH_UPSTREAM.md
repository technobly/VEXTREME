# Staying in sync with the upstream VEXTREME firmware

This repository is the upstream VEXTREME firmware (technobly/VEXTREME) plus VXE. VXE is a compile switch: `VXE=0`
builds the regular firmware, the same file as upstream, which `tools/check_vxe_off.sh` confirms. Our
changes to upstream's own files are small, add code rather than change it, and sit behind the switch, so merging a
new upstream version is usually clean.

| Upstream file | What we add |
|---|---|
| `code/stm32/Makefile` | `-include config/profile.mk` (an optional local build profile), the `VXE ?= 0` switch (any `vxe-*` target sets it to 1), and two include hooks, `vxe_config.mk` early and `vxe_rules.mk` at the end. Nothing of upstream's is removed. |
| `code/stm32/main.c` | `#include "vxe_config.h"`, then seven `#if VXE_ENABLED` blocks: the VXE header, bundle detection, bundle loading, the game path and high-score CRC, the menu score setter, the RPC 64-79 dispatch, and `/cart.vxe` developer mode with `vxeKernelStart()` |
| `code/stm32/menu.c` | `#include "vxe_config.h"`; both extension lists gain `.vxe` under `VXE_ENABLED` |
| `code/stm32/xprintf.c`, `xprintf.h` | `#include "vxe_config.h"`; under `VXE_ENABLED`, `xvprintf` is exported instead of `static` |
| `README.md` | a note near the top and a link to `docs/vxe/` |

Everything else is new files:
- in `code/stm32/`: `vxe_config.h`, `vxe_config.mk`, `vxe_rules.mk`, the two kernel linker scripts
  (`stm32f411rct6-kernel-uf2.ld`, `stm32f411rct6-kernel-dfu.ld`), and the folders `vxe/`, `vxt/`, `gamelib/`,
  `game/`, `games/` and `config/`;
- in `code/app6809/`: `vxt/`, `vectrex.i`, and the example stubs `Hello/`, `Cal/`, `VXCOOP/` and `VxeMsg/`;
- `docs/vxe/` and the scripts in `tools/`.

We don't delete or prune upstream files. Features are switched off at build time instead.

## Pulling a new upstream version

```
git fetch upstream
git merge upstream/main          # any conflicts are in the files in the table above
tools/check_vxe_off.sh           # VXE=0 must still equal upstream
cd code/stm32
make vxe-all USE_HW=v0.3 VXE_LAYOUT=uf2   # and again with VXE_LAYOUT=dfu
cd ../.. && for l in uf2 dfu; do tools/test_vxe_install.sh <image> $PWD/out/vxe-$l $l; done
tools/test_hello.sh <image>; tools/test_vxe_durable.sh <image>; tools/test_vxe_map_path.sh
```

`<image>` is a FAT32 image, made with `hdiutil create -size 64m -fs "MS-DOS FAT32" -layout NONE -type UDIF card.dmg`.

Then check the places where upstream changes can break VXE:

1. The kernel still fits. In the `uf2` layout it must stay under 112 KB, because sector 5 starts at `0x08020000`;
   the linker stops the build if it doesn't fit. In the `dfu` layout it must stay under 64 KB at `-Os`. It is about
   61 KB now, so a few KB of upstream growth will push it over (see [SDK_GUIDE.md](SDK_GUIDE.md), "Flash layouts and
   size limits").
2. The high-score calls. `vxe/vxe_kernel.c` wraps `highScoreGet` and `highScoreSetGameRecordToDefaults`. If upstream
   changes their signatures or the record type, the wrappers must change too, and `VXE_API_VERSION` must be
   increased.
3. The bootloader. If upstream moves the application address (now `0x08004000`) or changes what the bootloader
   erases, the `uf2` layout's linker scripts and `docs/vxe` must follow.
4. ROM emulation. `romemu.S` is upstream's and VXE doesn't edit it.
