/*
 * vxcoop_handler.h - VX-COOP: VXT toolkit reference application, STM32 side.
 * Copyright (C) 2026 Caelotronics.
 * GPLv3.
 *
 * VX-COOP exercises every reusable component of the VXT cooperative dual-CPU
 * toolkit: SmartList drawing, the draw-scale ladder, 3D-to-2D projection,
 * backface culling, edge occlusion, three-channel-plus-noise sound with
 * conflict resolution, controller input, and per-unit calibration. It contains
 * no application content; it runs as a VXE game on the shared kernel.
 *
 * Build:   cd code/stm32 && make vxe-vxcoop USE_HW=v0.3     -> out/vxe/VXCOOP.vxe
 * Cart:    code/app6809/VXCOOP/VXCOOP.asm (packed into the .vxe)
 * Install: copy VXCOOP.vxe to /roms/VXE/ on the card and pick it from the menu
 * Reference guide: docs/VX-COOP/VX-COOP_Reference_Guide.md
 *
 * RPC 78 (per frame) and 79 (one-shot boot init). These are the last two free
 * identifiers in the toolkit-reserved 64-79 block; 64 and 65 are also free.
 * Note that main.c's dispatch hook routes only 64-79 to vxtRpcDispatch(), so
 * the 19-63 and 80+ application ranges described in vxt_rpc.h are unreachable
 * until that test is widened. See the reference guide's RPC chapter.
 */
#ifndef VXCOOP_HANDLER_H
#define VXCOOP_HANDLER_H

#include <stdint.h>

/* Per-frame handler, registered on RPC 78 and driven by VXCOOP.asm's frame
 * loop. */
void vxcoop_handler(uint8_t id, volatile uint8_t *parm);

/* One-shot handler, fired once per 6809 boot (cold or warm reset) on RPC 79.
 * A 6809 warm reset does not reset the STM32, so without this the demo would
 * resume in whatever state it held before the reset. This is also the only
 * function permitted to perform blocking SD reads. */
void vxcoop_init_handler(uint8_t id, volatile uint8_t *parm);

#endif /* VXCOOP_HANDLER_H */
