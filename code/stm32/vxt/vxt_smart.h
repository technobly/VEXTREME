/*
 * vxt_smart.h - VXT toolkit: SmartList emitter (STM32 side). GPLv3.
 *
 * Emits the (A, B, subroutine-address) triples consumed by vxt_smart.asm's
 * pulu-dispatch draw engine. See that file for the technique, its source
 * (Malban / Kristof), and the measured ~2.2x speedup over vxt_draw.
 *
 * The 6809-side routine addresses are not known to the STM32 at compile time -
 * they land wherever the 6809 app links them. The app therefore publishes them
 * once at startup via an RPC (see VXT_RPC_ID_SMART_ADDRS), and the emitter
 * fills in the table below. Emitting before the table is populated is a no-op,
 * which fails visibly (blank screen) rather than by jumping the 6809 into
 * garbage.
 */
#ifndef VXT_SMART_H
#define VXT_SMART_H

#include <stdint.h>

#define VXT_SMART_OFFSET   0x0800    /* same window as the vxt_draw list -
                                      * the two are alternatives, never both */

/* Routine address table, published by the 6809 app at startup. */
typedef struct {
    uint16_t setScale;
    uint16_t setIntensity;
    uint16_t recenter;
    uint16_t startMove;
    uint16_t startDraw;
    uint16_t cont;       /* SM_continue_d - draw OR move, whichever is active */
    uint16_t end;
    uint16_t startMoveBig; /* Added: SM_startMoveBig_d, padded for
                            * scale 32, not the project default (12). Used
                            * only by vxtSmartMoveBig() for repositioning -
                            * see vxt_smart.asm for why one nop count can't
                            * correctly serve two different scales. */
    uint16_t startDrawBig; /* Added, REVISED same day: SM_startDrawBig_d,
                            * now padded for scale 64 (27 nops - UNVERIFIED
                            * on hardware, see vxt_smart.asm), not the
                            * scale-32/11-nop pairing this started as. Beam
                            * lit instead of blanked. Used only by
                            * vxtSmartDrawBig() for long single-record
                            * draws (e.g. grid lines) - see
                            * vxt_smart.asm. Apps that don't publish this
                            * (Test 6b/6c/6d, unchanged) leave it 0 and never
                            * call vxtSmartDrawBig(), so they're unaffected. */
    uint16_t startDrawHuge; /* Added:
                            * SM_startDrawHuge_d - same record shape as
                            * startDrawBig, but waits on a GENUINE Timer 1
                            * poll instead of a fixed, scale-specific nop
                            * count, so it is timing-correct at any scale
                            * without needing its own hardware-verified nop
                            * pairing. Used only by vxtSmartDrawHuge(), for
                            * the rare edges long enough that even scale 64
                            * needs many chained BigScale steps (the
                            * Destroyer's longest edges, worst when nearby -
                            * see gamelib_beam.c's gamelibDrawHugeLine()).
                            * Costs ~2.2x a normal SmartList dispatch (same
                            * premium vxt_draw.asm's own header measures for
                            * this technique) - deliberately not the default
                            * BigScale path. Apps that don't publish this
                            * leave it 0 and never call vxtSmartDrawHuge(),
                            * so they're unaffected, same convention as
                            * startMoveBig/startDrawBig above. */
    uint16_t setScaleHi;    /* Added
                            * follow-up): SM_setScaleHi - stages the HIGH
                            * byte of Timer 1's real 16-bit count (every
                            * routine above always hardcodes this to 0,
                            * capping single-record reach at ~25,500 phys
                            * units - a self-imposed record-format limit,
                            * not a Vectrex hardware one; VIA Timer 1 is a
                            * genuine 16-bit down-counter). Used only by
                            * vxtSmartDrawHuge16(). */
    uint16_t startDrawHuge16; /* Added
                            * follow-up): SM_startDrawHuge16_d - same
                            * genuine-Timer-1-poll technique as
                            * startDrawHuge, but loads the real staged high
                            * byte instead of hardcoding zero, so a single
                            * record's max reach becomes ~6,553,500 phys
                            * units (100*65535) instead of ~25,500 -
                            * comfortably past this game's largest edge
                            * (~303,000 at the Destroyer's near-plane
                            * worst case). Used only by
                            * vxtSmartDrawHuge16(), which gamelibDrawHugeLine()
                            * escalates to only when the needed scale
                            * exceeds 255 - the common case still
                            * uses the already-hardware-confirmed 8-bit
                            * startDrawHuge path unchanged. */
    uint16_t startDraw32;   /* Added: SM_startDraw32_d - the
                            * missing scale-32 DRAW. Draws existed only at
                            * 12 and 64; scale 32 had a MOVE routine but no
                            * draw counterpart. Lets a short edge skip
                            * startDrawBig's 27-nop scale-64 ramp wait (52
                            * of its 97 cycles) while landing on the same
                            * grid repositions use (the reposition grid scale, 32), so
                            * a drawn vertex and a repositioned one still
                            * coincide - which a scale-12 draw does not.
                            * Used by vxtSmartDraw32(); apps that do not
                            * publish it never take that path. */
    uint16_t startMoveHuge; /*
                            * wait is a genuine Timer 1 poll, correct at any scale (the dark twin of
                            * startDrawHuge). Used by vxtSmartMoveHuge(). Apps that do not publish it
                            * never call it (guard: vxtSmartHasMoveHuge()). */
} vxtSmartAddrs;

/* Called from the address-publishing RPC handler. */
void vxtSmartSetAddrs(const vxtSmartAddrs *a);
int  vxtSmartReady(void);

/* Emitter. Mirrors vxt_frame's shape so callers port easily. */
/* Fixed: `maxRecords` is now REQUIRED, not implied. Previously
 * vxtSmartBegin() only bounded against VXT_FRAME_MAX_RECORDS (the OUTER
 * $0800-$2000 span) - correct for a single-region frame, but Test 6b/6c both
 * split that span into a box sub-region ($0800-$1BFF) and a text sub-region
 * ($1C00-$1FFF, drawn via vxt_draw). The outer bound had NO knowledge of
 * that inner boundary, so a box scene could grow past $1C00 and * overwrite the text region - masked most frames (text draws AFTER boxes
 * and overwrites the tail again) but a genuine risk if the list's own
 * SM_end terminator address lands in the overlap and gets stomped: the
 * 6809 would then read vxt_draw-format bytes as SmartList (A,B,address)
 * triples and jump to a garbage address. Callers must now pass the record
 * count their sub-region has room for. */
void vxtSmartBegin(uint16_t offset, int maxRecords);
void vxtSmartScale(uint8_t scale);
void vxtSmartIntensity(uint8_t intensity);   /* FULL 0-127 - no stolen bits */
void vxtSmartRecenter(void);
void vxtSmartMove(int8_t y, int8_t x);       /* starts a blanked run */
void vxtSmartDraw(int8_t y, int8_t x);
/* Added - same one-record draw, but through SM_startDraw32_d
 * (scale 32, 12 nops) instead of SM_startDraw_d (scale 12, 1 nop). Caller
 * sets scale 32 first; y/x are rate units (phys/32), +-100 max. See the
 * startDraw32 field above. */
void vxtSmartDraw32(int8_t y, int8_t x);

/* 1 only if the running app published startDraw32. must be checked
 * before emitting a scale-32 draw - see the definition for the crash this
 * prevents when new firmware meets an old application binary. */
int vxtSmartHasDraw32(void);       /* starts a lit run     */
void vxtSmartCont(int8_t y, int8_t x);       /* continues the current run */

/* Move a LARGE distance, chaining int8_t-safe steps - the SmartList analogue
 * of vxtFrameMoveBig. Added after a real bug: row positioning
 * (e.g. y=+16800, x=-13056 phys, /32 = 525 and -408) WRAPPED through
 * int8_t, so every row landed in the same wrong place and the grid walked off
 * screen. `scale` must be the scale currently loaded on the 6809 side - emit
 * vxtSmartScale(scale) first, or the displacement is wrong by the ratio of
 * the two scales (that was a second, separate bug). */
void vxtSmartMoveBig(int32_t physY, int32_t physX, uint8_t scale);

/* Draw (not move) a LARGE distance in as few records as possible, chaining
 * +-100-rate-unit steps at the given scale via SM_startDrawBig_d - the
 * draw-mode analogue of vxtSmartMoveBig(). Added for a grid
 * lines (up to 6400 phys units): at scale 32, TWO chained records (3200
 * each) cover the full length, versus 5-6 scale-12 SM_continue_d records
 * through chainDelta-style chaining - a real, confirmed cycle saving (see
 * Same "scale must already be loaded on the
 * 6809 side" requirement as vxtSmartMoveBig - emit vxtSmartScale(scale)
 * first. Requires the app to have published startDrawBig in its address
 * handshake (vxtSmartAddrs); a no-op (fails blank, not wild) otherwise -
 * see sm_rec()'s existing !sm_have_addrs guard. */
void vxtSmartDrawBig(int32_t physY, int32_t physX, uint8_t scale);

/* Added - identical calling convention and
 * chaining behavior to vxtSmartDrawBig() (same +-100-rate-unit chaining
 * loop, same "emit vxtSmartScale(scale) first" requirement), but dispatches
 * through SM_startDrawHuge_d, which waits on a genuine Timer 1 poll instead
 * of a fixed nop count - correct at any scale, including one chosen at
 * runtime specifically to minimize chained steps for a given distance (see
 * gamelibDrawHugeLine(), which picks that scale). Use this only for edges
 * long/exposed enough to justify the ~2.2x per-record cost over
 * vxtSmartDrawBig() - not a drop-in replacement for the whole BigScale path.
 * Requires the app to have published startDrawHuge in its address handshake;
 * a no-op (fails blank, not wild) otherwise, same guard as vxtSmartDrawBig(). */
void vxtSmartDrawHuge(int32_t physY, int32_t physX, uint8_t scale);

/* Added - the actual fix for the
 * "why is there a cap at all" question: uses Timer 1's real 16-bit range
 * (via SM_setScaleHi + SM_startDrawHuge16_d) instead of the 8-bit-only
 * range every other routine in this file is limited to by always clearing
 * VIA_t1_cnt_hi. `scale16` may be up to 65535, raising one record's max
 * reach from ~25,500 to ~6,553,500 phys units (at this project's +-100
 * chaining margin) - past any realistic single edge in this game. Chains
 * only if `physY`/`physX` somehow still exceed that (shouldn't happen in
 * practice - defensive, not a real code path). Emits vxtSmartScale() for
 * the low byte and vxtSmartScaleHi() for the high byte itself - caller
 * does not need to call either separately. Requires the app to have
 * published setScaleHi/startDrawHuge16; a no-op otherwise, same guard as
 * every other vxtSmart* emitter. */
/*
 * SM_startMoveHuge_d - identical calling convention to vxtSmartDrawHuge() but dark. Emit vxtSmartScale(scale)
 * first. No-op if the app did not publish startMoveHuge. */
void vxtSmartMoveHuge(int32_t physY, int32_t physX, uint8_t scale);
int  vxtSmartHasMoveHuge(void);

void vxtSmartScaleHi(uint8_t hi);
void vxtSmartDrawHuge16(int32_t physY, int32_t physX, uint16_t scale16);

void vxtSmartEnd(void);

/* 1 if any record was dropped this frame for want of budget. CHANGED
 * Overflow is now SURVIVABLE - vxtSmartBegin() reserves the
 * terminator's slot so vxtSmartEnd() can never be the dropped record, and
 * the 6809 always gets a properly terminated (if incomplete) list. See
 * vxtSmartBegin()'s own comment for the hardware bug this fixes. */
int  vxtSmartOverflowed(void);

/* Records emitted so far this frame (terminator excluded) - for measuring
 * real frame cost on hardware, since no simulator exists for it. */
int  vxtSmartRecordCount(void);

#endif /* VXT_SMART_H */
