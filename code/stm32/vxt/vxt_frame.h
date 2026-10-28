/*
 * vxt_frame.h - VXT toolkit: vector-list writer (STM32 side). GPLv3.
 *
 * Writes vector records into the served image at a fixed offset, using the
 * byte format confirmed directly from Sprite_tm's original VOOM source
 * (voom-voom_asm.txt) - not from its header comment,
 * which describes a run-length format the assembled code does not * implement.
 *
 * Format, 4 bytes per vector record:
 *   byte0 = intensity ($00-$7F). Bit 6 ($40) set = "recenter to origin
 *           first" (VOOM's code masks this byte with $3F, not $7F, before
 *           calling Intensity_a in that case - so a recenter record's
 *           usable intensity is 0-63, not the full 0-127 the BIOS INTENS
 *           routine (Vol.2, $F2AB) supports. A quirk of the
 *           reference implementation; not required to preserve, noted for
 *           accuracy).
 *   byte1 = scale, loaded directly into VIA_t1_cnt_lo (line length/duration).
 *   byte2 = coord1 (first axis, written to VIA_port_a).
 *   byte3 = coord2 (second axis, written to VIA_port_a after an S/H strobe).
 * Sentinel: intensity byte == $FF ends the list.
 *
 * Offset $0800 is this project's adopted convention, matching VOOM's own
 * choice - not a universal requirement, just the
 * address both the STM32 writer and the 6809 draw loop must agree on.
 *
 * PATCHED (Test 5 prep): vf_put() renamed to vxtImgPut() and
 * exported. It implements the bank-mirroring rule (romemu.S
 * inverts PB6/A15; runtime writes into the 64K cartData buffer must be
 * mirrored into both 32K banks, but not into the 20K menuData buffer, which
 * would overflow). vxt_sound.c reuses this single copy of the rule rather
 * than duplicating it.
 */

#ifndef VXT_FRAME_H
#define VXT_FRAME_H

#include <stdint.h>

#define VXT_FRAME_OFFSET   0x0800

/* Hard ceiling on records per frame. The list starts at $0800 and the sound
 * command block sits at VXT_SND_OFFSET. If the list grows into it, the 6809
 * feeds garbage register/value pairs to Sound_Byte and the PSG goes silent
 * PERMANENTLY. That is not hypothetical - TEST 6a at ~85+ boxes did exactly
 * this. Sound has since moved to $2000, giving ($2000-$0800)/4 = 1536
 * records; this bound keeps the list inside that no matter how big a scene
 * gets. Overflow truncates the picture (visible, debuggable) instead of
 * corrupting another subsystem (invisible, baffling). */
#define VXT_FRAME_MAX_RECORDS  1530

/* Begin writing at a given offset into the currently-served image
 * (romData - see rom.h). Call once before any vxtFrameVector() calls. */
void vxtFrameBegin(uint16_t offset);

/* Emit one 4-byte vector record. */
void vxtFrameVector(uint8_t intensity, uint8_t scale, uint8_t coord1, uint8_t coord2);

/* Write the $FF sentinel, ending the list. */
void vxtFrameEnd(void);

/* --- Intensity API -------------------------------------------------------
 * FULL 0-127 RANGE. Previously capped at 0-63: VOOM's wire
 * format packed intensity and the recenter flag into one byte with BIT 6
 * ($40) as the flag, so any intensity >=64 set bit 6 and was misread by
 * vxt_draw as a beam reset (a 4-level gradient using 80/127 made most boxes
 * vanish - that is where the 63 ceiling came from; it was never a hardware
 * or BIOS limit, the BIOS Intensity_a takes 0-127). The flag now lives on
 * BIT 7, which the 0-127 range does not use, so the full range is free.
 * A recenter record must keep intensity <=126, since $FF ends the list. */
#define VXT_RECENTER_FLAG     0x80

#define VXT_INTENSITY_OFF     0
#define VXT_INTENSITY_DIM     40    /* bumped from 16 - was too faint to read */
#define VXT_INTENSITY_MEDIUM  70    /* bumped from 32 */
#define VXT_INTENSITY_BRIGHT  100
#define VXT_INTENSITY_MAX     127   /* the real BIOS ceiling, now reachable */

void vxtFrameVectorAt(uint8_t intensity, int recenter,
                      uint8_t scale, int8_t coord1, int8_t coord2);

/* Move a LARGE distance (more than one signed byte holds) by chaining small
 * safe relative moves. Computing a position as `start + index*pitch` in one
 * int8_t wraps once the pitch grows; this never does. */
void vxtFrameMoveBig(int16_t totalY, int16_t totalX, uint8_t scale);

/* Nonzero if the last frame hit VXT_FRAME_MAX_RECORDS and was truncated.
 * Cleared by vxtFrameBegin(). */
int vxtFrameOverflowed(void);

/* Write one byte into the currently-served image at `pos`, mirroring it into
 * the other 32K bank when (and only when) the served image is the 64K
 * cartData buffer (Appendix I.7). Exposed so other toolkit writers (vxt_sound)
 * share this ONE copy of the mirroring rule instead of duplicating it. */
void vxtImgPut(uint16_t pos, uint8_t b);

#endif /* VXT_FRAME_H */
