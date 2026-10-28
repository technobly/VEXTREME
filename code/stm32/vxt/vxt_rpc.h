/*
 * vxt_rpc.h - VXT toolkit: extensible RPC dispatch (STM32 side)
 *
 * Part of the VEXTREME cooperative-multitasking toolkit ("VXT"). GPLv3.
 *
 * PURPOSE: let applications add RPC handlers without editing the
 * doHandleEvent() switch in main.c. Upstream integration is ONE line:
 * route the switch's default case to vxtRpcDispatch(data).
 *
 * ID SPACE:
 *   0-18   upstream firmware (main.c cases through settingsSave)
 *   19-63  application-specific
 *   64-79  RESERVED: VXT toolkit (66 = frame render)
 *   80-255 application-specific
 * Registration of IDs <= VXT_RPC_UPSTREAM_MAX is refused.
 *
 * Timing note: dispatch executes inside doHandleEvent(), i.e. while the
 * romemu.S polling loop is paused and the 6809 is parked in its RAM stub.
 * A table lookup adds nothing measurable there, and the hot ROM-serving
 * path is untouched.
 */

#ifndef VXT_RPC_H
#define VXT_RPC_H

#include <stdint.h>

/* Lowest ID an application may register. IDs at or below
 * VXT_RPC_UPSTREAM_MAX belong to main.c's switch (cases 0..18). */
#define VXT_RPC_UPSTREAM_MAX   18
#define VXT_RPC_MIN_ID         (VXT_RPC_UPSTREAM_MAX + 1)

/* Toolkit-reserved block. */
#define VXT_RPC_TOOLKIT_FIRST  64
#define VXT_RPC_TOOLKIT_LAST   79
#define VXT_RPC_ID_FRAME       66   /* frame-render RPC (Sprite_tm's VOOM ID) */

/* Handler signature. `id` is the dispatched byte; `parm` is the live
 * parmRam[] (256 bytes; 6809-write-only channel - the 6809 cannot read
 * parmRam back; STM32->6809 data must be written into the served image
 * buffer instead). */
typedef void (*vxt_rpc_handler_t)(uint8_t id, volatile uint8_t *parm);

/* Clear the registration table. Call once from main() before use
 * (before the romemu loop is started). */
void vxtRpcInit(void);

/* Register `fn` for `id`. Returns 0 on success, -1 if the id is in the
 * upstream range or already registered (reported over xprintf serial). */
int vxtRpcRegister(uint8_t id, vxt_rpc_handler_t fn);

/* Hook target for doHandleEvent()'s default case. Unregistered IDs are
 * ignored (matching upstream's silent `default: case 0: break;`), with a
 * serial note for debugging. */
void vxtRpcDispatch(int data);

#endif /* VXT_RPC_H */
