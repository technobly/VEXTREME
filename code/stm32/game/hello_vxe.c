/*
 * hello_vxe.c - HELLO's one entry point for the VXE loader: register the RPCs.
 * (vxe/vxe_payload.c calls vxe_game_init() after the toolkit is up.)
 * Built WITH vxe/vxe_redirect.h, like every payload file.
 */
#include "hello.h"
#include "../vxe/vxe_game.h"
#include "../vxt/vxt_rpc.h"

void vxe_game_init(void)
{
    vxtRpcRegister(HELLO_RPC_FRAME, hello_frame_handler);
    vxtRpcRegister(HELLO_RPC_INIT,  hello_init_handler);
}
