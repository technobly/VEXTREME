/*
 * hello.h - HELLO, the SDK template game for the VXE loader (docs/vxe/SDK_GUIDE.md).
 *
 * A square you move with the joystick. Button 1 SAVES its position to a file in
 * the game's own folder (and beeps, and shows a small inner square for a moment);
 * the saved position is loaded again the next time the game starts. That is
 * everything a real game does - read input, advance state, compose a frame, make a
 * sound, persist a file - in about 250 lines you can replace piece by piece.
 *
 * Copy this game, rename it, and register it (games/<name>.mk): see §7 of the guide.
 */
#ifndef HELLO_H
#define HELLO_H

#include <stdint.h>

/* The two RPC IDs this game uses. They must be inside 64-79 (the only block the
 * kernel forwards to a payload) and must match code/app6809/Hello/Hello.asm. */
#define HELLO_RPC_FRAME  64   /* per-frame: advance one tick, compose the list */
#define HELLO_RPC_INIT   65   /* once per 6809 boot: reset state, load the file */

void hello_init_handler(uint8_t id, volatile uint8_t *parm);
void hello_frame_handler(uint8_t id, volatile uint8_t *parm);

#endif /* HELLO_H */
