/*
 * cal_vxe.c - the calibration rig's entry point for the VXE loader. The rig itself lives in
 * vxt/vxt_cal.c; vxe/vxe_payload.c registers its two RPCs (75 per frame, 76 one-shot init)
 * when the rig is linked, so there is nothing left for the game to register.
 * Built WITH vxe/vxe_redirect.h, like every payload file.
 */
#include "../vxe/vxe_game.h"

void vxe_game_init(void)
{
}
