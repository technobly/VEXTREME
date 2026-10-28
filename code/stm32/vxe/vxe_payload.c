/*
 * vxe_payload.c - PAYLOAD side entry point: vxe_payload_init(), the one call
 * the kernel makes into a payload (vxe_api.h). Starts the VXT toolkit,
 * registers the toolkit-level RPCs, then the game's own (vxe_game_init(),
 * one per game: game/hello_vxe.c is the template).

 * This is what main() would otherwise do inline before `romemu` starts (font tables, RPC 69, the game's touchpoints).
 * Built WITH vxe_redirect.h, like every payload object.
 */
#include "vxe_api.h"
#include "vxe_game.h"
#include "../vxt/vxt_rpc.h"
#include "../vxt/vxt_smart.h"
#include "../vxt/vxt_smart_text.h"
#include "../vxt/vxt_font_western.h"
#if VXT_ENABLE_CAL
#include "../vxt/vxt_cal.h"
#endif

/* RPC 69 - the SmartList address handshake every VXT cart makes at boot: the
 * 6809 publishes the addresses of its SM_* routines in the parameter block.
 * Moved here from main.c (vxt_smart_addrs_handler); see the
 * history of each field there and in vxt_smart.h. An app that publishes fewer
 * addresses leaves the later fields as leftover parmRam - harmless, because
 * such an app never dispatches to them (guards: vxtSmartHasDraw32(),
 * vxtSmartHasMoveHuge()). */
static void vxe_smart_addrs_handler(uint8_t id, volatile uint8_t *parm)
{
    vxtSmartAddrs a;
    (void)id;
    a.setScale        = (uint16_t)((parm[0]  << 8) | parm[1]);
    a.setIntensity    = (uint16_t)((parm[2]  << 8) | parm[3]);
    a.recenter        = (uint16_t)((parm[4]  << 8) | parm[5]);
    a.startMove       = (uint16_t)((parm[6]  << 8) | parm[7]);
    a.startDraw       = (uint16_t)((parm[8]  << 8) | parm[9]);
    a.cont            = (uint16_t)((parm[10] << 8) | parm[11]);
    a.end             = (uint16_t)((parm[12] << 8) | parm[13]);
    a.startMoveBig    = (uint16_t)((parm[14] << 8) | parm[15]);
    a.startDrawBig    = (uint16_t)((parm[16] << 8) | parm[17]);
    a.startDrawHuge   = (uint16_t)((parm[18] << 8) | parm[19]);
    a.setScaleHi      = (uint16_t)((parm[20] << 8) | parm[21]);
    a.startDrawHuge16 = (uint16_t)((parm[22] << 8) | parm[23]);
    a.startDraw32     = (uint16_t)((parm[24] << 8) | parm[25]);
    a.startMoveHuge   = (uint16_t)((parm[26] << 8) | parm[27]);
    vxtSmartSetAddrs(&a);
}

void vxe_payload_init(const VxeKernelApi *api, VxePayloadHooks *hooks)
{
    vxe_shim_bind(api);                 /* first: everything below may use the kernel */

    vxtRpcInit();
    vxtSmartTextSetFontTable(vxtFontWestern, vxtFontWesternCount);
    vxtRpcRegister(69, vxe_smart_addrs_handler);           /* 69: SmartList addr publish */
#if VXT_ENABLE_CAL
    vxtRpcRegister(75, vxt_cal_handler);                   /* 75: calibration/measurement rig (code/app6809/Cal) */
    vxtRpcRegister(76, vxt_cal_init_handler);              /* 76: cal rig one-shot init */
#endif
#if VXT_ENABLE_GAME
    vxe_game_init();                                       /* the game's own RPCs */
#endif
    hooks->rpc_dispatch = vxtRpcDispatch;
}

#ifndef VXE_SPLIT
#define VXE_SPLIT 0
#endif
#if VXE_SPLIT
/* ---- the payload as its own image (vxe/vxe_payload_<layout>.ld) ----------
 * The kernel's reset handler only initialises the KERNEL's .data/.bss, so a
 * separately linked payload does its own: vxe_payload_start() is the C
 * runtime start for the payload, and the header below is how the kernel finds
 * it (vxeKernelStart(), vxe_kernel.c). */
extern unsigned int _vxe_data_load, _vxe_data, _vxe_edata, _vxe_bss, _vxe_ebss;
extern void (*_vxe_init_array_start[])(void);
extern void (*_vxe_init_array_end[])(void);
extern const char _vxe_image_end[];

void vxe_payload_start(const VxeKernelApi *api, VxePayloadHooks *hooks);

void vxe_payload_start(const VxeKernelApi *api, VxePayloadHooks *hooks)
{
    unsigned int *src = &_vxe_data_load, *dst = &_vxe_data;
    void (**init)(void);

    /* No .data or .bss variable may be touched before these two loops. */
    while (dst < &_vxe_edata) *dst++ = *src++;
    for (dst = &_vxe_bss; dst < &_vxe_ebss; ) *dst++ = 0;
    for (init = _vxe_init_array_start; init < _vxe_init_array_end; init++) (*init)();

    vxe_payload_init(api, hooks);
}

__attribute__((section(".vxe_header"), used))
const VxePayloadHeader vxe_payload_header = {
    .magic       = VXE_PAYLOAD_MAGIC,
    .abi_version = VXE_API_VERSION,
    .header_size = sizeof(VxePayloadHeader),
    .image_end   = (uint32_t)_vxe_image_end,
    .crc32       = 0,                   /* tools/make_vxe.py stamps the identity here */
    .ram_start   = (uint32_t)&_vxe_data,
    .ram_end     = (uint32_t)&_vxe_ebss,
    .start       = (uint32_t)vxe_payload_start,
};
#endif /* VXE_SPLIT */
