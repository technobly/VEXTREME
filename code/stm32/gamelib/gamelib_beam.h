/*
 * gamelib_beam.h - reusable STM32-side beam positioning and chained
 * big-scale drawing, built on top of vxt_smart (code/stm32/vxt/).
 *
 * STM32-side only - not part of the 6809/STM32 wire protocol itself.
 * Distinguish from vxt/: vxt/ is the cross-CPU toolkit (protocol, memory
 * map, RPC), gamelib/ is reusable STM32 drawing math that any application
 * can call.
 *
 * Problem this solves: a shape made of several long (>127*scale unit) line
 * segments needs to (a) reposition to an absolute point WITHOUT the visible
 * artifact `vxtSmartRecenter()` leaves when called right after an open draw
 * run (hardware-confirmed), and (b) chain multiple segments/records so the
 * SmartList emitter only pays a fresh dispatch at real mode changes, not
 * every segment. Both require tracking one shared piece of state - whether
 * the beam is currently mid-draw, mid-move, or neither - across every call
 * in an emitted list. This header's functions carry that state internally
 * (mirroring vxt_smart.c's own sm_pos/sm_count/sm_last_intensity pattern),
 * reset once per list via gamelibBeamBegin().
 */
#ifndef GAMELIB_BEAM_H
#define GAMELIB_BEAM_H

#include <stdint.h>

/* Call once per emitted list/frame, alongside vxtSmartBegin() (before any of
 * the other functions below) - resets the internal run-mode tracker to
 * "nothing has been positioned or drawn yet" and records the two scale
 * bytes the rest of this header assumes are the caller's "resting" scales:
 *   posScale  - the scale gamelibRepositionAbs() moves at (vxtSmartMoveBig).
 *   drawScale - the scale gamelibChainDelta() chains at, and the scale
 *               gamelibRepositionAbs()/gamelibDrawBigLine() restore
 *               vxtSmartScale() to when they're done, so the caller can
 *               freely mix small chained draws (drawScale) and the big
 *               single-record draws below (their own explicit bigScale). */
void gamelibBeamBegin(uint8_t posScale, uint8_t drawScale);

/* DRAW-gain compensation.
 * Default is IDENTITY (1000), so nothing changes until a caller sets it.
 *
 * The measurement. Independent hardware measurements find that a DRAWN
 * line covers less physical distance than a blanked REPOSITION does, for
 * the same nominal delta. On a test screen where the reference tick, the
 * line's start and the measuring caret are all placed by the move path and
 * only the line body uses the draw path, so the comparison isolates
 * exactly this:
 *
 *   L =  2,000  ->  -5.00%      L =  8,000  ->  -5.64%
 *   L =  4,000  ->  -4.23%      L = 16,000  ->  -6.86%
 *
 * Why this misaligns A chained figure against an independent reposition.
 * If a figure reaches each vertex by chaining drawn edges while some other
 * element reaches those same vertices by an independent reposition, and
 * drawn deltas fall ~5% short, the chained figure's accumulated position
 * drifts from nominal while the independently-repositioned element sits
 * exactly on nominal. Around a shape of ~10,000-unit edges that is hundreds
 * to thousands of units - large enough to be visible. A scale-quantisation
 * correction (rounding error of only tens of units) does not explain or fix
 * this; the gain error is one to two orders of magnitude larger.
 *
 * `per1000` is the multiplier applied to every DRAWN delta, in thousandths:
 * 1000 = identity, 1053 = "draw 5.3% further to match the move path".
 *
 * COSTS ZERO 6809 RECORDS - it changes the numbers inside records that were
 * being emitted anyway, and is pure STM32 arithmetic.
 *
 * Applies to DRAW paths only, never to moves/repositions: the move path is
 * the reference this is being corrected TOWARDS, so correcting it too would
 * just move the target.
 *
 * PROVISIONAL. The measured spread is real and unexplained - one test
 * reads ~4-7%, another's non-first elements read ~3% - so one constant
 * cannot be right everywhere yet. Whether it is scale-dependent is
 * unknown (it was measured at only one length). Treat the first
 * value as a hypothesis to be tested on hardware, not a settled
 * calibration. */
void gamelibBeamSetDrawGain(int16_t per1000);
/* MOVE-path gain, per-1000 (1000 = identity = no change). Corrects the ~9.9%
 * overshoot measured on repositions - see gb_move_gain in gamelib_beam.c for
 * the measurements and the derivation. Clamped to +-30%. Costs zero 6809
 * records. */
void gamelibBeamSetMoveGain(int16_t per1000);
/* Move settling correction, k in hundredths (1000 = 10.00, 0 = off). The
 * reposition's landing error is ~k x the size of one chaining step - see
 * gb_move_settle in gamelib_beam.c for the measurements behind it. Zero
 * 6809 records. */
void gamelibBeamSetMoveSettle(int16_t kHundredths);

/* Opt a renderer out of the short-draw fast path. Its edges fall back to
 * the ordinary scale-64 Big dispatch, i.e. exactly the pre-fast-path
 * behavior, at no extra record or cycle cost.
 *
 * Use where vertices must coincide with something drawn by a different
 * path. Bracket the renderer:  SetFastDraw(0) ... SetFastDraw(1).
 * Reset to 1 by gamelibBeamBegin() every frame, so a missed restore cannot
 * leak. Prefer this over passing GAMELIB_SCALE_ACCURATE when you only want
 * the fast path off - ACCURATE also moves you onto the Huge path at ~2.2x
 * the dispatch cost. Costs zero records. */
void gamelibBeamSetFastDraw(int enable);

/* Global origin offset - the screen-centre correction. Default is IDENTITY
 * (0,0), so nothing changes until a caller sets it.
 *
 * WHY. A physical Vectrex's true screen centre is not guaranteed to sit
 * exactly where the DAC's own zero deflection is - measured by comparing
 * where a reference mark drawn at nominal (0,0) lands against a
 * known physical position.
 *
 * `offY`/`offX` are in the same physical units as gamelibRepositionAbs()'s
 * own arguments (unscaled by posScale), and are ADDED after move-gain
 * compensation - a DAC-zero shift is a fixed offset in deflection space,
 * not something the proportional move-gain correction should scale.
 *
 * Applied to absolute positions only (gamelibRepositionAbs()), never to
 * deltas (gamelibChainDelta()) - a pure translation cancels out of any
 * difference between two already-offset points, so every figure that
 * starts from a reposition shifts uniformly and a chain's own internal
 * shape is unaffected. Costs zero 6809 records.
 *
 * Known gap, not assumed fixed: text positioning
 * (`vxtSmartTextBegin()`, a separate primitive in vxt_smart_text.c) does
 * not go through gamelibRepositionAbs() and is untouched by this - a real
 * physical offset would leave text slightly disagreeing with corrected
 * geometry until vxt_smart_text.c gets its own equivalent hook. */
void gamelibBeamSetOffset(int16_t offY, int16_t offX);

/* Beam priming - call once per frame, right after gamelibBeamBegin(), BEFORE
 * any real geometry.
 *
 * WHY. Measured on hardware: the FIRST element drawn in a frame lands
 * significantly misplaced. On test screens where a like-for-like comparison
 * is possible - all items the same size and draw path - the first-drawn
 * item's error is 2.5x, 2.9x and 4.8x the mean of every other item on its
 * own screen. The cleanest case is a 12-spoke starburst: spoke 0 reads +403
 * units while all eleven others sit in a coherent 107-230 band.
 *
 * This also explains why drawing a reference figure first (before the real
 * geometry) reads as "better centred" - drawing several repositions first
 * means the real geometry is no longer the frame's first element.
 *
 * The presumed mechanism is that a recenter does not fully zero the analog
 * integrators in one go, and its residual depends on where the beam was
 * beforehand - which is why the error does not cancel out of the
 * arithmetic the way a constant offset would.
 *
 * What this is not: not a calibration value. There is no per-unit constant
 * to measure or store here - it is a structural fix that costs a handful of
 * blanked records and applies to every unit. Whether the RIGHT number of
 * cycles is 1, 2 or 8 IS measurable on a hardware test screen.
 *
 * Cost: ~6 records per cycle, all blanked (invisible). At the default 2
 * that is ~12 of the 1536-record frame budget, under 1%.
 *
 * Each cycle drives the beam to an offset and then re-zeroes, so it
 * exercises both candidate mechanisms at once (repeated zero assertion,
 * and deflecting the integrators before real work). If it proves
 * effective, a cheaper minimal form can be searched for later - do not
 * assume this exact shape is the minimum. */
void gamelibBeamPrime(int cycles);

/* Round-to-nearest (not truncating) integer division by `scale`, then
 * multiply back - i.e. "what physical delta does vxtSmartMoveBig()/
 * vxtSmartDrawBig() produce for this (value, scale) pair, after
 * their own round-to-nearest chaining." Needed to track the beam's real
 * landed position across a chained shape instead of drifting against the
 * ideal target. */
int32_t gamelibRoundToScale(int32_t val, int32_t scale);

/* Repositions the beam to an ABSOLUTE (physY, physX), via: a closing blanked
 * move (unconditional - this must never be skipped, even from a cold/
 * no-prior-operation state), recenter, then a vxtSmartMoveBig() at posScale
 * (set via gamelibBeamBegin()). Leaves vxtSmartScale() at drawScale
 * afterward and resets the internal run-mode to "neither move nor draw
 * open" - the next chained operation always pays a fresh start, never a
 * stale continue. */
void gamelibRepositionAbs(int32_t physY, int32_t physX);

/* Chains a (dy, dx) delta of any magnitude across int8-safe +-100*drawScale
 * steps, as either a draw (wantDraw != 0) or a move (wantDraw == 0) run.
 * Continues an existing run of the same kind across MULTIPLE calls (not
 * just within one), via the internal run-mode tracker, so a whole
 * multi-segment path stays one cheap SM_continue_d chain instead of paying
 * a fresh dispatch at every segment boundary - switches to a fresh start
 * automatically when wantDraw's mode differs from whatever was open. */
void gamelibChainDelta(int32_t dy, int32_t dx, int wantDraw);

/* Draws ONE long line in a single vxtSmartDrawBig() record (no chaining) -
 * for a line short enough that +-100*bigScale reaches its full length in
 * one record, avoiding the visible mid-line joint multi-record chaining
 * leaves (see vxt_smart.asm's SM_startDrawBig_d comment). Sets `intensity`,
 * repositions to (absY, absX) via gamelibRepositionAbs(), draws (dy, dx) at
 * `bigScale`, then restores vxtSmartScale() to the drawScale from
 * gamelibBeamBegin() and marks the run-mode as an open draw (so the NEXT
 * gamelibRepositionAbs() emits its closing move correctly). */
void gamelibDrawBigLine(int32_t absY, int32_t absX, int32_t dy, int32_t dx,
                        uint8_t intensity, uint8_t bigScale);

/* For edges long/exposed enough that gamelibDrawBigLine()'s fixed
 * BIG_DRAW_SCALE (64, UNVERIFIED nop pairing on hardware - see
 * vxt_smart.asm) is either untrustworthy or forces many more chained steps
 * than the hardware requires. Dispatches through vxtSmartDrawHuge() (genuine
 * Timer 1 poll, correct at any scale) and picks the scale itself - the
 * smallest value that reaches (dy, dx) in ONE record, capped at 255
 * (VIA_t1_cnt_lo's hardware width - the real ceiling on any single record's
 * reach, ~25,500 phys units at this project's +-100 chaining margin). No
 * `bigScale` parameter: unlike gamelibDrawBigLine(), the whole point is
 * picking the scale that minimizes chained steps for THIS specific
 * distance, not reusing one fixed value for every caller.
 *
 * Costs ~2.2x a normal SmartList dispatch per record (a genuine hardware
 * wait's own measured premium) - use only where that exposure is real (a
 * model's longest edges), not as a blanket replacement for
 * gamelibDrawBigLine(). */
void gamelibDrawHugeLine(int32_t absY, int32_t absX, int32_t dy, int32_t dx,
                        uint8_t intensity);

/* Picks the cheapest CORRECT technique for a single independent line,
 * escalating only as far as needed:
 *
 *   1. If (dy, dx) fits in ONE record at `preferredScale` (the caller's
 *      normal BigScale constant) - use gamelibDrawBigLine() at that scale.
 *      Cheapest: no Timer-1 poll overhead, single fixed-nop-padded
 *      dispatch. NOTE: this still carries whatever timing-verification
 *      status `preferredScale` itself has - e.g. a scale of 64's pairing is
 *      UNVERIFIED on hardware - this function does not change that, it
 *      only avoids making it WORSE by never letting gamelibDrawBigLine()
 *      chain multiple records at that scale (which would exercise
 *      the unverified timing repeatedly instead of once).
 *   2. Otherwise - escalate straight to gamelibDrawHugeLine(), which picks
 *      its own 8-bit-or-16-bit scale internally and uses a genuine Timer-1
 *      poll, correct at any scale with no calibration. Costs ~2.2x a
 *      normal dispatch per record, but only paid on the rare edges that
 *      need more reach than `preferredScale` gives in one record.
 *
 * Deliberately scoped to "one independent line" (matches
 * gamelibDrawBigLine()/gamelibDrawHugeLine()'s own fresh-reposition
 * shape) - not a replacement for gamelibChainDelta(), which is a
 * different, lower-level primitive for a CONTINUING multi-segment path
 * sharing one beam run, not comparable to "draw this one line." */
/* Pass as `preferredScale`/`bigScale` to any of the dispatch/chain
 * functions here to request the ACCURATE single-record path instead of the
 * cheap fixed-scale one.
 *
 * Still exactly ONE record per line - this is not "chain more segments."
 * A single record draws (dy,dx) as (sy*s, sx*s) with |sy|,|sx| <= 100,
 * which constrains the scale `s` only from below; the accurate path
 * SEARCHES the legal scales for the one whose rounding lands closest to
 * the true endpoint, spending STM32 cycles (free) rather than 6809 records
 * (precious). The cheap path can't do this because SM_startDrawBig_d's NOP
 * padding is hand-computed for scale 64 exactly, pinning its scale.
 *
 * Cost vs the cheap path: a genuine Timer-1 poll on the 6809, ~2.2x one
 * dispatch, same record count. Use only where a vertex must coincide with
 * an independently-drawn element. Self-contained geometry should pass a
 * real scale. */
#define GAMELIB_SCALE_ACCURATE  0

/* Draw scale ladder.
 *
 * gb_dispatch_auto() picks the smallest nop-calibrated draw routine that
 * still reaches the line, instead of paying scale 64 for everything:
 *
 *   scale 32  reaches 3200 units,  67 cyc   SM_startDraw32_d  (12 nops)
 *   scale 64  reaches 6400 units,  97 cyc   SM_startDrawBig_d (27 nops)
 *
 * SM_startDrawBig_d spends 52 of its 97 cycles in NOPs waiting out the
 * scale-64 beam ramp, and Timer 1 counts down from the SCALE rather than
 * the line's length - so a 2000-unit edge waits exactly as long as a
 * 6400-unit one. Every edge in 1200..3200 therefore drops 30 cycles at the
 * same single record. Strict improvement on the original all-scale-64
 * behavior: the ladder never picks a coarser routine, only a finer one
 * where that still reaches.
 *
 * Why scale 12 is not on the ladder, learned on hardware.
 * It is cheaper still (45 cyc) and was tried first, giving a large speed
 * win with no timing blackouts - but it is the one draw scale with no
 * divisor relationship to the reposition scale (32). Repositions land on
 * multiples of 32; scale-64 and scale-32 draws land on that same grid
 * (64 = 2*32), while multiples of 12 only coincide with it every 96 units.
 * Vertices reached by a short edge could therefore miss the same vertex
 * reached by a reposition by ~22 units, which showed on hardware as
 * adjoining mesh/hull lines drawing as two separate dots, and as a
 * meter/gauge line reading incomplete due to reduced dwell brightness.
 * Scale 12 also dwells 5.3x less than scale 64, so its lines are markedly
 * dimmer, which is exactly that second symptom.
 *
 * Honest limit: the grid argument explains a chained edge meeting an
 * INDEPENDENTLY repositioned one. It does not by itself explain a failure
 * between two edges inside one tracked chain, and that gap has not been
 * closed - the ladder avoids the whole question by staying grid-safe rather
 * than by having proven the mechanism.
 *
 * TUNING. GAMELIB_LADDER_SCALE_LO is the fast tier. 32 is the smallest
 * grid-safe value; setting it to 64 disables the ladder entirely and
 * restores the original all-scale-64 behavior (the one-constant rollback).
 * Per-renderer opt-out is gamelibBeamSetFastDraw(0). */
#define GAMELIB_LADDER_SCALE_LO   32

void gamelibDrawAutoLine(int32_t absY, int32_t absX, int32_t dy, int32_t dx,
                        uint8_t intensity, uint8_t preferredScale);

/* The same escalation decision as gamelibDrawAutoLine()/
 * gamelibChainBigGrouped(), exposed directly for callers that hand-manage
 * their own position tracking instead of using gamelibChainBigGrouped()'s
 * groupSize/segCounter shape.
 * Assumes the beam is ALREADY at the correct position and intensity is
 * already set - does not reposition, does not touch intensity. Use this
 * instead of a raw vxtSmartScale()/vxtSmartDrawBig() pair anywhere in the
 * codebase, so every draw call goes through the same one escalation
 * decision.
 *
 * The caller must add outDy/outDx (not its own preferredScale-based guess)
 * to its tracked position - the scale used internally can differ
 * from preferredScale (escalates to Huge/Huge16 on a long segment), and
 * assuming preferredScale diverges the tracked position from the
 * real landed one, compounding every subsequent chained segment. This is a
 * genuine, measured drift source, not merely a data/geometry issue. */
void gamelibDispatchAutoDraw(int32_t dy, int32_t dx, uint8_t preferredScale,
                             int32_t *outDy, int32_t *outDx);

/* For callers that emit their own raw vxtSmartScale()/vxtSmartDrawBig()
 * chain directly (skipping gamelibDrawBigLine()'s per-call reposition, e.g.
 * to chain several corners from ONE reposition) and need the internal
 * run-mode tracker to reflect that an open draw run is now in effect, so
 * the next gamelibRepositionAbs() still emits its closing move correctly.
 * Does not touch hardware itself - bookkeeping only. */
void gamelibBeamMarkOpenDraw(void);

/* A middle ground between two extremes hardware-tested here: ONE
 * continuous chain from a single recenter (cheap, but analog integrator
 * drift accumulates unbounded over a long chain) and a fresh
 * gamelibRepositionAbs()/recenter before every segment (safe, but ~9-10
 * records per segment instead of ~2).
 *
 * Chains BIG-scale segments (same technique as gamelibDrawBigLine(), same
 * "scale must already be loaded on the 6809 side" requirement) but forces
 * a fresh gamelibRepositionAbs() to the caller's own tracked actual
 * position every `groupSize` segments, instead of never or always. This
 * bounds drift to at most `groupSize` segments' worth instead of the
 * whole shape, while cutting recenters by roughly that same factor.
 *
 * Safe only for geometry whose segments only need to connect to their own
 * neighbors (a closed ring/perimeter) - not for anything that must land
 * on a separately-drawn shape's exact position (a connector meeting a
 * ring vertex) unless that OTHER shape is ALSO periodically re-grounded
 * this way; an ungrounded shape's own tracked "actual" position is a
 * digital prediction that does not reliably track real analog drift (a
 * real bug here targeted an ungrounded chain's predicted position, and
 * failed for exactly this reason). Once a shape uses this function, ITS
 * tracked position is safe for another shape to target, because it gets
 * re-grounded by a real recenter every `groupSize` segments.
 *
 * `groupSize` <= 0 means "never force a recenter" (equivalent to a single
 * long chain - only safe for short paths, matches the pre-fix behavior);
 * `groupSize` == 1 forces a recenter before every segment (equivalent to
 * gamelibDrawBigLine() per segment - the safe, proven, costly fallback).
 * Caller owns `*actualY`/`*actualX` (running tracked position, same
 * pattern as every hand-rolled chain in this file) and `*segCounter`
 * (starts at 0; this function manages it from there). */
void gamelibChainBigGrouped(int32_t dy, int32_t dx, uint8_t bigScale,
                            int32_t *actualY, int32_t *actualX,
                            int *segCounter, int groupSize);

/* ===== HELD-SCALE chained draws ============================================
 * Why these exist, measured not assumed: vxtSmartScale() is not change-gated
 * the way vxtSmartIntensity() is (checked in vxt_smart.c - it is a bare
 * sm_rec()), and gb_dispatch_auto() wraps every draw in
 * `scale(bigScale) ... scale(gb_draw_scale)`. So a chained edge that emits
 * one real draw record costs THREE, and two of the three exist only to set
 * a scale the very next edge immediately sets back. Simulating the exact
 * emitted record stream for a complex chained model put that waste at 54%
 * of the whole object's record count - the single largest line item,
 * larger than the recenters themselves.
 *
 * The fix: set the scale once for a run of draws, emit bare draw records
 * inside it, restore once at the end. NOTHING about the physical draw
 * changes - every edge is emitted at exactly the scale it was already
 * being emitted at. Only the redundant set-scale records go away.
 *
 * USAGE (both functions assume the beam is already positioned and the
 * intensity already set, exactly like gamelibDispatchAutoDraw()):
 *
 *     gamelibBeamSetDrawScale(BIG_DRAW_SCALE);   // repositions/jumps now
 *                                                // restore to 64, not 12
 *     vxtSmartScale(BIG_DRAW_SCALE);             // once, before the run
 *     ... gamelibChainBigHeld(...) per edge ...
 *     vxtSmartScale(MY_DRAW_SCALE);              // once, after the run
 *     gamelibBeamSetDrawScale(MY_DRAW_SCALE);
 *
 * The paired gamelibBeamSetDrawScale() calls are what keep this at ONE
 * record per edge: without them gamelibRepositionAbs()'s own trailing
 * restore would put the scale back to the caller's normal draw scale and
 * every following edge would have to re-set it. */

/* Changes the scale gamelibRepositionAbs()/gb_dispatch_*() restore to after
 * their own work (the value gamelibBeamBegin() set). Does not emit a record
 * itself - it only changes which value the NEXT restore writes. always
 * restore it to the caller's normal draw scale before returning, or an
 * unrelated later element inherits it. */
void gamelibBeamSetDrawScale(uint8_t drawScale);

/* One chained BIG-scale edge with NO surrounding scale records - the caller
 * has already put the scale register on `bigScale` and will restore it.
 * Escalates exactly like gamelibDispatchAutoDraw() when the delta is too
 * long for one record at `bigScale`; the escalation path emits its own scale
 * records and restores to gb_draw_scale, which is why the usage above sets
 * gb_draw_scale to `bigScale` for the duration - so an escalated edge leaves
 * the register where the next un-escalated edge needs it.
 * Draw-gain compensated and position-tracked identically to
 * gamelibChainBigGrouped(); `*actualY`/`*actualX` are advanced by the delta
 * achieved, never by a bigScale-based guess. */
void gamelibChainBigHeld(int32_t dy, int32_t dx, uint8_t bigScale,
                         int32_t *actualY, int32_t *actualX);

/* One chained edge on the ACCURATE (scale-search) path, with no trailing
 * restore - costs scale + draw = 2 records, and the caller restores the
 * scale once at the end of the run.
 *
 * Use this, not gamelibChainBigHeld(), for a chained shape whose vertices
 * something else lands on independently: the fixed BigScale path
 * quantises each landing to +-bigScale/2, which measured 48 units mean /
 * 96 worst against an independently-repositioned anchor, while the scale
 * search aimed at the QUANTISED target coordinate measures 4 units mean /
 * 37 worst (0.2px / 1.9px). Aim it at gamelibRoundToScale(ideal, posScale),
 * not the raw ideal, or the search has no reachable point to hit. */
void gamelibChainAccurateHeld(int32_t dy, int32_t dx,
                              int32_t *actualY, int32_t *actualX);

/* Draws a closed N-point perimeter as ONE continuous BIG_DRAW_SCALE chain
 * from a single reposition (same technique as
 * gamelibChainBigGrouped(..., groupSize=0) - never re-recenters mid-loop,
 * so safe only for a self-contained closed shape, not one another shape
 * must land on exactly - see that function's own comment for the full
 * drift-safety reasoning, which applies identically here).
 *
 * The closing segment (back to point 0) targets the real landed start
 * position (gamelibRoundToScale()'d), not the ideal py[0]/px[0] - residual
 * per-segment rounding drift between the reposition scale and the big-draw
 * scale otherwise accumulates around the loop and the closing segment
 * inherits all of it, a bug confirmed and fixed on hardware.
 *
 * Sets `intensity` before the first reposition (same ordering rule as
 * gamelibDrawBigLine()) and leaves the run-mode marked as an open draw
 * afterward, exactly like gamelibDrawBigLine()/gamelibRepositionAbs()
 * already do - no separate gamelibBeamMarkOpenDraw() call needed by the
 * caller. `py`/`px` must have `n` >= 2 points, already in final absolute
 * screen coordinates (any per-element offset pre-applied by the caller). */
void gamelibChainBigClosedPerimeter(const int32_t *py, const int32_t *px, int n,
                                    uint8_t bigScale, uint8_t intensity);

/* The "bright dot" fix, generalized. Root cause (hardware-confirmed): the
 * beam's physical position doesn't change until the NEXT move/draw record
 * executes, so if a render function's last operation is a draw
 * and whatever runs right after it opens with an expensive multi-record
 * setup (a fresh gamelibRepositionAbs(), typically), the beam sits LIT at
 * that last drawn point for the entire duration of that setup - a longer
 * dwell than anywhere else in the frame, hence visibly brighter than it
 * should be.
 *
 * Call this at the end of any render function whose last operation left
 * an open draw run (i.e. didn't already end on a move/blank) and that
 * ISN'T guaranteed to be immediately followed by another draw at the same
 * point - closes the run into blanked/move mode immediately, so whatever
 * dwell follows happens unlit instead of lit. Cheap (one blanked move) and
 * idempotent - safe to call even if the run was already closed. */
void gamelibBeamCloseRun(void);

/* ===========================================================================
 * Distributed closure correction
 *
 * For a CHAINED figure that must land back on a point it already visited. A
 * hardware test rig measures how far such a figure misses by; this
 * spreads the negation of that miss across the chain's segments so it lands
 * where it should.
 *
 * The offset goes on the target, never on the delta. This is the whole trap,
 * and it is not obvious: chained draws in this scheme are SELF-CORRECTING -
 * each segment computes its delta from the beam's TRACKED position to that
 * vertex's absolute target. Add a correction to a delta and the very next
 * segment re-aims from the corrected position and cancels it exactly. Adding
 * it to the TARGETS is what moves the landing point:
 *
 *     target[k] += (D * k) / n        k = 1..n
 *
 * After n segments the accumulated offset is exactly D, nulling the miss;
 * intermediate vertices shift proportionally, which reads as a smooth unwind
 * rather than a kink.
 *
 * Why not just change the draw gain: a uniform gain CANNOT change a closed
 * figure's closure at all. It scales every delta, and a closed figure's
 * deltas sum to zero, so g*0 = 0. Only an ASYMMETRIC correction - one that
 * treats the return path differently from the outbound one - has any
 * authority over where a closed chain lands. Measured directly on hardware:
 * re-fitting the draw gain alone did nothing for a large closed figure's
 * closure error.
 *
 * COSTS ZERO 6809 RECORDS - same records, same scales, just different
 * numbers inside them.
 *
 * `k` is 1-based; k==n returns exactly (dy,dx) with no rounding drift, which
 * matters because that is the segment that has to land. */
void gamelibChainCloseOffset(int32_t dy, int32_t dx, int k, int n,
                             int32_t *outY, int32_t *outX);

#endif
