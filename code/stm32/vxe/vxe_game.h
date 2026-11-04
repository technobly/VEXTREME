/*
 * vxe_game.h - the one function every game provides to its payload
 * (vxe/vxe_payload.c calls it from vxe_payload_init()).
 * Register the game's RPCs here and nothing else that blocks
 * (SD I/O belongs in the game's own one-shot init RPC - convention 19).
 */
#ifndef VXE_GAME_H
#define VXE_GAME_H

void vxe_game_init(void);

#endif
