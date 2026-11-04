/*
 * gamelib_beam.c - see gamelib_beam.h. Logic is a pure move from its
 * original location as an application's own beam-positioning helpers,
 * unchanged - the state variables below (gb_run_mode, gb_pos_scale,
 * gb_draw_scale) are just private to this module instead of file-static
 * in the calling application, and set once via gamelibBeamBegin().
 */
#include "gamelib_beam.h"
#include "../vxt/vxt_smart.h"

#define GB_RUN_NONE  -1
#define GB_RUN_MOVE   0
#define GB_RUN_DRAW   1

static uint8_t gb_pos_scale;
static uint8_t gb_draw_scale;
/* Per-element opt-out from the short-draw fast path. Default ON. A
 * renderer whose vertices must coincide with something drawn by another
 * path brackets itself with this OFF, and its edges fall back to the
 * ordinary scale-64 Big dispatch - the exact pre-fast-path behavior.
 *
 * Deliberately not the same thing as passing GAMELIB_SCALE_ACCURATE: that
 * also opts out, but onto the scale-searched Huge path at ~2.2x the
 * dispatch cost, which is the wrong trade for a scene already tight on
 * its record budget. This flag costs one static test and zero records. */
static int gb_fast_draw = 1;
static int     gb_run_mode;

/* Draw-gain compensation - see gamelib_beam.h for the measurement behind this
 * and why it is applied to draws only. 1000 = identity = no behavior change,
 * which is the default so this is inert until a caller opts in. */
static int16_t gb_draw_gain = 1000;

void gamelibBeamSetDrawGain(int16_t per1000)
{
    /* Clamped, not trusted: a stored calibration value that has been
     * corrupted (or fitted against a since-changed model) must not be able to
     * scale geometry into nonsense. +-30% is far outside anything measured. */
    if (per1000 < 700)  per1000 = 700;
    if (per1000 > 1300) per1000 = 1300;
    gb_draw_gain = per1000;
}

/* MOVE-path gain - the counterpart to gb_draw_gain above, which did not
 * exist until the move path itself had been measured for the first time.
 *
 * Every gb_gain_apply() call site is a DRAW. gamelibRepositionAbs() passed
 * raw coordinates straight through, so a hardware scale error on the move
 * ramp was completely uncompensated while draws were corrected by +5.3%.
 *
 * A hardware test rig measured it directly. Error at a figure's landing
 * point, against how far its opening reposition travels:
 *
 *   travel      0  ->  205 units   (no travel at all)
 *   travel   6000  ->  800         (mean of a full sweep)
 *   travel   6000  ->  900         (an independent figure, for comparison)
 *   travel  10630  -> 1262
 *
 * A line through the two ENDPOINTS gives error = 205 + 0.0994*travel, and
 * that predicts the un-fitted 6000-travel points to within ~100 units - a
 * prediction, not a curve fit. So the move OVERSHOOTS by ~9.9%, plus a
 * ~205-unit fixed term that survives at zero travel.
 *
 * This gain corrects the proportional 9.9%. The fixed term is deliberately
 * not corrected here: it does not scale, so it needs a different mechanism,
 * and it is worth confirming the dominant term first. Costs zero 6809
 * records - it changes the value in a record that was being emitted anyway,
 * exactly like the draw gain.
 *
 * NOTE the move and draw gains point in OPPOSITE directions (moves overshoot,
 * draws fall short). That is not a contradiction: SM_startMoveBig_d and
 * SM_startDrawBig_d carry different, independently hand-computed nop padding
 * (the scale-64 draw pairing is flagged UNVERIFIED in vxt_smart.asm), so the
 * two paths have no reason to share a timing error. */
static int16_t gb_move_gain = 1000;

void gamelibBeamSetMoveGain(int16_t per1000)
{
    if (per1000 < 700)  per1000 = 700;
    if (per1000 > 1300) per1000 = 1300;
    gb_move_gain = per1000;
}

static int32_t gb_move_gain_apply(int32_t v)
{
    if (gb_move_gain == 1000) return v;   /* identity fast path */
    return (v >= 0) ? ( (v * (int32_t)gb_move_gain + 500) / 1000)
                    : (-((-v * (int32_t)gb_move_gain + 500) / 1000));
}

/* Move settling correction. Added after a test rig's reposition-distance
 * sweep identified what two previous attempts had both missed.
 *
 * vxtSmartMoveBig() chains through sm_chain_steps(), which splits a move into
 * `steps` EQUAL steps, steps = ceil(|phys/scale|/100). The sweep measured the
 * landing error across the record boundaries:
 *
 *   dist  records  step   error   error/step
 *   3100     1     97.0   1014      10.45
 *   3300     2     51.5    506       9.83
 *   6300     2     98.5    975       9.90
 *   6500     3     67.7    676       9.99
 *   9500     3     99.0   1034      10.44
 *   9700     4     75.8    722       9.53
 *
 * The error is ~10.0x the size of one step - not the total distance, and not
 * the record count. That is why crossing a record boundary makes the error
 * DROP (3100 -> 1014 but 3300 -> 506): one more record means each step is
 * smaller. It also predicts figures it was not fitted on - a 6000-unit
 * reposition is 2 steps of 94, predicting 940 against 825-955 measured on
 * two independent test figures.
 *
 * Physically: the final step's ramp has not finished settling when the draw
 * begins, leaving an offset proportional to that ramp's slew. It explains the
 * two failed fixes too - a distance gain changed the distance but not the
 * step COUNT, so the step size barely moved and neither did the error; and
 * beam priming could not help because the fault is per-move, not a one-off
 * warm-up.
 *
 * Correcting it means emitting E such that E + k*(E/scale/steps) = target,
 * i.e. E = target * (100*scale*steps) / (100*scale*steps + k), with k in
 * hundredths (1000 = 10.00). Zero 6809 records - same steps, slightly
 * smaller values.
 *
 * Default 0 = off. Nothing sets it by default; a calibration rig does, to
 * test it. */
static int16_t gb_move_settle;
/* Only the FIRST reposition of a frame is corrected - see the note in
 * gamelibRepositionAbs(). Reset by gamelibBeamBegin(). */
static int     gb_move_settle_armed;

void gamelibBeamSetFastDraw(int enable) { gb_fast_draw = enable ? 1 : 0; }

void gamelibBeamSetMoveSettle(int16_t kHundredths)
{
    if (kHundredths < 0)    kHundredths = 0;
    if (kHundredths > 5000) kHundredths = 5000;
    gb_move_settle = kHundredths;
}

/* Global origin offset - see gamelib_beam.h for the measurement and the
 * before/after-move-gain ordering rationale. Default identity (0,0). */
static int16_t gb_offset_y;
static int16_t gb_offset_x;

void gamelibBeamSetOffset(int16_t offY, int16_t offX)
{
    /* Clamped, not trusted - same reasoning as every other stored
     * calibration value here: a corrupted or mis-fitted reading must not
     * be able to throw geometry off-window entirely. +-2000 comfortably
     * covers a bent unit with margin to spare. */
    if (offY < -2000) offY = -2000;
    if (offY >  2000) offY =  2000;
    if (offX < -2000) offX = -2000;
    if (offX >  2000) offX =  2000;
    gb_offset_y = offY;
    gb_offset_x = offX;
}

/* Nominal delta -> the delta to be emitted. */
static int32_t gb_gain_apply(int32_t v)
{
    if (gb_draw_gain == 1000) return v;   /* identity fast path */
    return (v >= 0) ? ( (v * (int32_t)gb_draw_gain + 500) / 1000)
                    : (-((-v * (int32_t)gb_draw_gain + 500) / 1000));
}

/* The inverse, for reporting what a caller's TRACKED position should become.
 * Callers track in nominal units; the beam physically lands where the move
 * path would have put that nominal delta, which is the whole point - so the
 * achieved value they record must be un-gained, not the emitted one. Getting
 * this backwards would reintroduce the same drift bug by a new route. */
static int32_t gb_gain_unapply(int32_t v)
{
    if (gb_draw_gain == 1000) return v;
    return (v >= 0) ? ( (v * 1000 + gb_draw_gain / 2) / (int32_t)gb_draw_gain)
                    : (-((-v * 1000 + gb_draw_gain / 2) / (int32_t)gb_draw_gain));
}

void gamelibBeamBegin(uint8_t posScale, uint8_t drawScale)
{
    gb_fast_draw = 1;   /* per-frame default ON, same discipline as
                         * gb_move_settle_armed below: a renderer that
                         * switches it off must not be able to leak that
                         * into the next frame. */
    gb_pos_scale = posScale;
    gb_draw_scale = drawScale;
    gb_run_mode = GB_RUN_NONE;
    gb_move_settle_armed = 1;   /* a new frame: the next reposition is the
                                 * first one, and the only one that carries
                                 * the settling fault */
}

/* See gamelib_beam.h for the measurement that motivated this and for what
 * it is (a structural fix, not a calibration value).
 *
 * Deliberately drives the beam OUT and back rather than just recentering in
 * place: the two candidate mechanisms are "the zero assertion needs repeating"
 * and "the integrators need to have been deflected before they behave", and
 * this exercises both. Splitting them is a later refinement, once the effect
 * is confirmed at all.
 *
 * Every record here is blanked, so nothing is visible - the only cost is
 * 6809 time. */
#define GB_PRIME_OFFSET  4000   /* a quarter-ish of the way out; enough to
                                 * move the integrators, small
                                 * enough that vxtSmartMoveBig() does not
                                 * chain many records to get there */

void gamelibBeamPrime(int cycles)
{
    int i;

    for (i = 0; i < cycles; i++) {
        vxtSmartMove(0, 0);          /* close any open run, blanked */
        vxtSmartRecenter();
        vxtSmartScale(gb_pos_scale);
        vxtSmartMoveBig(GB_PRIME_OFFSET, GB_PRIME_OFFSET, gb_pos_scale);
        vxtSmartMove(0, 0);
        vxtSmartRecenter();
    }
    vxtSmartScale(gb_draw_scale);
    gb_run_mode = GB_RUN_NONE;       /* next operation pays a fresh start,
                                      * exactly as after a reposition */
    /* Priming is itself the frame's first move, so it takes the settling
     * fault and the next reposition must not be corrected for it.
     *
     * This is why enabling the settle correction unconditionally made one
     * caller worse while a calibration rig measured it as fine. The rig
     * primes only on its own dedicated test screen, so on every screen the
     * model was fitted against, the first gamelibRepositionAbs() really is
     * the frame's first move. A caller that calls gamelibBeamPrime() right
     * after gamelibBeamBegin() has its first reposition already be a LATER
     * move, which lands clean - and correcting a clean move by up to 24% is
     * exactly the observed damage. Priming exists specifically because
     * "whatever is drawn FIRST each frame comes out offset"; it was already
     * solving this.
     *
     * Consuming the flag here keeps the correction attached to the real
     * first move whoever performs it, rather than to whichever API the
     * caller happens to reach first. The priming move's own landing accuracy
     * does not matter - it recenters immediately afterwards. */
    gb_move_settle_armed = 0;
}

int32_t gamelibRoundToScale(int32_t val, int32_t scale)
{
    int32_t q = (val >= 0) ? (val + scale / 2) / scale : (val - scale / 2) / scale;
    return q * scale;
}

void gamelibRepositionAbs(int32_t physY, int32_t physX)
{
    /* Closing move is UNCONDITIONAL: a recenter following EITHER an open
     * draw run OR a cold/no-prior-operation state needs a startMove-family
     * record in front of it, not only the former. */
    vxtSmartMove(0, 0);
    vxtSmartRecenter();
    vxtSmartScale(gb_pos_scale);
    /* Gain-compensated - see gb_move_gain. The caller's tracked position
     * stays NOMINAL on purpose: the beam physically lands where the
     * uncompensated coordinate would have put it, which is the whole point,
     * so every caller's own position bookkeeping is unaffected. Same
     * contract gb_gain_unapply() maintains for draws. */
    {
        int32_t my = gb_move_gain_apply(physY);
        int32_t mx = gb_move_gain_apply(physX);
        /* First reposition of the frame only. Narrowed after the first
         * correct-solver run on hardware.
         *
         * Correcting every reposition removed the step-size variation (one
         * sweep's spread went 528 -> 94, flat from 3100 to 9700) but left
         * the mean untouched at ~940. The reason is that a calibration
         * rig's own reference marks and its measuring caret are drawn
         * through this same function, so what it measures is the
         * DIFFERENCE between the first-drawn figure and later-drawn marks
         * - and shifting both by the same amount cannot change a
         * difference.
         *
         * Every test screen agrees that only the first element is
         * displaced at all: one screen's four later squares read 0,0,0,0
         * while its first reads 205, and another's independently-
         * positioned elements read 0-136 against its chained figure's
         * ~850. So the fault belongs to the first move after a frame
         * starts, not to moves in general, and correcting the rest was
         * both wrong and self-canceling. */
        if (gb_move_settle && gb_move_settle_armed) {
            gb_move_settle_armed = 0;
            /* Pick a step count that is SELF-CONSISTENT with the move we are
             * about to emit.
             *
             * The first version took the step count from the UNCORRECTED
             * value, but sm_chain_steps() recomputes it from what * gets emitted - and shrinking the move can drop it below a
             * record boundary. Losing a step makes every remaining step
             * BIGGER, so the error grew instead of vanishing on some
             * inputs while others, whose counts happened to survive,
             * improved 21-44%. A correction that changes its own input has
             * to close the loop.
             *
             * For a candidate n, factor = D/(D+k) with D = 100*scale*n, and
             * the emitted move lands at E*(1 + k/(100*scale*n_actual)) where
             * n_actual is what sm_chain_steps() will really use. Search n,
             * evaluate each against its OWN n_actual, and keep whichever
             * lands closest to nominal - so a boundary case degrades
             * gracefully instead of oscillating. */
            int32_t m = (my < 0 ? -my : my);
            int32_t ax = (mx < 0 ? -mx : mx);
            int32_t bestNum = 1, bestDen = 1, bestErr = -1;
            int n;

            if (ax > m) m = ax;
            m /= (int32_t)gb_pos_scale;

            if (m > 0) {
                for (n = 1; n <= 8; n++) {
                    int32_t D  = 100 * (int32_t)gb_pos_scale * n;
                    int32_t mn = (int32_t)(((int64_t)m * D) / (D + gb_move_settle));
                    int32_t na = (mn + 99) / 100;      /* what will REALLY be used */
                    int32_t Da, land, err;
                    if (na < 1) na = 1;
                    Da   = 100 * (int32_t)gb_pos_scale * na;
                    /* where mn ends up, in the same scaled units */
                    land = mn + (int32_t)(((int64_t)mn * gb_move_settle) / Da);
                    err  = land - m;
                    if (err < 0) err = -err;
                    if (bestErr < 0 || err < bestErr) {
                        bestErr = err; bestNum = D; bestDen = D + gb_move_settle;
                    }
                    if (na <= n) break;   /* larger n only shrinks further */
                }
                my = (int32_t)(((int64_t)my * bestNum) / bestDen);
                mx = (int32_t)(((int64_t)mx * bestNum) / bestDen);
            }
        }
        /* Global origin offset - added last, after every proportional
         * correction (move gain, move settle), so it lands as a fixed
         * deflection-space shift rather than something the proportional
         * corrections scale. See gamelibBeamSetOffset()'s own header. */
        my += (int32_t)gb_offset_y;
        mx += (int32_t)gb_offset_x;
        vxtSmartMoveBig(my, mx, gb_pos_scale);
    }
    vxtSmartScale(gb_draw_scale);
    gb_run_mode = GB_RUN_NONE;
}

/* Proportional step distribution - same real dogleg bug (and same fix) as
 * vxt_smart.c's sm_chain_steps() - see that function's comment for the full
 * derivation. Clamping each axis independently, so any delta needing more
 * than one record with unequal per-axis step counts was drawn as a bend
 * rather than a straight chord, was the earlier (incorrect) behavior.
 * Endpoint is unchanged; only the path between steps is corrected. */
void gamelibChainDelta(int32_t dy, int32_t dx, int wantDraw)
{
    int32_t ry, rx;

    /* Round first, then gain - the order matters and was got wrong once.
     *
     * Every caller of this function that tracks a running position does so
     * as `actual += gamelibRoundToScale(dy, drawScale)`. Gain-compensating
     * `dy` BEFORE the rounding breaks that contract: the caller's
     * assumed step and the step emitted then quantise differently,
     * and the mismatch accumulates across a chained shape. That is exactly
     * the drift bug described above, arriving by a new route, and it showed
     * up on hardware as one chained element's own bracket sitting high
     * relative to a freshly-repositioned neighbour.
     *
     * Rounding the NOMINAL delta first keeps `roundToScale(dy, drawScale)`
     * exactly true for every caller, and the gain is then applied to the
     * already-rounded rate values emitted. Physical result is the
     * same; the caller's arithmetic stays valid and no call site changes. */
    ry = (dy >= 0) ? (dy + gb_draw_scale / 2) / gb_draw_scale
                   : (dy - gb_draw_scale / 2) / gb_draw_scale;
    rx = (dx >= 0) ? (dx + gb_draw_scale / 2) / gb_draw_scale
                   : (dx - gb_draw_scale / 2) / gb_draw_scale;

    /* Compensate DRAWS only. A wantDraw==0 call is a blanked move, which is
     * the reference path this correction is measured against - compensating it
     * would move the target rather than hit it. */
    if (wantDraw) { ry = gb_gain_apply(ry); rx = gb_gain_apply(rx); }
    {
    int wantMode = wantDraw ? GB_RUN_DRAW : GB_RUN_MOVE;
    int32_t maxAbs = (ry < 0) ? -ry : ry;
    int32_t absRx  = (rx < 0) ? -rx : rx;
    int32_t steps, k, prevY = 0, prevX = 0;

    if (absRx > maxAbs) maxAbs = absRx;
    if (maxAbs == 0) return;
    steps = (maxAbs + 99) / 100;

    for (k = 1; k <= steps; k++) {
        int32_t curY = (ry * k >= 0) ? (ry * k + steps / 2) / steps
                                     : (ry * k - steps / 2) / steps;
        int32_t curX = (rx * k >= 0) ? (rx * k + steps / 2) / steps
                                     : (rx * k - steps / 2) / steps;
        int8_t sy = (int8_t)(curY - prevY);
        int8_t sx = (int8_t)(curX - prevX);

        if (gb_run_mode != wantMode) {
            if (wantDraw) vxtSmartDraw(sy, sx); else vxtSmartMove(sy, sx);
            gb_run_mode = wantMode;
        } else {
            vxtSmartCont(sy, sx);
        }
        prevY = curY;
        prevX = curX;
    }
    }
}

void gamelibDrawBigLine(int32_t absY, int32_t absX, int32_t dy, int32_t dx,
                        uint8_t intensity, uint8_t bigScale)
{
    vxtSmartIntensity(intensity);
    gamelibRepositionAbs(absY, absX);
    vxtSmartScale(bigScale);
    /* Compensated here too - this path emits its record directly and so is
     * not covered by either dispatcher above. */
    vxtSmartDrawBig(gb_gain_apply(dy), gb_gain_apply(dx), bigScale);
    vxtSmartScale(gb_draw_scale);
    gb_run_mode = GB_RUN_DRAW;
}

/* The ONE place the BigScale-vs-Huge/Huge16 decision is made, so every
 * caller in this file automatically gets it, instead of each render
 * function needing its own escalation logic. Assumes the beam is ALREADY
 * at the correct position and intensity is already set - callers that need
 * a fresh reposition/intensity do that themselves (gamelibDrawAutoLine())
 * before calling this; gamelibChainBigGrouped() deliberately does not
 * reposition between segments (that's its whole cost-saving point), so it
 * calls this directly mid-chain. */
/* A real bug, not a data/orientation issue: gb_dispatch_huge()/
 * gb_dispatch_auto() used to just draw and return void, leaving every
 * caller to guess the landed position via `gamelibRoundToScale(dy,
 * bigScale)` using its own preferred/fixed scale - but the actual scale
 * USED is whatever this function privately computed (`neededScale`,
 * escalated well past `bigScale` on any edge long enough to need it - e.g.
 * every drawn edge of one model's outer ring, which needed scale 65-96
 * against a preferred/fixed 64). The caller's tracked "actual" position
 * then diverged from the real landed position by the rounding
 * difference between the two scales, compounding every subsequent chained
 * segment - this is the reported drift. Fixed by having both functions
 * OUTPUT the delta they achieved (rounded to whatever scale was
 * really used), so every caller tracks the true landed position regardless
 * of which internal path fired. */
/* How far past the minimum scale to search for a better-fitting one. See
 * gb_dispatch_huge() below. */
#define GB_SCALE_SEARCH_SPAN  32
#define GB_MAX_RATE          100   /* this project's established per-record
                                    * rate limit (vxt_smart.asm) */

/* "_raw" takes an already gain-COMPENSATED delta and returns its achieved
 * value in those same compensated units. The gain is applied by the callers
 * below, exactly once - gb_dispatch_auto() escalates into this function
 * mid-computation, so applying it here as well would double-compensate
 * every long edge. */
static void gb_dispatch_huge_raw(int32_t dy, int32_t dx, int32_t *outDy, int32_t *outDx)
{
    int32_t maxAbs = (dy < 0) ? -dy : dy;
    int32_t absDx  = (dx < 0) ? -dx : dx;
    int32_t neededScale, s, bestScale, bestErr;

    if (absDx > maxAbs) maxAbs = absDx;
    neededScale = (maxAbs <= 0) ? 1 : (maxAbs + GB_MAX_RATE - 1) / GB_MAX_RATE;

    /* The misalignment fix, at ZERO cost in 6809 records.
     *
     * A single record draws (dy,dx) as (sy*s, sx*s) with |sy|,|sx| <=
     * GB_MAX_RATE. That constrains s only from BELOW: s must be at least
     * ceil(maxAbs/GB_MAX_RATE), but any larger s is equally legal and
     * still exactly ONE record. This function used to take the smallest
     * legal s unconditionally - the right choice for minimising record
     * COUNT, but it left the remaining freedom completely unused, and the
     * endpoint error it produces is +-s/2 per axis.
     *
     * That error is what misaligned a chained ring against its own
     * connectors: measured by simulating the exact emitted record stream,
     * the outer ring's vertices landed up to 47 units from ideal and up to
     * 61 units from where the independently-repositioned connectors start
     * (~3px) - small in absolute terms, but each vertex carries a bright
     * dwell dot, which is exactly where a few pixels are most visible.
     * (This also explains why the position-tracking fix above - a real
     * bug, correctly fixed - did not move the symptom: it corrected which
     * position was TRACKED, not how coarsely the endpoint was QUANTISED.)
     *
     * Fix: search a small window of legal scales and keep the one whose
     * rounding lands closest to the true endpoint. Still ONE record, still
     * the single-big-line dispatch this file already had - the search
     * costs only STM32 cycles (a few dozen divides), which is the right
     * place to spend, since the 6809 is the constrained CPU and its record
     * count is unchanged. Measured result: worst vertex error 47 -> 16
     * units, ring-vs-connector gap 61 -> 4 units (0.2px).
     *
     * Do not "fix" precision here by capping the scale and chaining more
     * records - that was tried once and reverted the same day. It defeats
     * the entire purpose of the Huge/Huge16 single-record path, whose
     * reason for existing is 6809 cycle savings over chaining. Precision
     * and record count are not a necessary tradeoff; the scale search gets
     * both. */
    if (maxAbs > 0) {
        bestScale = neededScale;
        bestErr = -1;
        for (s = neededScale; s <= neededScale + GB_SCALE_SEARCH_SPAN; s++) {
            int32_t ey = gamelibRoundToScale(dy, s) - dy;
            int32_t ex = gamelibRoundToScale(dx, s) - dx;
            int32_t err = ((ey < 0) ? -ey : ey) + ((ex < 0) ? -ex : ex);
            if (s > 65535) break;
            if (bestErr < 0 || err < bestErr) {
                bestErr = err;
                bestScale = s;
                if (err == 0) break;   /* exact - cannot do better */
            }
        }
        neededScale = bestScale;
    }

    if (neededScale > 255) {
        /* Cap at 65535 (uint16_t's own width) - defensive only; this
         * project's largest realistic edge (~303,000 phys units, a deep
         * model's near-plane worst case) needs scale ~3030, nowhere near
         * this ceiling. */
        uint16_t scale16 = (uint16_t)(neededScale > 65535 ? 65535 : neededScale);
        vxtSmartDrawHuge16(dy, dx, scale16);
        *outDy = gamelibRoundToScale(dy, (int32_t)scale16);
        *outDx = gamelibRoundToScale(dx, (int32_t)scale16);
    } else {
        uint8_t scale = (uint8_t)neededScale;
        vxtSmartScale(scale);
        vxtSmartDrawHuge(dy, dx, scale);
        *outDy = gamelibRoundToScale(dy, (int32_t)scale);
        *outDx = gamelibRoundToScale(dx, (int32_t)scale);
    }
    vxtSmartScale(gb_draw_scale);
}

/* Fits (dy, dx) in ONE record at `preferredScale`? Use the cheap BigScale
 * dispatch (no Timer-1 poll overhead) - otherwise escalate to gb_dispatch_huge().
 * Shared by gamelibDrawAutoLine() and gamelibChainBigGrouped(), so a
 * per-segment escalation inside a chained/grouped shape gets exactly the
 * same treatment as a standalone independent line, without either needing
 * its own copy of this logic.
 * Outputs the ACTUAL achieved (dy,dx) - see gb_dispatch_huge()'s own
 * comment for why this matters. */
static void gb_dispatch_auto(int32_t dy, int32_t dx, uint8_t preferredScale,
                             int32_t *outDy, int32_t *outDx)
{
    int32_t maxAbs, absDx;

    /* Gain compensation applied once, here, before any scale/record decision -
     * the record must be chosen for the delta being emitted, not the
     * nominal one, or a compensated edge could need a different
     * scale than the one picked for it. */
    dy = gb_gain_apply(dy);
    dx = gb_gain_apply(dx);

    maxAbs = (dy < 0) ? -dy : dy;
    absDx  = (dx < 0) ? -dx : dx;
    if (absDx > maxAbs) maxAbs = absDx;

    /* GAMELIB_SCALE_ACCURATE (0): skip the cheap fixed-scale BigScale
     * dispatch entirely and go straight to gb_dispatch_huge()'s scale
     * search. The BigScale path CANNOT be made accurate, because
     * SM_startDrawBig_d's NOP padding is hand-computed for scale 64
     * exactly, so its scale is not free to be searched. Both paths emit
     * exactly ONE record for a line this length; the accurate one just
     * costs a genuine Timer-1 poll on the 6809 (~2.2x a BigScale dispatch)
     * instead of fixed padding. Worth it only for geometry whose vertices
     * must coincide with an independently-drawn element. Everything else
     * (self-contained models) should keep passing a real preferred scale
     * and stay on the cheap path. */
    /* Scale ladder - pick the SMALLEST nop-calibrated routine that reaches
     * this line, instead of paying scale 64 for everything.
     *
     * SM_startDrawBig_d costs 97 cycles, 52 of them NOPs waiting out the
     * scale-64 beam ramp - and Timer 1 counts down from the SCALE, not from
     * the line's length, so a 2000-unit edge waits exactly as long as a
     * 6400-unit one. Not all long lines need the same wait.
     *
     *   scale 32  reaches 3200 units,  67 cyc   (SM_startDraw32_d, 12 nops)
     *   scale 64  reaches 6400 units,  97 cyc   (SM_startDrawBig_d, 27 nops)
     *
     * so every edge in 1200..3200 - a band that is currently paying full
     * scale-64 - drops 30 cycles at the same one record.
     *
     * SCALE 12 is deliberately not in this ladder. It is cheaper still (45
     * cyc) and was tried, but it is the ONE draw scale that is not a
     * divisor relationship with the reposition scale (32): 64 = 2*32 and
     * 32 = 32, so both land on the grid repositions land on, while
     * multiples of 12 only coincide with it every 96 units. That mismatch
     * showed on hardware as some vertices drawing as two separate dots. The
     * ladder buys back most of the speed WITHOUT reintroducing it.
     *
     * Note this also means the ladder is a strict improvement on the
     * ORIGINAL all-scale-64 behavior - it never picks a coarser routine
     * than before, only a finer one where that still reaches. */
    if (gb_fast_draw &&
        preferredScale != GAMELIB_SCALE_ACCURATE &&
        maxAbs <= 100L * (int32_t)GAMELIB_LADDER_SCALE_LO &&
        vxtSmartHasDraw32()) {   /* MANDATORY: startDraw32 is optional in the
                                  * address handshake, and an app assembled
                                  * before this routine existed leaves stale
                                  * parmRam garbage there - the 6809 would
                                  * `pulu a,b,pc` straight into it. */
        const int32_t S = (int32_t)GAMELIB_LADDER_SCALE_LO;
        int32_t ry = (dy >= 0) ? (dy + S / 2) / S : (dy - S / 2) / S;
        int32_t rx = (dx >= 0) ? (dx + S / 2) / S : (dx - S / 2) / S;

        if (ry == 0 && rx == 0) {
            *outDy = 0;
            *outDx = 0;
        } else {
            vxtSmartScale(GAMELIB_LADDER_SCALE_LO);
            vxtSmartDraw32((int8_t)ry, (int8_t)rx);
            vxtSmartScale(gb_draw_scale);   /* restore - downstream callers
                                    * (gamelibChainDelta) emit bare records
                                    * assuming the register holds this */
            gb_run_mode = GB_RUN_DRAW;
            *outDy = ry * S;
            *outDx = rx * S;
        }
    } else if (preferredScale != GAMELIB_SCALE_ACCURATE &&
        maxAbs <= 100L * (int32_t)preferredScale) {
        vxtSmartScale(preferredScale);
        vxtSmartDrawBig(dy, dx, preferredScale);
        vxtSmartScale(gb_draw_scale);
        *outDy = gamelibRoundToScale(dy, (int32_t)preferredScale);
        *outDx = gamelibRoundToScale(dx, (int32_t)preferredScale);
    } else {
        gb_dispatch_huge_raw(dy, dx, outDy, outDx);
    }

    /* Report the achieved delta in NOMINAL units - the beam now physically
     * sits where the move path would have put the uncompensated delta, and
     * that is what every caller tracks its position in. */
    *outDy = gb_gain_unapply(*outDy);
    *outDx = gb_gain_unapply(*outDx);
}

/* The huge-line counterpart to gamelibDrawBigLine(). always uses the
 * genuine-Timer-1-poll path (unlike gamelibDrawAutoLine(), which prefers
 * the cheap BigScale dispatch when it fits) - use this specifically when a
 * caller wants the proven-safe technique unconditionally, not just as a
 * fallback. */
void gamelibDrawHugeLine(int32_t absY, int32_t absX, int32_t dy, int32_t dx,
                        uint8_t intensity)
{
    int32_t achievedDy, achievedDx;   /* unused here - this call always
                                       * repositions fresh next time, no
                                       * running position to keep accurate */
    vxtSmartIntensity(intensity);
    gamelibRepositionAbs(absY, absX);
    gb_dispatch_huge_raw(gb_gain_apply(dy), gb_gain_apply(dx),
                         &achievedDy, &achievedDx);
    gb_run_mode = GB_RUN_DRAW;
}

/* See gamelib_beam.h. */
void gamelibDrawAutoLine(int32_t absY, int32_t absX, int32_t dy, int32_t dx,
                        uint8_t intensity, uint8_t preferredScale)
{
    int32_t achievedDy, achievedDx;   /* unused - see gamelibDrawHugeLine() */
    vxtSmartIntensity(intensity);
    gamelibRepositionAbs(absY, absX);
    gb_dispatch_auto(dy, dx, preferredScale, &achievedDy, &achievedDx);
    gb_run_mode = GB_RUN_DRAW;
}

/* Outputs the ACTUAL achieved (dy,dx), fixing the drift bug described at
 * gb_dispatch_huge()'s own comment. Callers that track a running position
 * must use these outputs, not their own preferredScale-based guess. */
void gamelibDispatchAutoDraw(int32_t dy, int32_t dx, uint8_t preferredScale,
                             int32_t *outDy, int32_t *outDx)
{
    gb_dispatch_auto(dy, dx, preferredScale, outDy, outDx);
    gb_run_mode = GB_RUN_DRAW;
}

void gamelibBeamMarkOpenDraw(void)
{
    gb_run_mode = GB_RUN_DRAW;
}

/* Per-segment dispatch goes through gb_dispatch_auto() instead of a
 * hardcoded vxtSmartScale()/vxtSmartDrawBig() pair - a segment that happens
 * to be too long for one record at `bigScale` now escalates to Huge/Huge16
 * automatically, WITHOUT inserting a reposition - preserving this
 * function's entire reason for existing (avoid a recenter between
 * connected segments). Every caller of this shared primitive gets this for
 * free, not just whichever one prompted it. */
void gamelibChainBigGrouped(int32_t dy, int32_t dx, uint8_t bigScale,
                            int32_t *actualY, int32_t *actualX,
                            int *segCounter, int groupSize)
{
    int32_t achievedDy, achievedDx;

    if (groupSize > 0 && *segCounter >= groupSize) {
        gamelibRepositionAbs(*actualY, *actualX);
        *segCounter = 0;
    }
    /* Track the ACTUAL achieved delta, not gamelibRoundToScale(dy,
     * bigScale) - that assumed bigScale was what got used, which
     * is false the moment this segment escalates to the Huge path (a real,
     * silent drift bug - see gb_dispatch_huge()'s own comment). */
    gb_dispatch_auto(dy, dx, bigScale, &achievedDy, &achievedDx);
    *actualY += achievedDy;
    *actualX += achievedDx;
    (*segCounter)++;
    gb_run_mode = GB_RUN_DRAW;
}

/* See gamelib_beam.h for the measurement that motivated these three (54%
 * of one complex model's records were redundant set-scale records) and for
 * the required usage pattern. */
void gamelibBeamSetDrawScale(uint8_t drawScale)
{
    gb_draw_scale = drawScale;
}

void gamelibChainBigHeld(int32_t dy, int32_t dx, uint8_t bigScale,
                         int32_t *actualY, int32_t *actualX)
{
    int32_t gy = gb_gain_apply(dy);
    int32_t gx = gb_gain_apply(dx);
    int32_t maxAbs = (gy < 0) ? -gy : gy;
    int32_t absGx  = (gx < 0) ? -gx : gx;

    if (absGx > maxAbs) maxAbs = absGx;

    if (maxAbs <= 100L * (int32_t)bigScale) {
        /* The point of this function: one bare record. vxtSmartDrawBig()
         * emits no scale record of its own (checked in vxt_smart.c - it is a
         * straight sm_chain_steps() call), so the scale the caller set once
         * before the run carries this edge and every other one. */
        vxtSmartDrawBig(gy, gx, bigScale);
        *actualY += gb_gain_unapply(gamelibRoundToScale(gy, (int32_t)bigScale));
        *actualX += gb_gain_unapply(gamelibRoundToScale(gx, (int32_t)bigScale));
    } else {
        /* Too long for one record at bigScale - same escalation every other
         * draw path in this file takes. It emits its own scale records and
         * restores to gb_draw_scale, which the caller has set to bigScale for
         * the duration, so the register comes back where the next edge wants
         * it with no extra record from us. */
        int32_t achievedDy, achievedDx;
        gb_dispatch_huge_raw(gy, gx, &achievedDy, &achievedDx);
        *actualY += gb_gain_unapply(achievedDy);
        *actualX += gb_gain_unapply(achievedDx);
    }
    gb_run_mode = GB_RUN_DRAW;
}

void gamelibChainAccurateHeld(int32_t dy, int32_t dx,
                              int32_t *actualY, int32_t *actualX)
{
    int32_t gy = gb_gain_apply(dy);
    int32_t gx = gb_gain_apply(dx);
    int32_t maxAbs = (gy < 0) ? -gy : gy;
    int32_t absGx  = (gx < 0) ? -gx : gx;
    int32_t neededScale, s, bestScale, bestErr;

    if (absGx > maxAbs) maxAbs = absGx;
    if (maxAbs == 0) return;   /* nothing to draw, and no scale to pick */

    /* Same scale search as gb_dispatch_huge_raw(): s is constrained only
     * from BELOW, so spend STM32 cycles finding the legal s that lands
     * closest to the target - never 6809 records. Inlined rather than
     * calling gb_dispatch_huge_raw() because that function ends with a
     * vxtSmartScale(gb_draw_scale) restore, which is exactly the record
     * this path exists to avoid paying per edge. */
    neededScale = (maxAbs + GB_MAX_RATE - 1) / GB_MAX_RATE;
    if (neededScale < 1) neededScale = 1;
    bestScale = neededScale;
    bestErr = -1;
    for (s = neededScale; s <= neededScale + GB_SCALE_SEARCH_SPAN; s++) {
        int32_t ey = gamelibRoundToScale(gy, s) - gy;
        int32_t ex = gamelibRoundToScale(gx, s) - gx;
        int32_t err = ((ey < 0) ? -ey : ey) + ((ex < 0) ? -ex : ex);
        if (s > 65535) break;
        if (bestErr < 0 || err < bestErr) {
            bestErr = err;
            bestScale = s;
            if (err == 0) break;
        }
    }

    if (bestScale > 255) {
        uint16_t scale16 = (uint16_t)(bestScale > 65535 ? 65535 : bestScale);
        vxtSmartDrawHuge16(gy, gx, scale16);   /* emits its own scale lo+hi */
        *actualY += gb_gain_unapply(gamelibRoundToScale(gy, (int32_t)scale16));
        *actualX += gb_gain_unapply(gamelibRoundToScale(gx, (int32_t)scale16));
    } else {
        vxtSmartScale((uint8_t)bestScale);
        vxtSmartDrawHuge(gy, gx, (uint8_t)bestScale);
        *actualY += gb_gain_unapply(gamelibRoundToScale(gy, bestScale));
        *actualX += gb_gain_unapply(gamelibRoundToScale(gx, bestScale));
    }
    gb_run_mode = GB_RUN_DRAW;
}

/* Same escalation as gamelibChainBigGrouped() above, for the same reason -
 * a perimeter edge that happens to exceed one record's reach at `bigScale`
 * now escalates instead of chaining multiple unverified-timing
 * records. */
void gamelibChainBigClosedPerimeter(const int32_t *py, const int32_t *px, int n,
                                    uint8_t bigScale, uint8_t intensity)
{
    int32_t actualY, actualX, startY, startX;
    int i;

    vxtSmartIntensity(intensity);   /* BEFORE reposition - see the ordering
                                     * rule at gamelibDrawBigLine() above */
    gamelibRepositionAbs(py[0], px[0]);
    startY = gamelibRoundToScale(py[0], gb_pos_scale);
    startX = gamelibRoundToScale(px[0], gb_pos_scale);
    actualY = startY;
    actualX = startX;
    for (i = 1; i <= n; i++) {
        int32_t ti = i % n;
        int32_t targetY = (ti == 0) ? startY : py[ti];
        int32_t targetX = (ti == 0) ? startX : px[ti];
        int32_t dy = targetY - actualY;
        int32_t dx = targetX - actualX;
        int32_t achievedDy, achievedDx;
        /* Same real drift bug as gamelibChainBigGrouped(): track what was
         * drawn, not an assumption that bigScale was used (false
         * once a segment escalates to the Huge path). */
        gb_dispatch_auto(dy, dx, bigScale, &achievedDy, &achievedDx);
        actualY += achievedDy;
        actualX += achievedDx;
    }
    gb_run_mode = GB_RUN_DRAW;
}

void gamelibBeamCloseRun(void)
{
    vxtSmartMove(0, 0);
    gb_run_mode = GB_RUN_MOVE;
}

/* See gamelib_beam.h. Exact at k==n by construction - the last segment must
 * land, so it gets the full D rather than n rounded n-ths of it. */
void gamelibChainCloseOffset(int32_t dy, int32_t dx, int k, int n,
                             int32_t *outY, int32_t *outX)
{
    if (n <= 0 || k <= 0) { *outY = 0; *outX = 0; return; }
    if (k >= n)           { *outY = dy; *outX = dx; return; }
    *outY = (dy * (int32_t)k) / (int32_t)n;
    *outX = (dx * (int32_t)k) / (int32_t)n;
}
