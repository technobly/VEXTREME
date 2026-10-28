/*
 * vxt_rpc.c - VXT toolkit: extensible RPC dispatch (STM32 side). GPLv3.
 *
 * See vxt_rpc.h for the contract. Error reporting uses the firmware's
 * existing xprintf serial debug path (as main.c does throughout).
 */

#include "vxt_rpc.h"
#include "../xprintf.h"

/* parmRam is owned by main.c: `unsigned char parmRam[256];` (main.c:68).
 * romemu.S stores 6809 writes to $7F00-$7FFE into it. */
extern unsigned char parmRam[256];

static vxt_rpc_handler_t vxt_handlers[256];

void vxtRpcInit(void)
{
    int i;
    for (i = 0; i < 256; i++) {
        vxt_handlers[i] = 0;
    }
}

int vxtRpcRegister(uint8_t id, vxt_rpc_handler_t fn)
{
    if (id <= VXT_RPC_UPSTREAM_MAX) {
        xprintf("vxt_rpc: refuse id %d (upstream range 0-%d)\n",
                (int)id, VXT_RPC_UPSTREAM_MAX);
        return -1;
    }
    if (vxt_handlers[id] != 0) {
        xprintf("vxt_rpc: refuse id %d (already registered)\n", (int)id);
        return -1;
    }
    if (fn == 0) {
        xprintf("vxt_rpc: refuse id %d (null handler)\n", (int)id);
        return -1;
    }
    vxt_handlers[id] = fn;
    xprintf("vxt_rpc: registered id %d\n", (int)id);
    return 0;
}

void vxtRpcDispatch(int data)
{
    uint8_t id = (uint8_t)data;

    if (id <= VXT_RPC_UPSTREAM_MAX) {
        /* Upstream IDs never reach here if the hook is placed in the
         * default case, but guard anyway. */
        return;
    }
    if (vxt_handlers[id]) {
        vxt_handlers[id](id, (volatile uint8_t *)parmRam);
    } else {
        /* Match upstream's silent-ignore semantics, but leave a trace. */
        xprintf("vxt_rpc: unhandled id %d\n", (int)id);
    }
}
