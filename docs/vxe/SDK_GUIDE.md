# VXE developer guide: building a game for the VEXTREME co-op loader

This guide is for converting a game to run on a VEXTREME cartridge as a co-op game. The STM32 on the cartridge
runs the game's logic, and the Vectrex's 6809 reads the controllers, draws the vectors and plays the sound. The
commands here are the ones used to build games that run on hardware (those games are not in this repository).

VXE is a compile option. `VXE=0` builds the regular VEXTREME firmware, unchanged (section 2, "The VXE switch").
[SYNC_WITH_UPSTREAM.md](SYNC_WITH_UPSTREAM.md) explains how this repository follows the upstream firmware.

Contents: 0 Quickstart - 1 What a VXE game is - 2 Is your game a candidate - 3 Tools - 4 Source layout - 5 The
6809 side - 6 The STM32 side - 7 Registering the game - 8 Build, pack, run - 9 Rules to read first - 10 Testing -
11 What is still missing

---

## 0. Quickstart

`HELLO` is a working template. You move a square with the stick; button 1 saves its position to a file and beeps,
and the position is reloaded at the next start. It uses input, drawing, sound and a saved file in about 200 lines of
C and a 100-line 6809 stub.

```
cd code/stm32
make vxe-hello USE_HW=v0.3     # kernel, payload, 6809 stub and out/vxe-uf2/HELLO.vxe
```

This builds the `uf2` layout. Add `VXE_LAYOUT=dfu` for the other one (section 2 helps you choose). Flash the kernel
from `out/vxe-<layout>/` once (section 8), copy `HELLO.vxe` to a folder under `/roms` on the card,
and pick it in the menu. To start your own game from it:

1. Copy `game/hello.c`, `hello.h` and `hello_vxe.c` to `game/<mygame>.c` and so on. Copy `code/app6809/Hello/` to
   `code/app6809/<MyGame>/`, and `games/hello.mk` to `games/<mygame>.mk`, then edit its five lines.
2. Rename `hello_` to `<mygame>_` and `HELLO_` to `<MYGAME>_`. Change the two RPC IDs in `<mygame>.h` and in the
   `.asm` (they must agree, and be in the range 64-79). Change the title string in the `.asm` header.
3. Run `make vxe-<mygame> USE_HW=v0.3`. No Makefile edit is needed.
4. Replace the body of `hello_frame_handler()` with your game.

`make vxe-all` builds every registered game. `make vxe GAME=<name>` is the same as `make vxe-<name>`.

## 1. What a VXE game is

A VEXTREME cartridge has an STM32F411 that serves the Vectrex cartridge bus like a ROM. A co-op game has two
programs:

| Part | Runs on | Typical size | Built from |
|---|---|---|---|
| 6809 image | the Vectrex | 600-650 bytes | `code/app6809/<game>/<game>.asm` |
| Payload | the STM32 | 13-190 KB | `code/stm32/game/*` and the VXT toolkit |

Both are packed into one `<NAME>.vxe` file. The kernel (`stm32_kernel.bin`) is the same firmware for every game and
contains no game code. When you pick a `.vxe` in the menu, the kernel installs the payload into internal flash
(unless the same payload is already there), serves the 6809 image as a normal cartridge, and passes the game's RPCs
to the payload. A payload is compiled for one flash address, so a `.vxe` carries one payload for each flash layout
it supports (section 2). The 6809 image is shared.

Each frame:

```
6809: Wait_Recal -> read controllers -> RPC "frame" (waits) -> draw the list
STM32 (while the 6809 waits): read inputs from parmRam, advance the game one
      tick, write the frame's SmartList into the served image at $0800
```

The 6809 does not run game logic in co-op games. The SmartList is a list of 4-byte records (`y, x, routine address`). The 6809's
`vxt_smart` engine plays it back with one `pulu a,b,pc` per record.

The memory map the 6809 sees (the served image, `code/stm32/vxt/`):

| Address | Contents |
|---|---|
| `$0000-` | the game's 6809 code (a few hundred bytes) |
| `$0700` | optional: a high-score staging block for the BIOS score display (`[magic $A5][7 bytes]`); the examples here don't use it |
| `$0800-$1FFF` | SmartList (`VXT_SMART_OFFSET`), up to about 1536 records |
| `$2000-` | sound register/value pairs (`VXT_SND_OFFSET`) |
| `$7F00-$7FFF` | `parmRam`, 256 bytes, written by the 6809 and not read back by it: the 14 `SM_*` routine addresses at `$7F00`, the input bytes at `parmRam[245..254]`, and the RPC id at `$7FFF` |

The served image is 64 KB, with the upper 32 KB mirroring the lower (`vxtImgPut()` writes both).

## 2. Is your game a candidate?

A game fits if its game logic can run on the STM32 and its drawing on the 6809:

* The STM32 handles state, collision, AI, projection, text layout and sound sequencing. It is a 120 MHz Cortex-M4
  with an FPU; the 6809 is a 1.5 MHz 8-bit CPU.
* The 6809 has a fixed budget of about 1,500 SmartList records per frame. The region from `$0800` to `$2000` holds
  1,536 four-byte records, with one slot kept for the terminator. With more records the 6809 cannot finish before
  the next frame, and the picture flickers.
* Input arrives as the controller state each frame (`parmRam[245..254]`).
* Sound goes out as AY register writes that the 6809 plays (3 channels).
* Files on the card hold config and high scores. Read and write them occasionally, not every frame.

A game does not fit if it needs the 6809 itself for real-time work the STM32 cannot do (raster tricks, per-scanline
effects), or needs more than about 1,500 vectors per frame with no way to cull them.

### The VXE switch

`make ... VXE=1` builds the VXE kernel, and so does any `make vxe-...` target. A plain `make` (`VXE=0`, the default)
builds the regular VEXTREME firmware, and the result is the same file as the upstream firmware: `tools/check_vxe_off.sh` builds both and compares
their checksums. Every VXE hook in the firmware's C files is behind `VXE_ENABLED` (`vxe_config.h`,
off unless the Makefile turns it on), and the Makefile reads `vxe_config.mk` and `vxe_rules.mk` only when `VXE=1`.

### Flash layouts and size limits

The F411 has 256 KB of flash in six sectors (16, 16, 16, 16, 64 and 128 KB), and a sector can only be erased whole.
There are two layouts, chosen at build time with `VXE_LAYOUT=uf2|dfu` (default `uf2`):

| | `uf2` (default) | `dfu` |
|---|---|---|
| Bootloader | VEXTREME 1.0 UF2 bootloader in sector 0 | none; flash with `dfu-util` |
| Kernel | from `0x08004000`: sectors 1-3 and the start of sector 4 (up to 112 KB), built `-O3` like upstream | sectors 0-3 (64 KB), built `-Os` (about 61 KB) |
| Payload slot | sector 5: `0x08020000`, 128 KB | sectors 4-5: `0x08010000`, 192 KB |
| Kernel updates | copy `VEXTREME.uf2` to the VX-BOOT drive; the payload in sector 5 is kept | `dfu-util` |
| Unused flash | the rest of sector 4, which cannot be erased separately from the end of the kernel | none |

| | Limit (`uf2` / `dfu`) |
|---|---|
| Payload in flash | 131,072 B / 196,608 B, including the 28-byte header and the initial-data image |
| Payload RAM (`.data` + `.bss`) | 16,384 B in both |
| 6809 image | 64 KB served image; a game's own code is a few hundred bytes |

A `.vxe` carries one payload for each layout the game supports, and the kernel installs the one for its own layout.
A game that fits both slots is one file that runs on either kernel. A game that fits only the 192 KB `dfu` slot
carries only that payload (`GAME_LAYOUTS := dfu` in its `games/<name>.mk`). A `uf2` kernel refuses it and shows a
message on the Vectrex screen, for example "MYGAME only loads on the non-UF2 kernel (192 KB payload)".

Choosing a layout:

- Use `uf2` unless you need the space. It is the default, kernel updates are a file copy with no jumper, and installed
  games survive them. A game must fit 128 KB to run on it.
- Use `dfu` when a game does not fit 128 KB, or when you want the full 192 KB slot for development. The cost is that
  every kernel update needs `dfu-util` and the BOOT0 jumper, and the 64 KB kernel is built `-Os`.
- Build games with `GAME_LAYOUTS` left at both layouts when they fit, so one `.vxe` runs on either kernel. Switching a
  board between layouts needs only a kernel flash; the `.vxe` files on the card stay as they are.
- To make `dfu` your default without typing `VXE_LAYOUT=dfu` each time, create `code/stm32/config/profile.mk`
  containing `VXE_LAYOUT ?= dfu`. The Makefile includes it if it exists, and `code/stm32/config/README.md` describes
  it. The public tree does not ship one.

The kernel shows a message cartridge like this for any file it refuses: wrong layout, damaged or old-format file,
wrong kernel version, or a failed install. RESET returns to the menu. Its source is `code/app6809/VxeMsg/`, built
into `vxe/vxe_msgcart.h` by `tools/gen_vxe_msgcart.sh`. `tools/make_vxe.py info GAME.vxe` lists the layouts a file
carries.

If a game outgrows its slot, you can build it with `-Os` instead of `-O2` (`PAYLOAD_CFLAGS` in `vxe_rules.mk`). That makes it
smaller, but it can change STM32 frame time, so check the game's timing on hardware.
You can also move constant data out, or trim what the toolkit links.

Converting a native 6809 game means rewriting its game logic in C. One shipped port was written from the 6809
source, another from a 6502 disassembly. The 6809 side of the new game is thin and nearly the same file for every
game, so most of the work is in the C. The game's own graphics code is replaced by calls into the toolkit
(section 6).

## 3. Tools you need

These were used on a Mac to build the shipped files:

| Tool | Used for | Where to get it |
|---|---|---|
| Docker | the STM32 build (`stm32-build` image: arm-none-eabi-gcc 10.2.1 and libopencm3 at a fixed commit) and the 6809 assembler image | `docker build code/stm32 -t stm32-build`; the `asm6809` image is built by `code/menu/Makefile` (`docker-build`) |
| asm6809 | the 6809 side | the Docker image `asm6809`, or `brew install asm6809` |
| python3 | `tools/make_vxe.py`, the packer (standard library only) | system |
| dfu-util | flashing the kernel | `brew install dfu-util` |
| libopencm3 | building the kernel; not needed to build or test a game on the host | the commit pinned in the repository's Dockerfile (`90753950`) |

The `make` targets run Docker for you. To build a 6809 stub on its own:

```
docker run --rm -v "$PWD/code/app6809":/build -w /build -u $(id -u):$(id -g) \
  asm6809 asm6809 -B -o MyGame/MyGame.bin MyGame/MyGame.asm
```

The board revision is chosen at compile time: `USE_HW=v0.3` (or `v0.4`, `v1.0`) must match your board, or the
firmware hangs waiting for a signal that is not asserted. A payload does not use `HW_VER`, so one `.vxe` runs on any
board revision. Only the kernel is built per board.

## 4. Source layout

```
code/stm32/
  vxe/            the VXE boundary: vxe_api.h (the interface), vxe_redirect.h,
                  vxe_shim.c, vxe_payload.c (+ vxe_payload_uf2.ld and
                  vxe_payload_dfu.ld), vxe_kernel.c,
                  vxe_install.c, host tests
  vxt/            the VXT toolkit, STM32 side (SmartList, text, sound, music, RPC...)
  gamelib/        optional drawing math built on vxt_smart (3D projection, big lines)
  game/           your game: <game>.c ..., plus <game>_vxe.c (the entry file)
  main.c ...      the kernel (no changes needed for a new game)
code/app6809/
  vxt/            the VXT toolkit, 6809 side (include it, don't copy it)
  vectrex.i       BIOS equates
  <Game>/<Game>.asm   your 6809 stub
tools/            make_vxe.py, pack_vxe.sh, test scripts
```

A new game needs no change to the kernel (`main.c`, `menu.c`, `romemu.S`, FatFs, USB).

## 5. The 6809 side

Copy `code/app6809/Hello/Hello.asm`. `VXCOOP/VXCOOP.asm` has the same structure for a larger demonstration. Change
only these:

* The cartridge header. The title string at offset `$11` is the game's high-score key in the VEXTREME store. It must
  be non-empty and unique (`"MYGAME V1"`, `$80`, `0`).
* The two RPC IDs. Pick two unused IDs in the range 64-79, the only range the kernel passes to a payload. The macros
  write them to `RPC_ID_ADDR` (`$7FFF`). The examples use HELLO 64/65, the calibration rig 75/76 and the VX-COOP demo
  78/79, and every game makes the SmartList address handshake on 69. Only one payload is loaded at a time, so
  different games can reuse IDs.
* The three `vxt_smart.asm` switches. Every app must define all three before including the file: in asm6809 an
  undefined symbol in an `if` counts as false, with no warning.

  | Switch | Meaning | For a new game |
  |---|---|---|
  | `VXT_BLANK_BEFORE_ZERO` | blank the beam before asserting ZERO in recenter | `1` (no cost; removes tails at line ends) |
  | `VXT_BLANK_AT_EXPIRY` | blank at Timer-1 expiry in the huge draws | `1` (6 cycles per huge record; removes bright dots where chained lines join) |
  | `VXT_MOVEHUGE` | assemble the blanked move that polls the timer; must be `1` if published | `1` |

  Games tested before these switches existed use `0/0/0`, as do the calibration rig and the VX-COOP demo. They showed
  no visible difference either way.

The 6809 program is this frame loop:

```
main:   VXT_RPC_INIT / VXT_SOUND_INIT / VXT_INPUT_INIT
        ... store the 14 SM_* routine addresses at $7F00.. then
        VXT_RPC #69                 ; address handshake
        VXT_RPC #<INIT_ID>          ; once per 6809 boot (the STM32 resets state
                                    ;  and does all its file I/O here)
frame:  jsr Wait_Recal
        VXT_INPUT_READ Joy_Digital  ; or Joy_Analog
        VXT_RPC #<FRAME_ID>         ; waits until the STM32 has built the list
        (copy the staged high score into Vec_High_Score - optional)
        jsr vxt_sound
        ldu #$0800
        jsr vxt_smart               ; play the SmartList
        bra frame
        include "vxt/vxt_rpc_stub.asm"   ; must be the last thing in the file
```

Two rules keep the stub safe, and breaking either froze or crashed the board. Put `jmp main` right after the
header rather than falling through to it, and put the RPC stub last in the file. The stub runs from RAM because
ordinary ROM bytes are not served while the STM32 is computing.

## 6. The STM32 side

You write a few C files under `code/stm32/game/`. Two things are required.

The entry file `<game>_vxe.c` registers the RPCs. For HELLO the file is `game/hello_vxe.c`:

```c
#include "hello.h"
#include "../vxe/vxe_game.h"
#include "../vxt/vxt_rpc.h"

void vxe_game_init(void)
{
    vxtRpcRegister(HELLO_RPC_FRAME, hello_frame_handler);   /* 64: per-frame tick + compose */
    vxtRpcRegister(HELLO_RPC_INIT,  hello_init_handler);    /* 65: one-shot init per 6809 boot */
}
```

Two handlers, each with the signature `void h(uint8_t id, volatile uint8_t *parm)`:

* init runs once per 6809 boot. A 6809 warm reset does not reset the STM32. Reset the game state here, load config,
  calibration and high scores from the card, and keep what you read in statics. Do file reads here and nowhere else.
* frame runs every frame. Read inputs from `parm[]`, advance one tick, build the list and queue the sound. No file
  I/O.

A frame handler in outline:

```c
vxtSmartBegin(REGION_OFFSET, REGION_RECORDS);    /* bound = the sub-region's capacity */
  ... game tick; draw with vxtSmartMove/Draw/DrawBig, vxtSmartText*, gamelib* ...
vxtSmartRecenter();
vxtSmartEnd();                                   /* required */
/* then sound: vxtSoundBegin()/... vxtSoundEnd() */
```

What the toolkit provides (headers in `code/stm32/vxt/` and `gamelib/`):

| Need | Use |
|---|---|
| Lines, moves, text, numbers | `vxt_smart.h`, `vxt_smart_text.h` (the Western font is included in every build; a second font can be registered, see below) |
| 3D projection, long lines, closed shapes | `gamelib_proj3d.h`, `gamelib_beam.h` (see `API_REFERENCE.md` and the header comments) |
| Sound effects and music | `vxt_sound.h`, `vxt_music.h` (`tools/vpy_to_music.py` converts songs) |
| Inputs | `vxt_input.h`: `VXT_IN_JOY1X/Y`, `VXT_IN_BTN1_1..4`, `VXT_IN_EDGE` |
| Calibration for the console's beam error | `vxt_cal_load.h` (the rig itself is `vxt_cal.c`) |
| Culling or clipping against a window (cockpit frame, round viewport, HUD box); near-plane clipping | `gamelib_clip.h` |
| Rotating a model; hiding the back faces of a convex solid | `gamelib_cull.h` |

### Optional building blocks

Every payload includes input, sound, and the SmartList draw engine with the Western font. Everything else is linked
only if you call it, because the linker (`--gc-sections`) drops what is unused. The blocks are plain C with no hidden
state and no I/O, so they are safe to call every frame.

- Overlay font. A game that needs a second alphabet supplies its own glyph table and registers it from
  `vxe_game_init()` with `vxtSmartTextRegisterOverlayFont()`.
- `gamelib_clip.h`. `gamelibClassifyCircle` tests whether an object is inside, outside or straddling the window,
  using a bounding circle against a disc or a convex polygon, with an optional rectangular HUD hole that hides what
  is behind it. For a straddling object, `gamelibClipSegment` and `gamelibClipSegmentHole` return the visible part
  of each edge (the hole version can return two pieces). `gamelibPointInWindow` tests a single point. For 3D,
  `gamelibClipEdgeNear` cuts an edge at the camera's near plane. Call it before any window test: a vertex behind the
  eye projects mirrored and greatly magnified. A polygon window must contain the beam origin (0,0); if yours does
  not, subtract its centre from every coordinate.
- `gamelib_cull.h`. `gamelibCullFaces` decides which faces are visible using the perspective test, and
  `gamelibEdgeVisible` turns two face flags into an edge decision. This gives correct hidden-line removal only for a
  closed convex mesh. Non-convex models, such as wings on struts, need real occlusion testing. The per-model data
  (normals, plane constants, edge-to-face table) comes from `tools/obj_cull_tool.py`; see convention 17 in
  [CONVENTIONS.md](CONVENTIONS.md). Culling removes edges, so some chained lines no longer join and must be
  repositioned, but the record count still falls by about half.

These blocks were taken from a shipped game's code, with its constants turned into parameters. Each function was
checked to give bit-identical results to the original over 200,000 random inputs, for both the circular and the
polygon window. The comparison harness needs that game's source, so it is not in this repository. Linking the
blocks into a game that does not call them changes its payload by zero bytes. Random inputs rarely hit an
exact floating-point tie, so a `>` changed to `>=` would still pass that test. For that reason the arithmetic is kept in
the original's order; please don't reorder it.

### What a payload can call in the firmware

Only the functions in the table in `vxe/vxe_api.h` (`VxeKernelApi`):
- FatFs `f_open`, `f_close`, `f_gets`, `f_write`, `f_sync`, `f_rename` and `f_unlink`;
- `flashDoWriteback`;
- the four LED calls, `millis`, `dwt_read_cycle_counter` and `xprintf`;
- `highScoreGet`, `highScoreSetGameRecordToDefaults` and `highScoreSave`, plus the menu score setter;
- pointers to `romData`, `cartData`, `parmRam` and `pActiveGameData`.

Your sources call these by their normal names: `vxe/vxe_redirect.h` is included into every payload object and
renames them to stand-ins that call through the table. libc, libm and libgcc link into the payload. The payload is
linked without the firmware, so the build fails on any other firmware symbol. If you need one, add it to the table
(append only, and increase `VXE_API_VERSION`).

### Calibrating for the console

A Vectrex's analog parts age, so no two consoles draw quite alike. Two measured corrections matter to every game
(the numbers are in `gamelib/gamelib_beam.h`):
- Draw gain. A drawn line covers about 5% less distance than a blanked move with the same command. Without the
  correction, every edge ends short and the corners of a box don't meet.
- Centre offset. The screen's centre is offset from the DAC's zero. Without the correction, the margins are
  wide on one side and narrow on the other.

The calibration rig (the `CAL` game, `games/cal.mk`) measures both. Run `CAL.vxe` once per console. It stores the
results in `/calmeas.csv` at the card root, shared by every game; `vxtCalLoadSetSource()` points the loaders at a
different file. Load the values once, in the init handler, with `vxtCalLoadDrawGain` and `vxtCalLoadOffset` (each
keeps your default if the console has never been measured). Apply them every frame with `gamelibBeamSetDrawGain()`
and `gamelibBeamSetOffset()`, which are cheap and do no file I/O. `game/hello.c` shows this. The first HELLO build
left it out, and on the test console it had a small gap at every corner and uneven margins.

### Files

Name a game's files `"/name"`, as if at the card root. The kernel opens them in the folder the game was launched
from, so `MYGAME.vxe` in `/roms/VXE/` keeps its files there. Don't build paths yourself. The calibration files
`calmeas.csv` (written by the calibration rig) and `cdfmeas.csv` (an older name still found on cards) stay at the
card root and are shared by all games.

### Saves

The cart stores files in SPI flash behind a one-block RAM cache, which is written out only when another block is
used. The kernel makes `f_close`, `f_sync`, `f_rename` and `f_unlink` write the cache out before they return, and a
game can also call `flashDoWriteback()` itself (HELLO does). Either one is enough. A game that also has to run
without VXE should call `flashDoWriteback()`.

Writing to the flash chip briefly disturbs the picture. On hardware, erasing and programming it makes a ghost of
the current drawing flash near the screen centre just after the write. Reading the file, pausing for the same 100 ms,
playing a sound, drawing a second box and changing the beam state did not cause it.
To hide it, send a few empty frames straight after a save, with the beam dimmed so they leave no dot
(`vxtSmartIntensity(0)`, then `vxtSmartRecenter()`). HELLO sends 8; 3 were enough on the test console. Better still,
save at a quiet moment, such as between levels, at game over or from a menu. This applies to every save, including
the calibration screens.

Known limit: on the test console a small dot stays at the screen centre while the save and the empty frames run.
Dimming the beam during the empty frames did not remove it, and neither did blanking and dimming before each wait
for the STM32. The cause is not known. It is brief and harmless, and one more reason to save outside play.

### High scores

Call `highScoreGet(name, NULL)`, then `highScoreSave(ascii6)` with 6 space-padded digits. Use the same title bytes
as the 6809 header; reading them from `&cartData[0x11]` keeps the two the same. To show the score in the menu
afterwards, call `vxe_menuScoreSet()`. The firmware keys high scores by ROM checksum, and the kernel adds it for
you, so the calls take the same two arguments as in the firmware.

## 7. Registering the game

One file registers a game: `code/stm32/games/<name>.mk`. Nothing else in the build changes (the Makefile includes
`games/$(VXT_GAME).mk`). Its variables:

```make
GAME_TITLE     := MYGAME            # the bundle is named MYGAME.vxe
GAME_OBJS      := game/mygame.o game/mygame_vxe.o ...   # every object the game needs
GAME_CFLAGS    := -DVXT_GAME_MYGAME=1                   # extra compiler flags (may be empty)
GAME_6809_DIR  := MyGame            # code/app6809/<dir>/<asm>.asm
GAME_6809_ASM  := MyGame
GAME_LAYOUTS   := uf2 dfu           # optional; the flash layouts to build the payload for (default: both)
```

`GAME_CFLAGS` can also pin toolkit behaviour a game was tested with. For example, `-DVXT_LAZY_SCALE=0` builds
`vxt_smart.c` without its lazy-scale optimisation, which is on by default. A new game normally leaves this alone.
`games/hello.mk`, `games/cal.mk` and `games/vxcoop.mk` are examples.

`VXT_GAME` selects the game a build links. If the file is missing, `make` stops and lists the registered games. The
rest of a game is its 6809 stub and its entry file (`<game>_vxe.c`), covered in sections 5 and 6.

## 8. Build, pack, run

```
cd code/stm32
make vxe-mygame USE_HW=v0.3                  # uf2 kernel + payloads + 6809 stub -> ../../out/vxe-uf2/MYGAME.vxe
make vxe-mygame USE_HW=v0.3 VXE_LAYOUT=dfu   # dfu kernel                       -> ../../out/vxe-dfu/MYGAME.vxe
```

`VXE_LAYOUT` chooses which kernel is built and where the output goes (section 2 explains how to choose, and how to
set a default in `config/profile.mk`). The `.vxe` is the same either way. The target
builds the payload once for each layout in `GAME_LAYOUTS` (the code depends on its flash address, so it is rebuilt
for each), assembles the 6809 stub with the Docker `asm6809`, and packs them into one bundle with
`tools/make_vxe.py`, copying `stm32_kernel.bin` next to it. To pack payloads you have already built
(`payload_<name>_uf2.bin`, `payload_<name>_dfu.bin`), use `tools/pack_vxe.sh <payload dir> <out dir>`, which reads
the registry.

Every target cleans first. Build options are `-D` flags and nothing tracks a change in them, so without a clean a
changed flag could link old objects. This is also why `vxe-all` builds its games one at a time. The build prints
each image's size. The payload slot is 128 KB in the `uf2` layout and 192 KB in the `dfu` layout.

On the card:

1. Once per board, put the board in DFU mode (BOOT0 jumper seated before power-on) and flash:
   - `uf2` layout: the bootloader, then the kernel.
     ```
     dfu-util -a 0 -d 0483:df11 -s 0x08000000 -D code/uf2-stm32f/bootloader.bin
     dfu-util -a 0 -d 0483:df11 -s 0x08004000:leave -D stm32_kernel.bin
     ```
     Later kernel updates need no jumper: copy `VEXTREME.uf2` (built next to the kernel) to the VX-BOOT drive (see
     `docs/firmware-updates.md`).
   - `dfu` layout: the kernel at the start of flash.
     ```
     dfu-util -a 0 -d 0483:df11 -s 0x08000000:leave -D stm32_kernel.bin
     ```
2. Copy `MYGAME.vxe` anywhere under `/roms` (the menu lists `.bin`, `.vec` and `.vxe` files). Eject the card on the
   Mac before playing.
3. Pick it in the menu. The first time, the LEDs turn magenta while the payload is written: about 1 s for a small
   game, 2 s near the 192 KB limit. Picking the same game again starts it at once.

Shortcuts for development: `/cart.vxe` at the card root starts that game at power-on (like `/cart.bin`). After a
6809-only change, copying the new `.vxe` is enough. To flash a payload without the card, use the slot's address
(`0x08020000` in the `uf2` layout, `0x08010000` in `dfu`). This path does not need the packer's identity stamp:

```
dfu-util -a 0 -d 0483:df11 -s <slot address> -D payload_mygame_<layout>.bin
```

## 9. Rules to read first

The full list is in [CONVENTIONS.md](CONVENTIONS.md). Start with these:

1. No blocking I/O on the frame path. An `f_open`, `f_write` or flash write takes about 100 ms, which at 50 Hz is
   five lost frames, and the picture flickers. Load in the init handler and keep the result; save only on a player
   action or at the end of a game.
2. The budget is about 1,500 records a frame, and it is fixed. A chained edge costs about 3 records, an independent
   line 9-14. Measure with `vxtSmartRecordCount()`.
3. Don't add records to gain precision (convention 18). Search the scale for the best landing point instead.
4. `extern` declarations of shared data must match the firmware's type (array or pointer).
5. A draw region's bound is the capacity of that sub-region, not the whole image.
6. A payload cannot reach `romemu`'s timing-critical path, but don't busy-wait in a handler for longer than a frame.
7. RAM is short. A payload has a fixed 16 KB RAM window (check `_ebss` in your map), and the kernel keeps
   about 13 KB of stack. Use the narrowest types that fit.
8. The title is a key. The 6809 header title must be unique and match the name the C side uses.
9. The 6809 stub's two safety rules (section 5).
10. Flash writes disturb the picture. Send a few empty, dimmed frames after one, and save at a quiet moment
    (section 6).
11. There is no simulator for the cartridge-bus timing or for how the picture looks. A clean build and passing host
    tests are not enough on their own: flash the game and look.

## 10. Testing

On the Mac, without hardware:
* `tools/test_vxe_install.sh` tests the loader, with test bundles and a simulated flash slot.
* `tools/test_vxe_durable.sh` tests that saves survive a power cut, against a model of the cart's flash cache.
* `code/stm32/vxe/test_vxe_map_path.c` tests which folder a game's files go to.
* `tools/test_hello.sh` tests the template, and is the one to copy for your own game. It links the game, the
  SmartList code, the shim, the kernel table and FatFs on the Mac. It decodes the frame the handler produces and
  checks that it is terminated, stays inside its region, uses only published routines and has a sensible record
  count. It drives the game with a simulated stick, and checks that a save lands in the game's folder and survives a
  power cut.

These tests compile the kernel sources with the FatFs in the tree, on a FAT32 image made with
`hdiutil create -size 64m -fs "MS-DOS FAT32" -layout NONE -type UDIF card.dmg`. macOS libc has its own
`xprintf(3)`, so a host test that links kernel code must define `xprintf` itself.

Checks you can automate:
* Count the entries in generated data rather than trusting a total written in a comment.
* After rewriting code that contains conditionals, diff its control flow against the old version.
* Grep the frame handler for `f_open|f_gets|f_write|flashDoWriteback`; there should be no hits.
* Test a new check by breaking the thing it checks, and confirm it fails.

On hardware: run the game from a freshly installed `.vxe`, then power-cycle while it is running and confirm saved
files are still there.

## 11. What is still missing

Hardware status: tested on a v0.3 board in both flash layouts. The HELLO template, the calibration rig, the VX-COOP
demo and larger games that are not in this repository install and run, and saves persist. A kernel update through the UF2 bootloader leaves
the installed payload in place. The `VXE=0` build is the same file as the upstream firmware. Boards other than v0.3
have not been tested: the build accepts `USE_HW=v0.1` to `v1.0`, but a wrong choice hangs the firmware until it is
reflashed, and the project has no other revisions to test on.

Open items, roughly in order of importance:

1. The UF2 layout has only run on a v0.3 board. The bootloader comes from the v1.0 hardware generation. It works on
   v0.3, but it has not been tried with VXE on a v0.4, v0.5 or v1.0 board.
2. There is no emulator for the cartridge. The host tests cover the loader, the file-path mapping, save durability
   and the template's frame logic, but not the cartridge bus, USB, the LEDs or the picture.
3. Signing. `.vxe` files are CRC-checked, not authenticated. The bundle header has reserved words for a signature
   or encryption. Whether the loader should require either has not been decided.
4. Platforms. Everything was built and tested on macOS. The Docker route and the standard Arm toolchain should also
   work on Linux and Windows (WSL), but this is untested. The host tests need macOS (`hdiutil` makes their FAT
   image).
5. The API reference is only as complete as the header comments. `API_REFERENCE.md` is generated from them by
   `tools/gen_api_ref.py`, so a function with no comment shows only its signature.
6. A second example. HELLO is kept small. `game/vxcoop_handler.c` (the VX-COOP demo) shows input, text, sound, music
   and 3D drawing. The culling and clipping blocks (`gamelib_cull.h`, `gamelib_clip.h`) have no example game here.
7. Kernel changes. A payload can only use what is in the kernel table (`vxe/vxe_api.h`, versioned; the oldest
   version accepted is `VXE_API_MIN_VERSION`). A game that needs a firmware service the table lacks, such as a new
   peripheral, needs a new table entry and a new kernel release.

Not planned: a prebuilt toolkit library. Each payload compiles its own copy of `vxt/` and `gamelib/` in a few
seconds, and with a library every payload would still need relinking to pick up a fix.

Licence: VEXTREME and the VXT toolkit are GPLv3, so a game linked with them is distributed under it. Check the licence terms, or arrange a licence exception, before any commercial use. FatFs has its own permissive licence in its source headers.
A game's own artwork, music, names and trademarks have their own rights, which this repository's licence does not
cover.
