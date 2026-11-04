/*
 * vxt_cal_load.h - VXT toolkit: reads the calibration rig's own
 * /calmeas.csv log (see vxt_cal.c) and derives the TEXT H per-character
 * skew correction from it, for any GAME (not just the rig itself) to
 * apply at its own boot. GPLv3.
 *
 * Why this exists. A calibration value measured on one physical unit is
 * specific to that unit's own analog drift - hardcoding a fitted
 * constant into a game's firmware bakes in one machine's number
 * permanently, which is wrong the moment that cart's SD card (or the
 * cart itself) moves to different hardware. This module instead
 * re-derives the correction from whatever /calmeas.csv is on
 * THIS SD card at THIS boot - the same file the calibration rig
 * (code/app6809/test8_cal, vxt_cal.c) already writes. No CSV format is
 * duplicated by invention here - just re-read with the same field order.
 */
#ifndef VXT_CAL_LOAD_H
#define VXT_CAL_LOAD_H

#include <stdint.h>

/* Reads /calmeas.csv (if present) and averages every card-off text H
 * end-of-row reading into a single (cross, along) rate-unit pair, same
 * formula and same card-off-only reasoning as the rig's own live
 * converge loop (vxt_cal.c's calMeasUpsert()/RECORD fold-in - card-ON
 * readings are excluded because the reference card overlay itself has
 * been observed to change where text lands, and real gameplay text never
 * draws with that overlay on screen).
 *
 * Returns 1 and fills cross and along if at least one usable card-off
 * TEXT H end reference was found; returns 0 and leaves both UNTOUCHED
 * if the file is missing, empty, or has no usable TEXT H data - callers
 * must default to identity (0,0) themselves in that case (this function
 * deliberately does not choose a fallback value for the caller - "no
 * data" and "measured zero" are different things, and * defaulting to some OTHER machine's old fitted constant would be
 * exactly the bug this module exists to avoid). */
int vxtCalLoadTextComp(int8_t *cross, int8_t *along);

/* ---------------------------------------------------------------------------
 * PHASE B - the two Class-I drawing corrections, read from this
 * machine's own /calmeas.csv instead of being compiled in.
 *
 * These exist because of a real architectural defect: the calibration system's
 * stated purpose is "a runtime, PER-UNIT calibration system... values
 * persisted to the board, loaded at boot, and applied inside the shared
 * drawing primitives" - and until now only TEXT H did that. The two geometry
 * corrections were fitted on ONE Vectrex and hardcoded, so any other unit got
 * this machine's analog behaviour baked in.
 *
 * Both follow vxtCalLoadTextComp()'s contract exactly: return 1 and fill the
 * out-param if at least one usable row was found; return 0 and leave it
 * UNTOUCHED otherwise, so the caller supplies its own fallback. "No data" and
 * "measured zero" must stay distinguishable - defaulting to another
 * machine's fitted constant is precisely the bug this module exists to avoid.
 *
 * Both require provenance. Only rows whose recorded drawgain is 1000 (identity,
 * which the rig forces every frame) are used. A row measured THROUGH a
 * correction is not a raw measurement of the hardware, and a row from before
 * those columns existed cannot say either way - that exact ambiguity already
 * cost one whole fitting attempt, so legacy rows are skipped, not trusted.
 * ------------------------------------------------------------------------ */

/* Draw gain, from the ANGLE screen. Averages each spoke's RADIAL error as a
 * fraction of its radius, then returns the gain that cancels it, in
 * thousandths (1000 = identity, 1060 = "draw 6% further").
 *
 * Prefers CARD-ON rows when enough exist: the reference card absorbs the
 * first-element displacement (~800-960 units without it, 45 with), so a
 * card-on sweep isolates the draw term alone - half the scatter of any
 * card-off fit, and the reason earlier attempts disagreed. Falls back to
 * card-off rows EXCLUDING spoke 0, the one carrying that displacement. */
int vxtCalLoadDrawGain(int16_t *per1000);

/* Closure correction, from the CHORD screen, which draws one LONG chord plus
 * an arc back onto its own start. The miss is A = (ref 1) - (ref 0), expressed
 * as a fraction of the chord's length - the form that holds across radii.
 * Returns thousandths (35 = "3.5% of the closing sub-path's longest segment").
 *
 * Needs both refs of a variant to form a difference, so it pairs them and
 * skips any variant missing either half. */
int vxtCalLoadClosureComp(int16_t *per1000);

/* ---------------------------------------------------------------------------
 * Centre origin offset - the third Class-I geometry correction,
 * read the same way as the two above. See gamelibBeamSetOffset() for what
 * it corrects and why (a physical Vectrex's true screen centre need not sit
 * exactly at the DAC's own zero deflection).
 *
 * CENTRE's single reference sits at nominal (0,0) by construction, so a
 * row's own (dy,dx) directly IS the measured offset - no per-length or
 * per-radius division like TEXT H/CHORD need. Same provenance rule as the
 * other two: only rows recorded at identity draw gain (1000) are used.
 *
 * Returns 1 and fills offY/offX if at least one usable CENTRE row was
 * found; returns 0 and leaves both UNTOUCHED otherwise - same "no data"
 * vs "measured zero" contract as every loader in this file. */
int vxtCalLoadOffset(int16_t *offY, int16_t *offX);

/* ---------------------------------------------------------------------------
 * Hop drift - The sideways (and vertical) error a chained
 * column of short ticks picks up per blanked hop, from the rig's HOP screen.
 *
 * The figure is a typical HUD meter, drawn exactly as a game draws one: ONE reposition, then for each further tick a pure-vertical
 * MoveBig hop at scale 32 and a serpentine 800-unit tick (left->right, then
 * right->left, ...). Hardware showed that column walking sideways as ticks
 * were added (Scene 2's 15 ticks drifted LEFT), and no other screen measures
 * it: REPOS starts each move from a recenter, ACCUM chains draws, not hops.
 *
 * The geometry lives HERE, not in either .c file, so the rig that measures it
 * and the loader that interprets its rows cannot disagree; a game that draws the same meter
 * should _Static_assert its constants against these. One variant per
 * tick count the game draws -
 * because the hop LENGTH (height / ticks) differs per count and nothing yet
 * says the error is independent of it.
 *
 * Measured as ACCUM is: ref 0 = LEFT end of the first tick, ref 1 = LEFT end
 * of the last. A whole-figure displacement moves both equally and cancels, so
 *     drift per hop = ((ref 1) - (ref 0)) / (ticks - 1)
 * The left end is the same tick-pair phase for every count (an even number of
 * serpentine ticks always precedes it), so a tick's own length error cancels
 * pairwise and only the hop term survives. */
#define VXT_CAL_HOP_NVAR     4
#define VXT_CAL_HOP_TICKS    { 4, 6, 8, 15 }
#define VXT_CAL_HOP_BAR_X    9752L      /* meter x position */
#define VXT_CAL_HOP_BASE_Y   (-15200L)  /* meter base y     */
#define VXT_CAL_HOP_HEIGHT   3500L      /* meter height     */
#define VXT_CAL_HOP_HALF_W   400L       /* tick half-width  */

/* Reads `csvPath` (pass VXT_CAL_RIG_FILE: HOP rows are raw samples only the
 * standalone rig writes, so this ignores vxtCalLoadSetSource()'s selection on
 * purpose) and fills, per variant, the RAW error per hop - the beam lands this
 * far from where it was sent, each hop; a caller cancels it by aiming the
 * other way. Card-OFF rows only (the game never draws the card), drawgain
 * 1000 only, and both refs of a variant are required.
 *
 * have[v] is set 1 for each variant found, 0 otherwise; dy/dx[v] are written
 * only where have[v] is 1. Returns the number of variants found (0 = file
 * missing or no HOP rows - "no data", which callers must keep distinct from
 * a measured zero). Blocking SD I/O: one-shot init only (convention 19). */
int vxtCalLoadHopDrift(const char *csvPath, int16_t dyPerHop[VXT_CAL_HOP_NVAR],
                       int16_t dxPerHop[VXT_CAL_HOP_NVAR],
                       uint8_t have[VXT_CAL_HOP_NVAR]);

/* ---------------------------------------------------------------------------
 * Write side - added for a game's own in-game Cal screen, which
 * needs to RECORD a measurement, not just load one at boot. Lives here
 * rather than being duplicated in the calling game's own code because
 * this module already owns the calmeas.csv column format and its parser
 * (vxtCalLoadParseField()) - a second, game-local parser would just be
 * the same format re-invented, which this whole module exists to avoid.
 * Not the rig's own vxt_cal.c (explicitly not reused).
 *
 * UPSERT, not append: streams the existing /calmeas.csv (if any) to a temp
 * file, replacing the ONE row whose (screen,variant,ref) key matches with
 * a freshly built one and passing every other line through untouched
 * (including a full standalone-rig log's data for screens this call never
 * touches), then swaps the temp file over the original. NO in-RAM table of
 * prior rows is held at any point - the design doc's own "no RAM table"
 * decision, satisfied by streaming rather than buffering. Matches (and is
 * interoperable with) the rig's own calBuildLogLine() column order exactly:
 *   screen,variant,ref,card,refY,refX,dy,dx,records,compy,compx,nrefs,
 *   drawgain,movegain,movesettle,session,fw
 *
 * `card` is always written 0 (a game's own Cal screen tiles never draw a
 * reference-card overlay - the game already primes every frame, see
 * gamelibBeamPrime(), which is what the rig's own card-on workaround for
 * ANGLE existed to substitute for). `records`/`session`/`fw` are written 0
 * - a game has no per-reading SmartList count, boot-session counter, or
 * firmware-stamp macro of its own; none of the three feeds any loader's
 * provenance check, which looks at `drawgain` alone. `movegain`/
 * `movesettle` are written at identity (1000/0) - the Cal screen never
 * exercises either path. Caller is responsible for `drawGain` * reflecting what was active for THIS reading (1000 for a raw measurement,
 * matching what vxtCalLoadDrawGain()/vxtCalLoadClosureComp() require to
 * accept the row at all).
 *
 * Returns 1 on success, 0 if the SD card couldn't be written (caller should
 * treat this the same as any other blocked save - see the rig's own
 * calSaveLog() for the equivalent contract). Blocking SD I/O - safe to call
 * only from a one-shot button-press handler, never per-frame - the rig's
 * own "flashed through black" incident is exactly what happens if this
 * rule is broken. */
int vxtCalSaveRow(const char *screen, int32_t variant, int32_t ref,
                  int32_t refY, int32_t refX, int32_t dy, int32_t dx,
                  int32_t compY, int32_t compX, int32_t drawGain);

/* ---------------------------------------------------------------------------
 * Which file, and whose geometry - call once from a one-shot boot-init
 * handler, before any loader or vxtCalSaveRow() call.
 *
 * Two different tools write measurement rows, and they are not
 * interchangeable even though the schema is identical:
 *
 *   - The standalone rig (code/app6809/test8_cal) writes RAW samples: a
 *     12-spoke ANGLE sweep, several CHORD variants, meant to be
 *     median-fitted across rows.
 *   - A game's own Cal screen writes ONE row per correction, reverse-derived
 *     so a loader's own formula reconstructs the value the player converged
 *     on. One row is correct there by design; it is an encoding, not a
 *     sample.
 *
 * Sharing one file lets those collide on identical (screen,variant,ref)
 * keys - a game's synthetic ANGLE row upserts over one of the rig's real
 * measured spokes - so each tool gets its own file.
 *
 * `chordPadR` is the radial offset between a CHORD row's own refX and the
 * arc radius it was measured against (|refX| = R + chordPadR). The two
 * tools do not agree on it: the rig pads by 2165, a game may pad by
 * something else entirely, and assuming the wrong one rescales every
 * reconstructed closure value by the ratio of the two radii. It therefore
 * travels with the file selection rather than being a single hardcoded
 * constant.
 *
 * The STM32 is not reset when the 6809 changes carts, so this is sticky
 * state a previous cart can leave behind: every consumer must set its own
 * source explicitly at boot rather than relying on the default below.
 * Passing NULL for either path restores the rig's own default. */
void vxtCalLoadSetSource(const char *csvPath, const char *tmpPath,
                         int32_t chordPadR);

/* The rig's own values, for a caller that wants to name them explicitly
 * rather than hardcode them again. */
#define VXT_CAL_RIG_FILE        "/calmeas.csv"
#define VXT_CAL_RIG_TMP         "/calmeas.tmp"
#define VXT_CAL_RIG_CHORD_PAD   2165

/* The rig's TEXT H measurement-method version, written into every rig row's
 * trailing `method` column. Bump it whenever the TEXT H strings change: the
 * rig drops older rows at load (so the screen asks for fresh readings) and
 * vxtCalLoadTextComp() ignores them. 0 = the original all-'8' strings;
 * 1 = an interim string set, superseded;
 * 2 = the three strings in vxt_cal.c's CAL_TEXT_STRS[]. */
#define VXT_CAL_TEXTH_METHOD    2

#endif /* VXT_CAL_LOAD_H */
