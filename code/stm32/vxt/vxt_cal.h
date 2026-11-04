/*
 * vxt_cal.h - VXT toolkit: CALIBRATION / measurement rig (STM32 side).
 *
 * Toolkit-level, not game-level: this serves every game built on the VXT
 * toolkit, which is why it is a standalone 6809 cart
 * (code/app6809/test8_cal/) rather than a mode inside any one game.
 *
 * Measurement only. There are deliberately no adjustable calibration
 * values, no persistence, and no settings UI in this file. A slider added
 * before the measurement exists would happily null out a defect that is
 * in a firmware constant, on one specific machine, permanently -
 * and the screen could not tell you which you had. Measure first, adjust
 * only from a written-down number.
 *
 * HOW A measurement is taken. The STM32 cannot see the screen; only the user
 * can. So every screen draws (a) a test figure via some draw path and (b) an
 * independently repositioned reference mark at the figure's ideal endpoint.
 * The gap between them is the error. The joystick drives a measuring caret
 * which the user parks on where the beam visibly landed; the readout then
 * prints the caret's offset from that screen's nominal target in physical
 * units. Subjective "that looks off" becomes a number you can write down,
 * photograph, and argue about later.
 */
#ifndef VXT_CAL_H
#define VXT_CAL_H

#include <stdint.h>

/* Per-frame handler. Registered on RPC 75 in main.c; driven by
 * code/app6809/test8_cal/test8_cal.asm's frame loop. */
void vxt_cal_handler(uint8_t id, volatile uint8_t *parm);

/* One-shot, fired once per 6809 boot (RPC 76). Resets screen selection,
 * caret and variant state. A 6809 warm reset does not reset the STM32, so
 * without this the rig would resume mid-state after a reset and quietly
 * misreport which screen you are on. */
void vxt_cal_init_handler(uint8_t id, volatile uint8_t *parm);

#endif /* VXT_CAL_H */
