/*
 * vxcoop_vxe.c - VX-COOP's entry point for the VXE loader: register its two RPCs.
 * (vxe/vxe_payload.c calls vxe_game_init() after the toolkit is up.)
 * Built WITH vxe/vxe_redirect.h, like every payload file.
 */
#include "vxcoop_handler.h"
#include "../vxe/vxe_game.h"
#include "../vxt/vxt_rpc.h"

void vxe_game_init(void)
{
    vxtRpcRegister(78, vxcoop_handler);        /* 78: per frame: compute and compose a frame */
    vxtRpcRegister(79, vxcoop_init_handler);   /* 79: one-shot, once per 6809 boot */
}
