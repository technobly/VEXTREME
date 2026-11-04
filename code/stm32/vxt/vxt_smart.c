/*
 * vxt_smart.c - VXT toolkit: SmartList emitter (STM32 side). GPLv3.
 * See vxt_smart.h and app6809/vxt/vxt_smart.asm.
 */
#include "vxt_smart.h"
#include "vxt_frame.h"    /* for vxtImgPut() + VXT_FRAME_MAX_RECORDS */

static vxtSmartAddrs sm_addrs;
static int      sm_have_addrs;
static uint16_t sm_pos;
static int      sm_count;
static int      sm_overflow;
static int      sm_max_records;
static int      sm_last_intensity;   /* -1 = unknown; see vxtSmartIntensity */

/* Lazy scale (added 2026-10-09; set VXT_LAZY_SCALE to 0 to restore the old
 * emit-every-call behaviour). Only SM_setScale writes the 6809's Timer 1 low
 * latch (vxt_smart.asm) - every ramp-starting routine merely USES it - so a
 * scale that is replaced before any ramp runs at it is a dead record.
 * vxtSmartScale() therefore only records the request, and sm_need_scale()
 * emits the record just before a ramp whose latch would differ.
 *
 * MEASURED on the host (a real game + this emitter, 3000 frames):
 * mean 1163 -> 944 records/frame, p99 1511 -> 1257, 47% of setScale records
 * dead. Effective streams compared over 1.4M records: the latch in effect at
 * every ramp is identical except 221 first-of-frame zero-rate pen-lifts that
 * ran on a stale cross-frame latch in the old code too. not yet confirmed on
 * hardware - there is no simulator for the 6809 side (see docs/vxe/CONVENTIONS.md). */
#ifndef VXT_LAZY_SCALE
#define VXT_LAZY_SCALE 1
#endif
static int      sm_want_scale;       /* last scale the game asked for; -1 = none this frame */
static int      sm_hw_scale;         /* what the T1 latch holds as far as this frame knows */

void vxtSmartSetAddrs(const vxtSmartAddrs *a)
{
    sm_addrs = *a;
    sm_have_addrs = 1;
}

int vxtSmartReady(void)     { return sm_have_addrs; }
int vxtSmartOverflowed(void) { return sm_overflow; }

/* One record: A, B, then the routine address BIG-ENDIAN (the 6809's PULU
 * pulls PC high byte first). */
static void sm_rec(uint8_t a, uint8_t b, uint16_t addr)
{
    if (!sm_have_addrs) return;                   /* fails blank, not wild */
    if (sm_count >= sm_max_records) { sm_overflow = 1; return; }
    sm_count++;
    vxtImgPut(sm_pos++, a);
    vxtImgPut(sm_pos++, b);
    vxtImgPut(sm_pos++, (uint8_t)(addr >> 8));    /* hi first - 6809 order */
    vxtImgPut(sm_pos++, (uint8_t)(addr & 0xFF));
}

/* Changed - hardware bug fix, not a hardening tweak.
 * `sm_rec()` above drops every record once the budget is hit,
 * and that INCLUDED the closing terminator `vxtSmartEnd()` writes. A
 * dropped terminator means the 6809's SmartList walker never stops - it
 * runs off the end of the valid list into whatever stale bytes sit there
 * and executes them as bogus routine addresses. Observed on real
 * hardware as the whole app apparently rebooting mid-frame (jumping back
 * to its own boot animation), and as stray lines, whenever a frame's
 * content got heavy enough to overrun - exactly the failure mode a busy
 * frame (cockpit overlay ON + two full-detail enemies straddling the
 * window edge) now reaches routinely.
 *
 * FIX: reserve one record slot for the terminator up front, so overflow
 * degrades to dropped geometry (a visibly incomplete but structurally
 * VALID list the 6809 walks and terminates normally) instead of an
 * unterminated list that crashes it. The budget the rest of the frame
 * sees is one smaller; `vxtSmartEnd()` then always has somewhere to go.
 * Overflow is still reported via vxtSmartOverflowed() - this makes the
 * failure survivable and diagnosable, it does not make it acceptable;
 * callers still need to fit the budget. */
void vxtSmartBegin(uint16_t offset, int maxRecords)
{
    sm_pos = offset;
    sm_count = 0;
    sm_overflow = 0;
    /* Reserve the terminator's slot - see this function's own comment. */
    sm_max_records = (maxRecords > 0) ? (maxRecords - 1) : 0;
    sm_last_intensity = -1;   /* force the first intensity of the frame */
    sm_want_scale = -1;       /* BIOS calls between frames rewrite the T1 latch, */
    sm_hw_scale = -1;         /*   so nothing is known about it at frame start   */
}

/* Peak record usage of the frame so far - for on-hardware budget audits
 * (Added; no simulator exists for this, so the only honest way
 * to size a frame is to measure it on the device). Does not count the
 * reserved terminator slot. */
int vxtSmartRecordCount(void) { return sm_count; }

#if VXT_LAZY_SCALE
void vxtSmartScale(uint8_t scale)      { sm_want_scale = scale; }
static void sm_need_scale(void)
{
    if (sm_want_scale >= 0 && sm_want_scale != sm_hw_scale) {
        sm_hw_scale = sm_want_scale;
        sm_rec(0, (uint8_t)sm_want_scale, sm_addrs.setScale);
    }
}
#else
void vxtSmartScale(uint8_t scale)      { sm_rec(0, scale, sm_addrs.setScale); }
#define sm_need_scale() ((void)0)
#endif
void vxtSmartIntensity(uint8_t i)
{
    if (i > 127) i = 127;   /* BIOS Intensity_a range. No bit is stolen for a
                             * recenter flag here, unlike vxt_draw's byte0. */

    /* CHANGE-GATED. Previously this emitted on every call,
     * so a caller looping over boxes paid a full BIOS Intensity_a call
     * (~60 cycles with the pshs/tfr/jsr/puls/pulu wrapper) per box - ~15% of
     * the whole 30k frame budget at 73 boxes, thrown away. vxt_draw has had
     * an intensity cache since v2; the SmartList emitter had none. Callers
     * can now emit intensity unconditionally and this does the right thing. */
    if ((int)i == sm_last_intensity) return;
    sm_last_intensity = (int)i;
    sm_rec(0, i, sm_addrs.setIntensity);
}

/* Fixed (hardware bug): plain C division truncates toward
 * zero, so physY/physX values that aren't exact multiples of `scale` lose
 * up to (scale-1) units of real distance - always shortening, never
 * lengthening, so every reposition landed slightly short of its target.
 * A real game's own screen-offset constant (-13500) isn't evenly divisible
 * by scale 32 or 64, so this bit on essentially every repositionAbs() call, matching
 * the user's reported "origin point offset from the corner" symptom.
 * Round-to-nearest instead. */
static int32_t sm_round_div(int32_t num, int32_t den)
{
    if (num >= 0) {
        return (num + den / 2) / den;
    } else {
        return (num - den / 2) / den;
    }
}

/* Distributes (ry, rx) proportionally across `steps` chained records.
 * Step k lands on round(r*k/steps), so every intermediate point sits
 * within one scale unit of the true straight chord, and the steps still
 * sum exactly to (ry, rx). Each step's magnitude is <= ceil(max/steps) <=
 * 100 by construction, so the int8_t record field never overflows.
 *
 * Do not clamp each axis independently to +-100 instead
 * (sy = clamp(ry, +-100); sx = clamp(rx, +-100); emit(sy, sx); ry -= sy;
 * rx -= sx). The total displacement comes out correct, but the path is
 * not a straight line whenever the two axes need different step counts:
 * ry=200, rx=100 would emit (100,100) then (100,0) - a 45deg segment
 * followed by a vertical one, a visible dogleg instead of the intended
 * 2:1 chord. This only shows up once a caller chains more than one
 * record for a single line (a caller that always fits the whole line in
 * one record never exercises it), so verify against a multi-record chain
 * before trusting any change here. */
static void sm_chain_steps(int32_t ry, int32_t rx, uint16_t addr)
{
    int32_t maxAbs = (ry < 0) ? -ry : ry;
    int32_t absRx  = (rx < 0) ? -rx : rx;
    int32_t steps, k, prevY = 0, prevX = 0;

    if (absRx > maxAbs) maxAbs = absRx;
    if (maxAbs == 0) return;
    sm_need_scale();
    steps = (maxAbs + 99) / 100;

    for (k = 1; k <= steps; k++) {
        int32_t curY = sm_round_div(ry * k, steps);
        int32_t curX = sm_round_div(rx * k, steps);
        sm_rec((uint8_t)(int8_t)(curY - prevY), (uint8_t)(int8_t)(curX - prevX), addr);
        prevY = curY;
        prevX = curX;
    }
}

void vxtSmartMoveBig(int32_t physY, int32_t physX, uint8_t scale)
{
    /* Fixed (hardware bug, not a style choice): this
     * previously called vxtSmartMove() -> SM_startMove_d, which is padded
     * with only 1 nop (correct for the project's default scale, 12). Row
     * repositioning runs at scale 32 (POS_SCALE), needing 11 nops (Malban's
     * formula, (32-9)/2) - Timer 1 hadn't expired before the next VIA write,
     * causing the row-to-row drift/misalignment seen on hardware. Now routes
     * through SM_startMoveBig_d specifically, which carries the correct
     * padding for scale 32. Box-to-box moves (vxtSmartMove, scale 12) are
     * unaffected and still use the cheaper, correctly-padded SM_startMove_d. */
    /* Changed (Sec.72): proportional chaining - see
     * sm_chain_steps() for the dogleg bug the old independent-axis clamp
     * produced. Endpoint unchanged; the path between steps is now straight. */
    sm_chain_steps(sm_round_div(physY, (int32_t)scale),
                   sm_round_div(physX, (int32_t)scale),
                   sm_addrs.startMoveBig);
}
/* Added: draw-mode analogue of vxtSmartMoveBig() - see
 * vxt_smart.h's doc comment. Mirrors vxtSmartMoveBig's exact chaining
 * pattern (repeats the same "start" routine for every step, never a
 * separate "continue" - re-asserting VIA_shift_reg=$FF when it's already
 * $FF is a harmless no-op, per vxt_smart.asm's SM_startDrawBig_d comment). */
void vxtSmartDrawBig(int32_t physY, int32_t physX, uint8_t scale)
{
    /* Fixed: round-to-nearest, not truncate - see sm_round_div()
     * comment above vxtSmartMoveBig().
     * Changed (Sec.72): proportional chaining, see
     * sm_chain_steps() - matters far more for a DRAW than a move, since a
     * dogleg here is a visibly bent line, not just a bent blank travel. */
    sm_chain_steps(sm_round_div(physY, (int32_t)scale),
                   sm_round_div(physX, (int32_t)scale),
                   sm_addrs.startDrawBig);
}
/* Added - identical chaining loop to
 * vxtSmartDrawBig() above, just dispatched through startDrawHuge (genuine
 * Timer 1 poll, correct at any scale) instead of startDrawBig (fixed,
 * scale-64-only nop pairing, unverified). See gamelibDrawHugeLine() for the
 * caller that picks `scale` to minimize chained steps for a given distance. */
void vxtSmartDrawHuge(int32_t physY, int32_t physX, uint8_t scale)
{
    /* Changed (Sec.72): proportional chaining - see
     * sm_chain_steps(). This is THE path the capped-scale dispatch now
     * chains on, so the dogleg fix is a hard prerequisite here. */
    sm_chain_steps(sm_round_div(physY, (int32_t)scale),
                   sm_round_div(physX, (int32_t)scale),
                   sm_addrs.startDrawHuge);
}
/*
 * through the poll-exact blank-move routine, so it is correct at any scale (MoveBig is padded for 32 only). */
void vxtSmartMoveHuge(int32_t physY, int32_t physX, uint8_t scale)
{
    sm_chain_steps(sm_round_div(physY, (int32_t)scale),
                   sm_round_div(physX, (int32_t)scale),
                   sm_addrs.startMoveHuge);
}

/* Same safety idea as vxtSmartHasDraw32(): an app assembled before this routine existed leaves stale parmRam here,
 * and a garbage `pulu a,b,pc` target is a hard crash. A genuine startMoveHuge is assembled from the same file as
 * startDrawHuge and sits within a few hundred bytes of it. */
int vxtSmartHasMoveHuge(void)
{
    int32_t d = (int32_t)sm_addrs.startMoveHuge - (int32_t)sm_addrs.startDrawHuge;
    if (d < 0) d = -d;
    return (sm_addrs.startMoveHuge != 0) && (sm_addrs.startDrawHuge != 0) && (d < 1024);
}

/* Added - see vxt_smart.h. */
void vxtSmartScaleHi(uint8_t hi) { sm_rec(0, hi, sm_addrs.setScaleHi); }

void vxtSmartDrawHuge16(int32_t physY, int32_t physX, uint16_t scale16)
{
    int32_t ry = sm_round_div(physY, (int32_t)scale16);
    int32_t rx = sm_round_div(physX, (int32_t)scale16);

    vxtSmartScale((uint8_t)(scale16 & 0xFF));
    vxtSmartScaleHi((uint8_t)(scale16 >> 8));

    /* Changed (Sec.72): proportional chaining - see
     * sm_chain_steps(). */
    sm_chain_steps(ry, rx, sm_addrs.startDrawHuge16);
}
/* Added - scale-32 draw, one record, no chaining. Caller must
 * have set scale 32 first (same contract as vxtSmartDraw()); |y|,|x| must
 * already be rate units, i.e. phys/32, and fit in +-100. See
 * vxt_smart.h's startDraw32 field and SM_startDraw32_d's own header for why
 * this scale specifically. Falls back to nothing if the app never published
 * the address - sm_rec() drops records with a zero routine address the same
 * way the other optional routines behave. */
void vxtSmartDraw32(int8_t y, int8_t x) { sm_need_scale(); sm_rec((uint8_t)y, (uint8_t)x, sm_addrs.startDraw32); }

/* Safety guard, and it is not optional.
 *
 * An app assembled before this routine existed never writes parm[24]/[25],
 * so sm_addrs.startDraw32 reads whatever stale parmRam garbage happens to
 * be there. For every other optional routine (startDrawBig, startDrawHuge,
 * setScaleHi) that is harmless, because an app that does not publish them
 * also never calls them. startDraw32 is different: the DISPATCHER decides
 * to use it, not the app - so a new STM32 firmware running against an old
 * application binary would emit records pointing at a garbage address, and
 * the 6809's `pulu a,b,pc` would jump straight into it. That is a hard
 * crash, not a visual glitch.
 *
 * Every SmartList routine is assembled from the same vxt_smart.asm include,
 * so a genuine startDraw32 always sits within a few hundred bytes of
 * startDrawBig (they are adjacent in that file). Stale garbage essentially
 * never satisfies both tests. Callers must check this before taking the
 * scale-32 path; gb_dispatch_auto() does. */
int vxtSmartHasDraw32(void)
{
    int32_t d = (int32_t)sm_addrs.startDraw32 - (int32_t)sm_addrs.startDrawBig;
    if (d < 0) d = -d;
    return (sm_addrs.startDraw32 != 0) && (d < 1024);
}

void vxtSmartRecenter(void)            { sm_rec(0, 0, sm_addrs.recenter); }
void vxtSmartMove(int8_t y, int8_t x)  { sm_need_scale(); sm_rec((uint8_t)y, (uint8_t)x, sm_addrs.startMove); }
void vxtSmartDraw(int8_t y, int8_t x)  { sm_need_scale(); sm_rec((uint8_t)y, (uint8_t)x, sm_addrs.startDraw); }
void vxtSmartCont(int8_t y, int8_t x)  { sm_need_scale(); sm_rec((uint8_t)y, (uint8_t)x, sm_addrs.cont); }
/* Changed: writes DIRECTLY, bypassing sm_rec()'s budget check,
 * into the slot vxtSmartBegin() reserved for it - see that function's own
 * comment. Going through sm_rec() is exactly what used to drop this
 * record on a full frame and leave the 6809 walking an unterminated list. */
void vxtSmartEnd(void)
{
    if (!sm_have_addrs) return;                   /* fails blank, not wild */
    vxtImgPut(sm_pos++, 0);
    vxtImgPut(sm_pos++, 0);
    vxtImgPut(sm_pos++, (uint8_t)(sm_addrs.end >> 8));
    vxtImgPut(sm_pos++, (uint8_t)(sm_addrs.end & 0xFF));
}
