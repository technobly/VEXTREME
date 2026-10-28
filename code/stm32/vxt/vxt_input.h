/*
 * vxt_input.h - VXT toolkit: controller input block (STM32 side). GPLv3.
 *
 * Mirrors code/app6809/vxt/vxt_input.asm's parmRam block. Keep the two in step.
 *
 * The 6809 reports RAW controller state every frame; all game logic lives here
 * on the STM32 (the VOOM model). Joystick axes are reported as SIGNED bytes,
 * verbatim from the BIOS, not pre-digitized into direction flags - so the same
 * block serves both digital and analog games:
 *
 *   Digital game: the 6809 fills the axes via Joy_Digital (JOYBIT, $F1F8);
 *                 just test the sign here (vxtInJoyDir()).
 *   Analog  game: the 6809 fills them via Joy_Analog (JOYSTK, $F1F5) instead -
 *                 a one-token change on that side - and the magnitude is
 *                 meaningful here. Nothing in this header changes.
 *
 * Vol. 2 confirms the two BIOS routines are drop-in interchangeable: same entry
 * values, same result locations; JOYSTK even calls JOYBIT internally.
 *
 * parmRam is 6809-WRITE-only - the 6809 cannot read these back,
 * and the STM32 must never try to signal the 6809 through them. STM32->6809
 * data goes in the served image (see vxt_frame.h).
 */

#ifndef VXT_INPUT_H
#define VXT_INPUT_H

#include <stdint.h>

/* --- parmRam offsets (mirror of vxt_input.asm) --- */
#define VXT_IN_JOY1X    254   /* signed */
#define VXT_IN_JOY1Y    253   /* signed */
#define VXT_IN_JOY2X    252   /* signed; only meaningful if ctrl 2 enabled */
#define VXT_IN_JOY2Y    251   /* signed; ditto */
#define VXT_IN_BTN1_1   250
#define VXT_IN_BTN1_2   249
#define VXT_IN_BTN1_3   248
#define VXT_IN_BTN1_4   247
#define VXT_IN_BTNS     246   /* raw Vec_Btn_State bitmask, all buttons */
#define VXT_IN_EDGE     245   /* newly-pressed transitions (Read_Btns' EDGE) */

/* Signed axis accessors. parmRam is unsigned char; the axes are signed. */
#define vxtInJoyX(parm)   ((int8_t)(parm)[VXT_IN_JOY1X])
#define vxtInJoyY(parm)   ((int8_t)(parm)[VXT_IN_JOY1Y])
#define vxtInJoy2X(parm)  ((int8_t)(parm)[VXT_IN_JOY2X])
#define vxtInJoy2Y(parm)  ((int8_t)(parm)[VXT_IN_JOY2Y])

/* Digital reduction of an axis: -1 / 0 / +1. Works whether the 6809 filled the
 * axis via Joy_Digital or Joy_Analog - which is the point of reporting raw. */
#define vxtInDir(v)       ((v) > 0 ? 1 : ((v) < 0 ? -1 : 0))

/* Buttons. NOTE: vectrex.i calls the per-button bytes the "toggle state" of
 * each button; whether that means "held right now" or a latch that flips per
 * press is not established in the available documentation. Treat nonzero as
 * pressed, but verify the behavior on hardware - if a held button doesn't
 * behave as expected, the raw VXT_IN_BTNS bitmask and VXT_IN_EDGE are both
 * also reported for exactly this reason. (Bit positions within VXT_IN_BTNS are
 * likewise undocumented in the available material - no claim is made here
 * about which bit corresponds to which button.) */
#define vxtInBtn1(parm)   ((parm)[VXT_IN_BTN1_1] != 0)
#define vxtInBtn2(parm)   ((parm)[VXT_IN_BTN1_2] != 0)
#define vxtInBtn3(parm)   ((parm)[VXT_IN_BTN1_3] != 0)
#define vxtInBtn4(parm)   ((parm)[VXT_IN_BTN1_4] != 0)

#endif /* VXT_INPUT_H */
