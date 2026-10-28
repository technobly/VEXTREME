/*
 * vxt_cal.c - VXT toolkit: CALIBRATION / measurement rig (STM32 side).
 * See vxt_cal.h for what this is and why it exists as a standalone cart.
 *
 * Measurement only: no adjustable calibration values, no persistence, no
 * settings UI.
 *
 * The design in one paragraph. There is exactly ONE measurement primitive
 * here - a landing test: draw a figure by some draw path, then draw an
 * independently repositioned reference mark at that figure's ideal endpoint.
 * The visible gap between them is the error. Every screen below is that same
 * primitive swept over a different axis (length, scale, record count,
 * direction, screen deflection) or applied to a different shape archetype.
 * Each archetype is a real defect pattern, stripped of any game-specific
 * geometry - generic in form, defect-derived in content, so the rig serves
 * any game on this toolkit without measuring things that never bit us.
 *
 * Nothing in this file may cost 6809 records to buy precision. The rig
 * MEASURES the draw paths; it must never quietly improve them, or it
 * would measure itself instead of the hardware.
 */

#include <math.h>
#include <stdint.h>   /* flash.h below uses uintN_t without including it */

#include "../fatfs/ff.h"
#include "../flash.h"
#include "vxt_cal.h"
#include "vxt_cal_load.h"
#include "vxt_smart.h"
#include "vxt_smart_text.h"
#include "vxt_input.h"
#include "vxt_bounds.h"
#include "../gamelib/gamelib_beam.h"

/* ---------------------------------------------------------------------------
 * Region + scales. Identical to a real game's so the rig measures the same
 * numbers the game draws with - a rig calibrated at different
 * scales would be measuring a configuration no game ships.
 * ------------------------------------------------------------------------ */
#define CAL_REGION_OFFSET   VXT_SMART_OFFSET
#define CAL_REGION_RECORDS  ((0x2000 - VXT_SMART_OFFSET) / 4)   /* 1536 */

#define CAL_POS_SCALE       0x20    /* 32 - matches the reposition grid     */
#define CAL_DRAW_SCALE      0x0C    /* 12 - the default draw scale          */
#define CAL_BIG_SCALE       0x40    /* 64 - the UNVERIFIED nop pairing that
                                     * the SCALE screen exists to measure.
                                     * Do not "fix" this constant
                                     * here; measuring it is the point. */

/* Intensities, chosen so the three roles are separable by eye at a glance -
 * which matters more here than it does in a game, because the whole method is
 * "tell these two marks apart and judge the gap between them". */
#define CAL_INT_GRID        30      /* the reference card / background      */
#define CAL_INT_FIGURE      70      /* the thing under test                 */
#define CAL_INT_REF         95      /* independently-drawn reference marks  */
#define CAL_INT_CARET       120     /* the user's measuring caret           */
#define CAL_INT_TEXT        60

/* Reference card extents. Deliberately INSIDE VXT_BOUNDS_HALF_* so the card
 * is fully visible on a unit whose deflection gain is low - a card clipped by
 * overscan cannot be used to judge overscan. */
#define CAL_CARD_HALF_X     10000L
#define CAL_CARD_HALF_Y     10000L
#define CAL_BRACKET_LEN     1200L
#define CAL_CROSS_ARM       900L
#define CAL_TICK_LEN        500L
#define CAL_CARET_ARM       400L

/* ---------------------------------------------------------------------------
 * Screens
 * ------------------------------------------------------------------------ */
enum {
    CAL_SCR_CENTRE = 0,   /* the true-centre / geometry test card           */
    CAL_SCR_LADDER,       /* length sweep - proportional vs fixed error     */
    CAL_SCR_SCALE,        /* scale-band probe - the four draw paths         */
    CAL_SCR_CHAIN,        /* chain accumulation - closed N-gon closure      */
    CAL_SCR_SHARED,       /* archetype B - two shapes sharing a vertex      */
    CAL_SCR_ANGLE,        /* direction sweep                                */
    CAL_SCR_DEFLECT,      /* absolute-deflection sweep                      */
    CAL_SCR_TEXTH,        /* long strings, horizontal                       */
    CAL_SCR_TEXTV,        /* long strings, console rotated onto its side    */
    CAL_SCR_PRIME,        /* how many priming cycles kill the first-element
                           * anomaly? variant = cycle count                 */
    CAL_SCR_REPOS,        /* reposition distance sweep across RECORD
                           * boundaries - distance vs record-count          */
    CAL_SCR_ACCUM,        /* chain accumulation ISOLATED from whole-figure
                           * displacement - fixed-per-record vs
                           * length-proportional                            */
    CAL_SCR_CHORD,        /* A closed-arc topology: one LONG chord, then a
                           * multi-segment arc back onto its own start      */
    CAL_SCR_HOP,          /*
                           * - drift per chained blanked hop         */
    CAL_SCR_COUNT
};

static const char *const CAL_SCREEN_NAME[CAL_SCR_COUNT] = {
    /* Every name below is spelled within vxt_smart_text.c's actual glyph set
     * (digits, N U M B E R O F L I S T G H A D V C X P Y, '=', '-', space).
     * There is no J K Q W Z, no '+' and no '.'. Checked by hand; if you add a
     * screen, check its name too - a missing glyph draws nothing. */
    "CENTRE", "LADDER", "SCALE", "CHAIN", "SHARED", "ANGLE", "DEFLECT",
    "TEXT H", "TEXT V", "PRIME", "REPOS",
    /* "ACCUM" - A, C, U, M are all in the glyph set above; checked by hand
     * per this comment's own instruction. "CHORD" - C, H, O, R, D likewise. */
    "ACCUM", "CHORD",
    /* "HOP" - H, O, P all in the glyph set above; checked by hand. */
    "HOP"
};

/* GEOMETRY variant count per screen. The variant button cycles through twice
 * this many: the second pass repeats every geometry variant with the reference
 * card ON.
 *
 * WHY: a SKIP control was needed, and all four buttons were
 * taken. Folding the card toggle into the variant sequence frees one, and it
 * is better than the toggle it replaces - the card is an experimental variable
 * (it was observed to change where figures land), so pairing every geometry
 * variant with a card-on twin makes the A/B comparison systematic instead of
 * something you have to remember to do. V0 and its twin differ in exactly one
 * thing, and both are logged. */
static const uint8_t CAL_VARIANTS[CAL_SCR_COUNT] = { 2, 3, 4, 7, 3, 3, 3, 5, 5, 5, 11, 7, 7, VXT_CAL_HOP_NVAR };
#define CAL_VARIANT_TOTAL(s)  (CAL_VARIANTS[s] * 2)
#define CAL_GEOM_VARIANT(s,v) ((uint8_t)((v) % CAL_VARIANTS[s]))
#define CAL_CARD_ON(s,v)      ((v) >= CAL_VARIANTS[s])

/* ---------------------------------------------------------------------------
 * State. Reset by vxt_cal_init_handler() on every 6809 boot - see vxt_cal.h
 * for why that RPC exists at all.
 * ------------------------------------------------------------------------ */
static int      cal_screen  = CAL_SCR_CENTRE;
static uint8_t  cal_variant[CAL_SCR_COUNT];
static int      cal_overlay = 0;       /* derived from the variant, see above */
static int32_t  cal_caret_y, cal_caret_x;
static int      cal_fig_records;              /* cost of the figure alone   */

/* An old analog joystick routinely rests a few units off true center - well
 * inside CAL_CARET_DEADZONE's own 8-unit margin being exceeded is common,
 * not a defect in any one unit. calCaretStep()'s fine-control floor (any
 * reading past the deadzone moves the caret by at least 1 unit/frame) then
 * turns that rest offset into a constant one-directional creep with the
 * stick untouched. Sampled once, on this rig's first frame - not in
 * vxt_cal_init_handler(), which fires before the 6809 has ever called
 * VXT_INPUT_READ, so parm's joystick bytes are not yet real readings
 * there. */
static int8_t   cal_joy_y_center, cal_joy_x_center;
static int      cal_joy_centered;

/* The item cursor - which reference point is currently being measured.
 *
 * Added. This replaces "whichever reference is nearest the caret".
 * Nearest-reference was forgiving but it could not support a SKIP, because
 * there was no notion of a CURRENT item to skip past - and without an explicit
 * cursor there is no way to tell "I looked at this one and could not read it"
 * from "I never reached it". An explicit cursor turns a screen into a walk
 * through its items in a fixed order, which is also what makes two sessions'
 * data comparable. */
static int      cal_sel;

/* Reference points - every mark a screen draws registers itself here, and
 * the readout reports the caret's offset from the NEAREST one.
 *
 * Redesigned after the first real measurement session. The first
 * version tracked ONE target per screen (the last/longest element), so the
 * moment the user walked the caret to a different rung, vertex or spoke -
 * which is exactly how you use a ladder or a starburst - DY/DX * included the geometric offset BETWEEN elements as well as the error. The
 * readings were still recoverable (subtract the nominal difference by hand,
 * which is what had to be done for the first session's photos), but a
 * measuring instrument that needs mental arithmetic to interpret is a
 * measuring instrument that will eventually be misread. Nearest-reference
 * makes every reading direct: park the caret on the point you care about and
 * the number is the error at THAT point. */
#define CAL_MAX_REFS  40
static int32_t cal_ref_y[CAL_MAX_REFS], cal_ref_x[CAL_MAX_REFS];
static int     cal_ref_n;
/* Index at which the CURRENT screen's own references begin - nonzero only
 * when the reference card is on, since the card registers its centre cross
 * first. See calApplyGeomComp(). */
static int     cal_ref_base;

/* ---------------------------------------------------------------------------
 * Measurement log (button 4).
 *
 * Stored as a CSV on the SD card (`/calmeas.csv`) rather than in
 * `SettingsRecord`:
 *   - SettingsRecord is a fixed 1024 bytes and is SHARED with the multicart
 *     menu, which reads and writes the whole block. A growing measurement log
 *     does not belong in a structure another program owns.
 *   - A log is append-only and can run to dozens of rows; settings are a
 *     small fixed set of current values. Different lifetimes, different files.
 *   - A CSV is readable the moment the card is pulled, which is the entire
 *     point - these numbers exist to be analyzed off-device.
 * The persistence MECHANISM is the same one settings.c already uses (FatFs on
 * the SD card, mounted at boot in main.c). Note it is an SD card, not EEPROM -
 * the STM32F411 has none.
 *
 * This is still Phase 0: recording a measurement changes NOTHING about how
 * anything draws. The log is evidence, not calibration.
 * ------------------------------------------------------------------------ */
/* History of the storage, kept because each step was a hardware-caught
 * data-loss bug and the reasoning is what justifies the current design:
 *   - 64-entry APPEND-only array: RECORD's gate was `cal_meas_n < 64`, a HARD
 *     STOP that made RECORD do nothing once full - indistinguishable
 *     from "the correction doesn't work".
 *   - 64-entry RING: fixed the hard stop but EVICTED the oldest
 *     entry. Destroyed a full SHARED sweep mid-session, including the ring's
 *     816-unit reading the whole investigation turned on.
 *   - 384 then 448 then 512-entry ring, "sized to the whole measurement
 *     space": still a hand-maintained number with no link to the screens it
 *     sized for, and it was mis-derived at least once (REPOS was recorded as
 *     costing 14 entries; it costs 22).
 * Replaced by the dense slot table below, which removes the
 * capacity question entirely rather than re-answering it.
 * ------------------------------------------------------------------------ */
/* ===========================================================================
 * Dense fixed slot table. Replaced the ring buffer,
 * instruction ("it should be a fixed defined array for all possible measured
 * items... it will also tell us which measures are blank and can be
 * supplemented").
 *
 * WHY THE RING HAD TO GO. cal_meas[] was a ring of 512 rows keyed by
 * (screen,variant,ref) with a linear search. Three problems, all real:
 *   1. It could still WRAP and evict, which had already destroyed a
 *      full SHARED sweep once. Every raise of CAL_MAX_MEAS was a guess at a
 *      number that had already been mis-derived once (an earlier note
 *      said REPOS cost 14 entries; it costs 22).
 *   2. It could not answer "what have I not measured yet?" - an unmeasured
 *      item and an absent row are the same thing in a sparse list.
 *   3. Its size was a hand-maintained constant with no link to the screens
 *      it was sizing for, so adding a screen ate the headroom.
 *
 * NOW: one slot per measurable item, always present, addressed arithmetically
 * rather than searched:
 *
 *     slot = CAL_SLOT_BASE[screen] + variant * CAL_REFS[screen] + ref
 *
 * where `variant` counts the card-ON twins too (0 .. CAL_VARIANT_TOTAL-1).
 * Recording the same item again overwrites its own slot by construction -
 * the upsert rule is now structural instead of a search that could miss.
 * `valid` distinguishes "never measured" from "measured as zero", so the
 * readout can report coverage and point at the gaps.
 *
 * Size is derived from the screen tables, not guessed - and checked. Adding a
 * screen changes CAL_VARIANTS[]/CAL_REFS[] and therefore the requirement;
 * calSlotInit() recomputes the bases every boot and sets cal_slots_over if the
 * total exceeds CAL_TOTAL_SLOTS, which the readout shows as "SLOTS!". It
 * cannot fail the way the ring's capacity did.
 * ======================================================================== */

/* References each screen registers. must match what the screen's own draw
 * function calRef()s - calSlotInit() cannot verify this at boot
 * (references are registered during the draw), but calMeasSlot() refuses any
 * ref >= CAL_REFS[screen], so an under-stated entry loses measurements
 * loudly rather than corrupting a neighbouring screen's slots. */
static const uint8_t CAL_REFS[CAL_SCR_COUNT] = {
/*  Centre ladder scale chain shared angle deflect texth textv prime repos accum chord hop */
        1,     5,    4,    1,     7,   12,      5,    6,    6,    4,    1,    2,    2,   2
};

/* Slots one screen needs: (geometry variants x 2 card twins) x refs. */
#define CAL_SLOTS(v,r)   ((v) * 2 * (r))
#define CAL_TOTAL_SLOTS  ( CAL_SLOTS(2,1)  + CAL_SLOTS(3,5)  + CAL_SLOTS(4,4)  \
                         + CAL_SLOTS(7,1)  + CAL_SLOTS(3,7)  + CAL_SLOTS(3,12) \
                         + CAL_SLOTS(3,5)  + CAL_SLOTS(5,6)  + CAL_SLOTS(5,6)  \
                         + CAL_SLOTS(5,4)  + CAL_SLOTS(11,1) + CAL_SLOTS(7,2)  \
                         + CAL_SLOTS(7,2)  + CAL_SLOTS(VXT_CAL_HOP_NVAR,2) )
/*                     =    4  +  30  +  32  +  14  +  42  +  72
 *                        + 30  +  60  +  60  +  40  +  22  +  28
 *                        + 28 (CHORD) + 16 (HOP)  =  478     */

/* Firmware stamp - the staleness key, and the direct answer to the defect
 * that made this whole refactor necessary.
 *
 * There is no RTC on this board (_FS_NORTC is 1 in ffconf.h; FatFS hands out a
 * fixed fake 2015 date), so a wall-clock timestamp is not available and a fake
 * one would be worse than none. What invalidates a measurement here
 * is not elapsed time but a firmware change - and that is exactly what this
 * captures, for free, at compile time.
 *
 * The concrete case that motivated this: the rig once inherited whatever
 * draw gain the game last set, so rows measured before a gain change were
 * ~5% off rows measured after it - and NOTHING in the file distinguished
 * them. That ambiguity made an attempt to validate a draw-gain constant
 * against measured rows inconclusive: the same rows support either
 * direction of error depending on which firmware took them, with no way
 * to tell. Never again: the gains ACTIVE at the moment of the reading are
 * now recorded alongside it too (drawGain/moveGain/moveSettle below),
 * which is the belt to this braces. */
/* The gains the rig FORCES every frame - named so the values recorded into a
 * row and the values applied can never drift apart. See the
 * vxtSmartBegin block in vxt_cal_handler() for why the rig must not inherit
 * whatever gain the game last set. */
#define CAL_RIG_DRAW_GAIN    1000
#define CAL_RIG_MOVE_GAIN    1000
#define CAL_RIG_MOVE_SETTLE  0

#define CAL_FW_STAMP ( ((uint32_t)(__DATE__[7]-'0')*1000u + (uint32_t)(__DATE__[8]-'0')*100u \
                      + (uint32_t)(__DATE__[9]-'0')*10u   + (uint32_t)(__DATE__[10]-'0')) * 10000u \
                     + (uint32_t)((__DATE__[4]==' '?0:__DATE__[4]-'0')*10 + (__DATE__[5]-'0')) * 100u \
                     + (uint32_t)((__TIME__[0]-'0')*10 + (__TIME__[1]-'0')) )

/* Field order is deliberate, not incidental: the uint32 goes FIRST so the
 * struct packs to 28 bytes instead of 32. Declared in the obvious order it
 * padded to 32 (4-byte alignment forcing a hole before fwStamp and another
 * after valid), costing 1736 bytes of bss across 434 slots for nothing -
 * bss comes straight out of the stack. */
typedef struct {
    uint32_t fwStamp;       /* CAL_FW_STAMP of the build that took it       */
    int16_t  dy, dx;        /* The measurement: caret - ideal ref            */
    int16_t  refY, refX;    /* the ideal target it was taken against - still
                             * stored (though the slot index implies the ref
                             * NUMBER) because it detects a screen whose
                             * reference LAYOUT moved, which renumbering alone
                             * cannot: see the earlier SHARED incident in
                             * calSeedGeomComp(). */
    /* The geometry correction ACTIVE on this reference when the reading was
     * taken. Needed correction is always (compY - dy, compX - dx), whether
     * the reading was raw (comp 0 -> -dy/-dx) or a residual with Corr ON.
     * Without it a raw reading and a residual are indistinguishable. */
    int16_t  compY, compX;
    /* Added - the gamelib gains ACTIVE at the moment of the
     * reading. Same self-describing principle as compY/compX one layer up,
     * and missed there: see CAL_FW_STAMP above for the measurement this
     * omission cost us. */
    int16_t  drawGain, moveGain, moveSettle;
    uint16_t n;             /* SmartList records the frame used             */
    uint16_t session;       /* monotonic, +1 per boot that records anything */
    uint8_t  valid;         /* 0 = never measured (distinct from measured 0) */
} CalMeas;

static CalMeas  cal_meas[CAL_TOTAL_SLOTS];
static uint16_t cal_slot_base[CAL_SCR_COUNT];
static int      cal_slots_used;     /* what the tables require      */
static int      cal_slots_over;     /* CAL_TOTAL_SLOTS too small - see above */
static uint16_t cal_session;        /* this boot's session id                */

/* Computes the per-screen slot bases from the screen tables. Called once from
 * vxt_cal_init_handler() BEFORE calLoadLog(). */
static void calSlotInit(void)
{
    int s, total = 0;
    for (s = 0; s < CAL_SCR_COUNT; s++) {
        cal_slot_base[s] = (uint16_t)total;
        total += CAL_VARIANT_TOTAL(s) * (int)CAL_REFS[s];
    }
    cal_slots_used = total;
    cal_slots_over = (total > CAL_TOTAL_SLOTS);
}

/* THE single addressing point. Returns the slot for one measurable item, or
 * NULL if the (screen,variant,ref) triple is outside what the tables declare -
 * which is a programming error (a screen registering more refs than CAL_REFS
 * says), never user input, so failing loudly beats clamping into a neighbour's
 * slot. Replaces calMeasUpsert()/calMeasPush(): the "same item overwrites
 * itself" rule is now the addressing itself. */
static CalMeas *calMeasSlot(int screen, int variant, int ref)
{
    int idx;
    if (screen < 0 || screen >= CAL_SCR_COUNT) return 0;
    if (variant < 0 || variant >= CAL_VARIANT_TOTAL(screen)) return 0;
    if (ref < 0 || ref >= (int)CAL_REFS[screen]) return 0;
    idx = (int)cal_slot_base[screen] + variant * (int)CAL_REFS[screen] + ref;
    if (idx < 0 || idx >= CAL_TOTAL_SLOTS) return 0;
    return &cal_meas[idx];
}

/* How many of a screen's slots are filled - drives the readout's coverage
 * indicator, i.e. "which measures are blank and can be supplemented". */
static int calScreenFilled(int screen, int *outTotal)
{
    int v, r, filled = 0, tot = 0;
    for (v = 0; v < CAL_VARIANT_TOTAL(screen); v++) {
        for (r = 0; r < (int)CAL_REFS[screen]; r++) {
            const CalMeas *m = calMeasSlot(screen, v, r);
            tot++;
            if (m && m->valid) filled++;
        }
    }
    if (outTotal) *outTotal = tot;
    return filled;
}

static int     cal_record_now;
static int     cal_skip_now;


/* ---------------------------------------------------------------------------
 * Live text-skew apply/CONVERGE. A way to apply the CURRENTLY measured
 * TEXT H correction live, re-measure, and converge to ~0 over a few rounds
 * - rather than hand-computing a constant offline and hardcoding it into a
 * game, only to find out on the next hardware flash that it was wrong
 * (exactly what happened to an early guessed constant, see
 * vxt_smart_text.c's own history). Scoped to TEXT H (horizontal) only -
 * TEXT V's rotated-space conversion from screen-measured drift back to the
 * module's LOCAL pre-rotation (cross,along) pair is a real, unconfirmed
 * open question (see vxt_smart_text.c's own caveat), not something to
 * quietly assume correct here.
 *
 * The converge loop: RECORD (button 4, on a TEXT H row's END reference -
 * the only reference with a known character count) folds that reading into
 * the accumulated cal_text_comp_cross/along; button 2 (a plain TAP) TOGGLES
 * whether the accumulated pair is being applied to the draw. The
 * two are independent so the mode is always exactly what the user last set,
 * never implicitly changed by taking a measurement - see calDrawReadout()'s
 * "corr on/OFF" line, added so this is never invisible.
 *
 * Control scheme, revised after the first attempt didn't register on
 * hardware: this was originally a HOLD-buttons-2+3-together chord for the
 * apply toggle, freeing nothing else up. That didn't register on real
 * hardware, so it is now a plain button 2 tap instead - and since button 2
 * is no longer free for variant-cycling, variant cycling moved to button 1
 * HELD + joystick left/right (see calBtnHeld()/CAL_VARIANT_JOY_THRESHOLD
 * below), with button 1's own
 * screen-cycle now firing on RELEASE rather than press, and only if the
 * press was short enough to be a genuine tap rather than the start of a
 * hold-for-variant gesture - see the button 1 handling block in
 * vxt_cal_handler() for the exact mechanism.
 *
 * Why fold-in must branch on whether apply was on during that measurement
 * (the exact question this feature was scoped around): if apply was ON, the
 * string just measured was ALREADY drawn with the current accumulated
 * correction, so its residual DY/DX is what's LEFT to fix - fold it in by
 * ADDING to the accumulator. If apply was OFF, the string was drawn
 * uncorrected, so its DY/DX is the FULL raw error - folding that in by
 * REPLACING the accumulator (not adding) is what avoids double-counting
 * whatever was already in there from an earlier, unrelated round. */
#define CAL_TEXT_COMP_SCALE_DIV  8    /* mirrors vxt_smart_text.c's own
                                      * PRIVATE VXT_TEXT_SCALE (not exported
                                      * by that header) - keep in sync by
                                      * hand if that ever changes; see this
                                      * module's rate-unit conversion,
                                      * vxtSmartTextSetSkewComp()'s own
                                      * comment for the formula this mirrors */
static int8_t   cal_text_comp_cross;
static int8_t   cal_text_comp_along;
static uint8_t  cal_text_apply_enabled;

/* This rig's OWN general text - titles, labels, the readout - is drawn
 * uncorrected by default, on purpose: the two lines above are this rig's
 * live, in-session CANDIDATE for the TEXT H/V screens' own narrow
 * demonstration, not a general-purpose correction. Loading the machine's
 * already-SAVED text comp separately, into its own pair below, lets the
 * rig's own UI read correctly without touching that narrow, carefully
 * scoped candidate mechanism at all. */
static int8_t   cal_ui_text_cross;
static int8_t   cal_ui_text_along;

/* Added, real gap found via direct feedback ("Corr on/off using
 * button2 [has no effect] for the other test"): button 2 has always
 * toggled cal_text_apply_enabled unconditionally, on every screen, but only
 * calScreenText() ever reads it - so on CHAIN/SHARED/etc the toggle
 * flipped a variable nothing looked at. Extended the converge-loop
 * mechanism (apply toggle + RECORD folds the residual in, add-vs-replace on
 * whether apply was on) to line geometry, starting with SHARED (the
 * "priority case" - a shared-vertex defect in a large closed-arc model,
 * still unresolved pending a Class I vs III blocking question).
 *
 * First version (same day, REPLACED) decomposed the correction into each
 * connector's own local along/cross frame - modeled on text's own
 * direction-relative comp, since that's the only precedent in this file.
 * Hardware-tested and DISPROVEN: fitting the correction at one SHARED
 * vertex (the 3-o'clock one, whose local frame happens to be pure
 * screen-X) converged that vertex to ~0 residual but did not generalize to
 * the other five, which sit at different angles - a photo showed the
 * diagonal connectors still visibly short of their vertices while the
 * fitted one landed exactly. A direction-relative model predicts it SHOULD
 * generalize if the physical defect really is "along/across this specific
 * line's own direction." It didn't, which rules that model out.
 *
 * Replaced with a flat, GLOBAL, screen-space (dy,dx) offset - the same
 * additive nudge applied to every test figure on every geometry screen,
 * regardless of that figure's own direction. This matches what the
 * original (pre-tool) calmeas.csv SHARED data showed before any
 * of this was built: dx ran a large, consistently POSITIVE 900-1400 across
 * all six differently-angled connectors - a pattern a direction-relative
 * model cannot produce (it would flip sign with direction), but a flat
 * screen-space bias produces exactly. int16_t for the same reason as
 * before - a single-segment correction needs more range than text's
 * per-character-divided int8_t. Shared across LADDER/SCALE/CHAIN/SHARED/
 * ANGLE/DEFLECT (not TEXT H, which keeps its own proven, separate
 * mechanism) so the same question - does one flat correction explain
 * everything, on every screen, at every angle and distance - gets
 * answered with actual breadth instead of one screen's six data points.
 *
 * Revised again (round 3) - PER-REFERENCE, not one global pair.
 * Real gap found via direct feedback ("if you look at that vertex and line
 * and all the other lines and vertices, they don't line up after I've gone
 * through and marked the end of each line... the visual with Corr=on should
 * result in the line ends and vertices connecting together"). That
 * expectation is correct and the tool did not meet it, for a structural
 * reason: ONE global (dy,dx) cannot simultaneously close six connectors
 * whose errors differ, so measuring all six just meant each RECORD
 * overwrote the last and only the final one could ever look right. Now each
 * reference index carries its OWN correction, applied to that reference's
 * OWN figure (calRef() is called in draw order by calMark()/calTick(), so
 * ref index == figure index on every geometry screen - verified per screen).
 * Measure every item on a screen and every item should visibly close up.
 *
 * This does not weaken the original class I vs iii question, it sharpens
 * it. "Does one flat correction generalize" is now answered by READING the
 * six stored numbers (identical => flat model; varying with angle/length/
 * deflection => it doesn't), which is a measurement rather than an eyeball
 * judgement. And it adds a check nothing else in the rig could make: if
 * per-item corrections, fitted per item, still don't visually close, then
 * the error is not repeatable frame to frame and no static calibration of
 * any shape can fix it - a decisive result the old single-value design
 * could not distinguish from "wrong model."
 *
 * Sized CAL_MAX_REFS (the same bound cal_ref_y/x already use, so any screen
 * that can register a reference can carry a correction for it): 40 * 2 *
 * int16_t = 160 bytes of bss. Reset on every screen AND variant change -
 * a variant change is different geometry, so a correction fitted to the old
 * one is meaningless, and keeping it would be the "stale
 * calibration" failure this whole rig exists to avoid. */
static int16_t  cal_geom_comp_dy[CAL_MAX_REFS];
static int16_t  cal_geom_comp_dx[CAL_MAX_REFS];
static uint8_t  cal_geom_comp_set[CAL_MAX_REFS];   /* has this ref been
                                    * measured this session? drives the
                                    * on-screen MEAS/-- indicator, so
                                    * "nothing happened" can be told apart
                                    * from "measured, and the answer was
                                    * near zero" - those look identical
                                    * without it */
static uint8_t  cal_geom_apply_enabled;
/* Which (screen,variant) the corrections above were fitted to - -1 = none.
 * Any change invalidates them, see the reset in vxt_cal_handler(). */
static int8_t   cal_geom_comp_screen  = -1;
static uint8_t  cal_geom_comp_variant;
static uint8_t  cal_geom_seed_pending;

/* Button 1's learned bitmask - single button, so the ISOLATED-edge learning
 * technique is the proven-safe one: learning MULTIPLE coincident bits from
 * one frame, tried for an old all-4-buttons hold gesture elsewhere in this
 * toolkit, proved fragile and was replaced. Button 1 is pressed solo
 * constantly during normal rig use (screen cycling), so this learns fast. */
static uint8_t  cal_btn1_mask;

/* Button 1 TAP-vs-HOLD state. A short press (<=
 * CAL_BTN1_TAP_MAX_FRAMES) cycles the screen, on RELEASE - not on press,
 * so a press that turns into a hold-for-variant gesture never also fires a
 * screen cycle first. A longer hold cycles NOTHING on its own; it just
 * arms the joystick-driven variant control below. */
#define CAL_BTN1_TAP_MAX_FRAMES   12   /* ~0.24s @ 50Hz */
static uint16_t cal_btn1_hold_frames;

/* Variant cycling via button 1 HELD + joystick left/right, replacing the
 * old plain button-2-tap. THRESHOLD/REARM form a small hysteresis band
 * (must return near center before the next step counts) so stick jitter
 * near the trigger point can't fire two steps for one push - same
 * reasoning as any other debounced digital-from-analog control in this
 * project. */
#define CAL_VARIANT_JOY_THRESHOLD   60
#define CAL_VARIANT_JOY_REARM       20
static uint8_t  cal_var_joy_armed = 1;

static int calBtnHeld(volatile uint8_t *parm, uint8_t *learnedMask, uint8_t rawEdge)
{
    uint8_t raw = parm[VXT_IN_BTNS];
    if (!*learnedMask && rawEdge && raw && !(raw & (uint8_t)(raw - 1))) {
        *learnedMask = raw;   /* learned from an ISOLATED (single-bit) press only */
    }
    if (*learnedMask) return (raw & *learnedMask) != 0;
    return rawEdge != 0;   /* not yet learned - edge-only fallback, imperfect
                            * but harmless (one frame of "held" per press) */
}

/* Round-to-nearest + clamp to int8_t range - used by the live RECORD
 * fold-in, inside vxt_cal_handler() below. */
static int8_t calClampComp(float v)
{
    int32_t r = (int32_t)(v + (v >= 0.0f ? 0.5f : -0.5f));
    if (r > 127) r = 127;
    if (r < -128) r = -128;
    return (int8_t)r;
}

/* int16_t counterpart for cal_geom_comp_dy/dx - see that pair's own header
 * comment for why geometry comp needs a wider range than text's. Clamp is
 * generous (+-8000) rather than tight, since this is a live measurement
 * tool finding out what the real magnitude is, not a game applying an
 * already-trusted value. */
static int16_t calClampComp16(float v)
{
    int32_t r = (int32_t)(v + (v >= 0.0f ? 0.5f : -0.5f));
    if (r > 8000) r = 8000;
    if (r < -8000) r = -8000;
    return (int16_t)r;
}

/* Applies the live geometry correction to a test-figure delta, IF apply is
 * on - used by every geometry screen (LADDER/SCALE/CHAIN/SHARED/ANGLE/
 * DEFLECT) on exactly the segment being measured against a reference,
 * never on reference marks/ticks themselves (those must stay the true
 * ideal target - this rig's foundational rule, see the file's own header).
 * Off (cal_geom_apply_enabled == 0) returns the input unchanged, so every
 * screen draws byte-identical to before this feature existed. */
/* `fig` is the figure's index within its screen (0 = first line/spoke/
 * connector that screen draws). It is offset by cal_ref_base because the
 * reference card, when on, draws its own centre cross through calMark() and
 * therefore registers as reference 0 BEFORE the screen runs - shifting every
 * screen's own references by one. That shift is pre-existing (it also means
 * a `ref` column in calmeas.csv means something different depending on the
 * `card` column - worth knowing when reading old logs), but per-figure
 * corrections are the first thing that breaks on it, since a
 * correction fitted to reference i would be applied to figure i-1. */
static void calApplyGeomComp(int fig, int32_t *dy, int32_t *dx)
{
    int ref = cal_ref_base + fig;
    if (cal_geom_apply_enabled && fig >= 0 && ref >= 0 && ref < CAL_MAX_REFS) {
        *dy += cal_geom_comp_dy[ref];
        *dx += cal_geom_comp_dx[ref];
    }
}

/* ---------------------------------------------------------------------------
 * Small drawing helpers.
 *
 * Everything here draws INDEPENDENTLY (its own reposition per line) on
 * purpose: a reference mark that chained off the figure under test would
 * inherit that figure's error and could never reveal it. This is the one
 * place in the project where paying ~9-14 records for a short line is the
 * correct choice rather than the expensive mistake convention 12 warns about.
 * ------------------------------------------------------------------------ */
static void calLineAt(int32_t y, int32_t x, int32_t dy, int32_t dx,
                      uint8_t intensity, uint8_t scale)
{
    gamelibDrawAutoLine(y, x, dy, dx, intensity, scale);
}

/* Same, but TRANSLATED by figure `fig`'s live correction - the origin moves,
 * the delta does not.
 *
 * Fixed. LADDER/SCALE/ANGLE and SHARED's connectors used to add
 * the correction to the DELTA instead, which is wrong twice over:
 *
 *  1. It distorts the figure. A rung corrected by -800 on a 1000-unit line
 *     is drawn at 200 units - its endpoint lands on the tick, but the line
 *     is visibly the wrong length and its start is still displaced.
 *  2. It cannot converge in one pass. The error has a component that scales
 *     with the delta (ANGLE measures 403 at length 4000 and 672 at 9500 -
 *     roughly proportional), so changing the delta changes the very error
 *     just measured. Each correction perturbs its own measurement.
 *
 * Translating leaves the delta - and therefore its draw error - untouched,
 * so subtracting the measured error lands the endpoint exactly, first time.
 * It is also what the evidence says the defect is: a whole-figure
 * displacement (CHAIN's error is constant across N=4..16, and this is the
 * same correction CHAIN/DEFLECT/SHARED's ring already use). Zero extra
 * records either way - only the coordinates change. */
static void calLineAtComp(int fig, int32_t y, int32_t x, int32_t dy, int32_t dx,
                          uint8_t intensity, uint8_t scale)
{
    int32_t cy = 0, cx = 0;
    calApplyGeomComp(fig, &cy, &cx);
    gamelibDrawAutoLine(y + cy, x + cx, dy, dx, intensity, scale);
}

/* Added - CHAIN and DEFLECT's own equivalent of a "shared
 * vertex": both measure whether a closed chained polygon's LAST segment
 * lands back on its own start (registered independently as the
 * reference). gamelibChainBigClosedPerimeter() is a shared gamelib
 * primitive real games also use, so this does not reach into it or add a
 * cal-specific parameter to it - instead it draws the polygon exactly as
 * before, then, if apply is on, appends ONE more chained segment nudging
 * the beam by the live correction. gamelibChainBigClosedPerimeter() left
 * the chain open (GB_RUN_DRAW) on exit, so this continues the same run,
 * not a fresh one - the correction reads as part of the closing corner,
 * not a separate mark. */
#define CAL_PERIM_MAX  32   /* CAL_CHAIN_N's largest entry - the only caller
                             * that gets near it */

static void calChainClosedPerimeterComp(const int32_t *py, const int32_t *px, int n,
                                        uint8_t bigScale, uint8_t intensity, int ref)
{
    int32_t ty[CAL_PERIM_MAX], tx[CAL_PERIM_MAX];
    int32_t cy = 0, cx = 0;
    int i;

    calApplyGeomComp(ref, &cy, &cx);
    if ((!cy && !cx) || n > CAL_PERIM_MAX) {
        gamelibChainBigClosedPerimeter(py, px, n, bigScale, intensity);
        return;
    }

    /* Translate the whole figure - do not append a corrective segment.
     *
     * Fixed (same day it was written). The first version drew the
     * polygon unchanged and then emitted the correction as one more chained
     * step. Two things wrong with that, both real:
     *
     *  1. gamelibDispatchAutoDraw() is a DRAW, beam on - so it did not
     *     correct anything, it PAINTED a stub sticking out of the closing
     *     corner, one correction-length long. At the ~900-unit magnitude
     *     this defect has, that is a visible line artifact.
     *  2. On DEFLECT the reference is the square's START corner, so nudging
     *     at the END corrects a different point than the one being measured.
     *
     * Whole-figure translation is also the shape the evidence supports: the
     * CHAIN length sweep (936/910/879/869/818 for N = 4/6/8/12/16) shows an
     * essentially CONSTANT error independent of segment count, i.e. the
     * figure is displaced as a unit rather than drifting per segment. Same
     * correction the SHARED ring uses, and it costs zero extra records -
     * the coordinates change, the record count does not. */
    for (i = 0; i < n; i++) {
        ty[i] = py[i] + cy;
        tx[i] = px[i] + cx;
    }
    gamelibChainBigClosedPerimeter(ty, tx, n, bigScale, intensity);
}

/* Register a point the caret can be measured against. Every reference mark
 * calls this, so "what is the error HERE" works at any mark on any screen
 * without each screen having to nominate one privileged target. */
static void calRef(int32_t y, int32_t x)
{
    if (cal_ref_n < CAL_MAX_REFS) {
        cal_ref_y[cal_ref_n] = y;
        cal_ref_x[cal_ref_n] = x;
        cal_ref_n++;
    }
}

/* A short cross ("plus") centred on (y,x) - the standard reference mark. */
static void calMark(int32_t y, int32_t x, int32_t arm, uint8_t intensity)
{
    calRef(y, x);
    calLineAt(y, x - arm, 0, 2 * arm, intensity, CAL_BIG_SCALE);
    calLineAt(y - arm, x, 2 * arm, 0, intensity, CAL_BIG_SCALE);
}

/* ===========================================================================
 * Tick orientation convention:
 *
 *   A tick that marks a point ON A LINE must be drawn PERPENDICULAR to that
 *   line. A tick lying ALONG the line it marks is invisible against it - there
 *   is no crossing point to sight the caret onto, which is the entire job.
 *
 * This is why calTick() takes the LINE's direction and computes the
 * perpendicular itself, rather than taking the tick's own direction: the call
 * site states what it is marking, and cannot get the orientation wrong.
 *
 * PREFER calTick() OVER calMark() for anything on a line. calMark() draws an
 * axis-aligned cross with no knowledge of direction, so on a horizontal or
 * vertical line one of its two arms lies exactly along the figure - and at a
 * right-angle corner (DEFLECT's squares) both arms do. Five sites were
 * converted for exactly this: CHAIN's vertex 0 (near-tangential
 * edges), DEFLECT's corner (use the 45-degree diagonal), TEXT's string start
 * and end (perpendicular to the run), and REPOS's segment end. It also costs
 * one line instead of two, so it is cheaper on the record budget.
 *
 * calMark() remains correct for marking a POINT that is not on a line - the
 * reference card's centre cross being the one such case left, where a cross is
 * the natural true-centre target.
 * ======================================================================== */

/* Tick GEOMETRY only, WITHOUT registering a reference. Split out of calTick()
 * This exists so a purely DECORATIVE hash can reuse it: calTick() calls
 * calRef(), and anything that is not itself a measurement target must never
 * register one - see calDrawCard()'s own note for the misnumbering that
 * causes.
 *
 * The tick is perpendicular to (dirY,dirX). Perpendicular of (dy,dx) is
 * (dx,-dy); normalised crudely via the larger component - exactness is not
 * needed for a visual mark, and a sqrt here would cost cycles for nothing. */
static void calTickAt(int32_t y, int32_t x, int32_t dirY, int32_t dirX,
                      int32_t len, uint8_t intensity)
{
    int32_t py = dirX, px = -dirY;
    int32_t m = (py < 0 ? -py : py);
    int32_t n = (px < 0 ? -px : px);
    int32_t big = (m > n) ? m : n;

    if (big == 0) return;
    py = py * len / big;
    px = px * len / big;
    calLineAt(y - py / 2, x - px / 2, py, px, intensity, CAL_BIG_SCALE);
}

/* A single tick perpendicular to the given direction, centred on (y,x), which
 * ALSO registers (y,x) as a measurable reference. Used where a full cross
 * would visually collide with the figure's own line. */
static void calTick(int32_t y, int32_t x, int32_t dirY, int32_t dirX,
                    int32_t len, uint8_t intensity)
{
    calRef(y, x);
    calTickAt(y, x, dirY, dirX, len, intensity);
}

/* Corner L-brackets - overscan / centring reference, the same trick PiTrex's
 * own settings GUI uses (v_SettingsGUI draws three of these at the extremes).
 * Four here, not three: this card's primary job is judging CENTRE, and four
 * corners let you compare opposite pairs directly. */
static void calBrackets(uint8_t intensity)
{
    const int32_t sy[4] = {  1,  1, -1, -1 };
    const int32_t sx[4] = { -1,  1,  1, -1 };
    int i;

    for (i = 0; i < 4; i++) {
        int32_t cy = sy[i] * CAL_CARD_HALF_Y;
        int32_t cx = sx[i] * CAL_CARD_HALF_X;
        calLineAt(cy, cx, 0, -sx[i] * CAL_BRACKET_LEN, intensity, CAL_BIG_SCALE);
        calLineAt(cy, cx, -sy[i] * CAL_BRACKET_LEN, 0, intensity, CAL_BIG_SCALE);
    }
}

/* ---------------------------------------------------------------------------
 * Numeric readout.
 *
 * vxtSmartTextNumber() CLAMPS magnitude to 999 - fine for a game HUD, but a
 * measurement rig that clamps is worse than one with no readout at
 * all, because a clamped number still looks like a reading. So this file
 * prints its own, up to 5 digits plus sign.
 * ------------------------------------------------------------------------ */
static void calNumber(int32_t v)
{
    char buf[8];
    int i = 0, j;
    uint32_t mag;

    if (v < 0) { vxtSmartTextChar('-'); mag = (uint32_t)(-v); }
    else       { mag = (uint32_t)v; }
    if (mag > 99999u) mag = 99999u;

    do { buf[i++] = (char)('0' + (mag % 10u)); mag /= 10u; } while (mag);
    for (j = i - 1; j >= 0; j--) vxtSmartTextChar(buf[j]);
}

#define CAL_TEXT_ROW(physY, physX) \
    vxtSmartTextBegin((int16_t)((physY) / CAL_POS_SCALE), \
                      (int16_t)((physX) / CAL_POS_SCALE))

/* ---------------------------------------------------------------------------
 * Screen 0 - true centre test card.
 *
 * The standard hardware-calibration overlay, and the FOUNDATION of everything
 * else in this rig: "calibrated centre" is a whole-toolkit property, and every
 * other screen's numbers are relative to the origin this one establishes. If
 * the electrical zero the beam recenters to is not the tube's optical centre,
 * then a deflection sweep measured around the wrong origin is measuring the
 * offset, not the drift.
 *
 * Four independent things to read off it:
 *   - centre cross      -> is electrical zero the optical centre? (offset)
 *   - corner brackets   -> overscan, and centring by comparing opposite pairs
 *   - crosshatch        -> linearity / pincushion / barrel across the field
 *   - aspect polygon    -> X:Y gain ratio (a circle drawn with equal phys
 *                          radii reads as an ellipse if the gains differ)
 * ------------------------------------------------------------------------ */
#define CAL_HATCH_N   5
#define CAL_ASPECT_N  12
#define CAL_ASPECT_R  5000L

static void calDrawCard(int full)
{
    int i;

    /* Centre cross - drawn last-but-one in importance order, first here so a
     * budget overflow on a crowded screen loses the crosshatch, not this.
     *
     * It registers A reference only on centre. Fixed, real bug
     * exposed the moment the card started drawing at all (see cal_overlay's
     * note): calMark() calls calRef(), so as a background overlay on every
     * other screen this inserted itself as reference 0 and pushed
     * the screen's own references up by one. Two consequences, both visible
     * in the logged data:
     *   - every card-ON row is numbered one too high (ACCUM V7 ref 0 recorded
     *     (0,0), the card's own cross, and ref 1 recorded the UPPER tip that
     *     should have been ref 0), so those rows are misaligned and must be
     *     re-taken;
     *   - the screen's LAST reference became unreachable - it landed at index
     *     CAL_REFS[screen], which calMeasSlot() correctly refuses, so ACCUM's
     *     and CHORD's lower tip could not be recorded on a card-on variant at
     *     all, making A uncomputable there.
     * On CENTRE the cross IS the measurement target and must keep registering,
     * which is why this is keyed on the screen rather than on `full` (CENTRE
     * variant 1 calls calDrawCard(0) and still needs its reference). */
    if (cal_screen == CAL_SCR_CENTRE) {
        calMark(0, 0, CAL_CROSS_ARM, CAL_INT_REF);
    } else {
        calLineAt(0, -CAL_CROSS_ARM, 0, 2 * CAL_CROSS_ARM,
                  CAL_INT_REF, CAL_BIG_SCALE);
        calLineAt(-CAL_CROSS_ARM, 0, 2 * CAL_CROSS_ARM, 0,
                  CAL_INT_REF, CAL_BIG_SCALE);
    }
    calBrackets(CAL_INT_GRID);

    if (!full) return;   /* reduced form when used as a background overlay */

    for (i = 0; i < CAL_HATCH_N; i++) {
        int32_t f = (int32_t)i - (CAL_HATCH_N / 2);
        int32_t y = f * (2 * CAL_CARD_HALF_Y / (CAL_HATCH_N - 1));
        int32_t x = f * (2 * CAL_CARD_HALF_X / (CAL_HATCH_N - 1));
        calLineAt(y, -CAL_CARD_HALF_X, 0, 2 * CAL_CARD_HALF_X,
                  CAL_INT_GRID, CAL_BIG_SCALE);
        calLineAt(-CAL_CARD_HALF_Y, x, 2 * CAL_CARD_HALF_Y, 0,
                  CAL_INT_GRID, CAL_BIG_SCALE);
    }

    /* Aspect polygon: EQUAL phys radius in X and Y by construction, so any
     * departure from round is the display's own X:Y gain ratio, not ours. */
    {
        int32_t py[CAL_ASPECT_N], px[CAL_ASPECT_N];
        for (i = 0; i < CAL_ASPECT_N; i++) {
            float a = (float)i * (6.283185307f / (float)CAL_ASPECT_N);
            py[i] = (int32_t)(CAL_ASPECT_R * sinf(a));
            px[i] = (int32_t)(CAL_ASPECT_R * cosf(a));
        }
        gamelibChainBigClosedPerimeter(py, px, CAL_ASPECT_N,
                                       CAL_BIG_SCALE, CAL_INT_FIGURE);
    }
}

static void calScreenCentre(uint8_t variant)
{
    calDrawCard(variant == 0);   /* variant 1 = card without the crosshatch,
                                  * for judging the centre cross against a
                                  * clean field */
}

/* ---------------------------------------------------------------------------
 * Screen 1 - LADDER: the length sweep, and the mechanism discriminator.
 *
 * The same line drawn at five lengths, each from its own fresh reposition,
 * each with an independent reference tick at its ideal endpoint.
 *
 *   error grows with length  -> PROPORTIONAL (per-axis crosstalk)  -> Group B1
 *   error constant at every  -> FIXED per record                   -> Group B3
 *   length
 *
 * Our evidence to date ("worse toward the end of a long chain") does not
 * distinguish those two. This screen does, which is why it is the first
 * measurement worth taking.
 * ------------------------------------------------------------------------ */
static const int32_t CAL_LADDER_LEN[5] = { 1000, 2000, 4000, 8000, 16000 };

static void calScreenLadder(uint8_t variant)
{
    int i;
    for (i = 0; i < 5; i++) {
        int32_t L  = CAL_LADDER_LEN[i];
        int32_t y0 = 7000 - (int32_t)i * 3200;
        int32_t x0 = -8000;
        int32_t dy, dx;

        switch (variant) {
        case 1:  dy = L; dx = 0; y0 = -8000; x0 = -8000 + (int32_t)i * 4000; break;
        case 2:  dy = L * 707 / 1000; dx = L * 707 / 1000;
                 y0 = 6000 - (int32_t)i * 3000; x0 = -9000;                  break;
        default: dy = 0; dx = L;                                             break;
        }

        /* Added - live geometry comp, on the test line only,
         * never on the tick below (that's the true ideal target). */
        calLineAtComp(i, y0, x0, dy, dx, CAL_INT_FIGURE, CAL_BIG_SCALE);
        calTick(y0 + dy, x0 + dx, dy, dx, CAL_TICK_LEN, CAL_INT_REF);

    }
}

/* ---------------------------------------------------------------------------
 * Screen 2 - SCALE band probe. Gives roadmap item 10a a NUMBER.
 *
 * One fixed length drawn four ways, each against its own reference tick:
 *   row 0  chained at CAL_DRAW_SCALE      (many small records)
 *   row 1  one record at CAL_BIG_SCALE    (the UNVERIFIED 27-nop pairing)
 *   row 2  one record, scale SEARCHED     (Sec.72's accurate path)
 *   row 3  Huge - genuine Timer-1 poll    (known-good reference)
 *
 * Row 3 is correct by construction (it waits on hardware rather than a
 * fixed nop count), so it is the ruler the other three are read against. If
 * row 1 lands short of row 3, the scale-64 nop extrapolation is measurably
 * wrong and Sec.54's standing suspicion becomes a fact. If all four agree,
 * that suspicion can finally be closed - which is worth almost as much.
 * ------------------------------------------------------------------------ */
static const int32_t CAL_SCALE_LEN[4] = { 4000, 8000, 16000, 24000 };

static void calScreenScale(uint8_t variant)
{
    int32_t L  = CAL_SCALE_LEN[variant & 3];
    int32_t x0 = -L / 2;
    int i;

    for (i = 0; i < 4; i++) {
        int32_t y = 6000 - (int32_t)i * 4000;
        /* Live geometry comp, applied as a TRANSLATION of this row's own
         * origin - see calLineAtComp() for why translating beats adjusting
         * the delta. Applied uniformly across all four draw paths so the
         * same question gets asked of every path; the reference tick below
         * is never moved. */
        int32_t cy = 0, cx = 0;
        calApplyGeomComp(i, &cy, &cx);

        switch (i) {
        case 0:
            /* Chained at the small draw scale - many SM_continue_d records. */
            gamelibRepositionAbs(y + cy, x0 + cx);
            gamelibChainDelta(0, L, 1);
            gamelibBeamMarkOpenDraw();
            break;
        case 1:
            calLineAt(y + cy, x0 + cx, 0, L, CAL_INT_FIGURE, CAL_BIG_SCALE);
            break;
        case 2:
            calLineAt(y + cy, x0 + cx, 0, L, CAL_INT_FIGURE, GAMELIB_SCALE_ACCURATE);
            break;
        default:
            gamelibDrawHugeLine(y + cy, x0 + cx, 0, L, CAL_INT_FIGURE);
            break;
        }
        calTick(y, x0 + L, 0, L, CAL_TICK_LEN, CAL_INT_REF);
    }
}

/* ---------------------------------------------------------------------------
 * Screen 3 - CHAIN accumulation (archetype A: closed chained polygon).
 *
 * Abstracts a window outline, a HUD rectangle and a ship's wing outline
 * down to their shared essence: does a chain come back to where it started?
 * The polygon is drawn as ONE chained run; an independent mark sits at vertex
 * 0. The gap between the chain's final landing point and that mark IS the
 * accumulated error over N records.
 *
 * Sweeping N gives Group C its numbers directly: the N at which the gap first
 * becomes visible is maxLinesPerRecenter, and the travel at that point is
 * maxDriftBudget - both currently a GUESSED value of 6.
 * ------------------------------------------------------------------------ */
static const uint8_t CAL_CHAIN_N[7] = { 4, 6, 8, 12, 16, 24, 32 };
#define CAL_CHAIN_R  6000L

static void calScreenChain(uint8_t variant)
{
    int n = CAL_CHAIN_N[variant % 7];
    int32_t py[32], px[32];
    int i;

    for (i = 0; i < n; i++) {
        float a = (float)i * (6.283185307f / (float)n);
        py[i] = (int32_t)(CAL_CHAIN_R * sinf(a));
        px[i] = (int32_t)(CAL_CHAIN_R * cosf(a));
    }
    /* Live geometry comp on the closing segment, via
     * calChainClosedPerimeterComp() - see its own header comment. */
    calChainClosedPerimeterComp(py, px, n, CAL_BIG_SCALE, CAL_INT_FIGURE, 0);

    /* Independent reference at vertex 0 - drawn AFTER, so it cannot be part
     * of the chain it is measuring.
     *
     * Perpendicular tick, not a cross (see the tick-orientation
     * convention at calTickAt()): vertex 0 is at 3 o'clock, where the
     * polygon's own edges run near-TANGENTIALLY (i.e. near-vertical), so a
     * cross's vertical arm lay along them and the crossing point was hard to
     * read off. Passing the local tangent (1,0) puts the tick radial. */
    calTick(py[0], px[0], 1, 0, CAL_TICK_LEN, CAL_INT_REF);

}

/* ---------------------------------------------------------------------------
 * Screen 4 - shared vertex (archetype B). The priority case.
 *
 * This is a real, still-unresolved defect pattern with every game-specific
 * detail removed: a closed chained polygon, plus connector lines drawn
 * INDEPENDENTLY from outside, each aimed at one of that polygon's vertices.
 * Do the connectors' endpoints land on the polygon's corners, or not?
 *
 * The variants isolate the candidate fix rather than assuming it:
 *   0  connectors on the cheap fixed-scale path
 *   1  connectors on the SEARCHED-scale accurate path
 *   2  both ring and connectors accurate
 *
 * If 0 and 1 differ, the scale search does what Sec.72 claimed. If they do
 * not, Sec.73's report is confirmed and the cause lies elsewhere - which is
 * exactly the question Sec.73 says to answer with measurement rather than
 * another round of reasoning.
 * ------------------------------------------------------------------------ */
#define CAL_SHARED_N     6
#define CAL_SHARED_R     6000L
#define CAL_SHARED_OUT   9500L

static void calScreenShared(uint8_t variant)
{
    int32_t py[CAL_SHARED_N], px[CAL_SHARED_N];
    int32_t ry[CAL_SHARED_N], rx[CAL_SHARED_N];
    int32_t ringCy = 0, ringCx = 0;
    uint8_t ringScale = (variant == 2) ? GAMELIB_SCALE_ACCURATE : CAL_BIG_SCALE;
    uint8_t connScale = (variant == 0) ? CAL_BIG_SCALE : GAMELIB_SCALE_ACCURATE;
    int i;

    for (i = 0; i < CAL_SHARED_N; i++) {
        float a = (float)i * (6.283185307f / (float)CAL_SHARED_N);
        py[i] = (int32_t)(CAL_SHARED_R * sinf(a));
        px[i] = (int32_t)(CAL_SHARED_R * cosf(a));
    }

    /* The ring is now reference 0, and it is correctable . Until now this screen registered only the six ideal vertices and
     * corrected only the CONNECTORS, which made a real defect impossible to
     * see: a hardware session measured all six connectors at 0-136 units
     * (i.e. already landing essentially on the ideal vertices), applied
     * those corrections, and the picture still showed ~1000-unit gaps -
     * because the thing that is displaced is the RING, and nothing
     * here could measure it, let alone correct it.
     *
     * The ring is the FIRST figure drawn on this screen, and the first
     * figure of a frame is the one that suffers this project's documented
     * first-element anomaly. The CHAIN screen's
     * own length sweep confirms the shape of it: closure error 936/910/879/
     * 869/818 for N = 4/6/8/12/16 - essentially CONSTANT, and slightly
     * BETTER with more segments, so it is not per-segment accumulation and
     * no amount of recentering would address it. A whole-figure translation
     * is therefore the right correction, which is also the cheap one: it
     * changes the coordinates handed to the existing draw, adding zero 6809
     * records.
     *
     * Ref 0 = the ring's own corner at vertex 0; refs 1..N = the connectors.
     * Refs 0 and 1 deliberately share the same IDEAL coordinate but measure
     * two different drawn points (where the ring's corner really landed vs.
     * where connector 0's tip really landed) - which is exactly the
     * distinction this screen could not previously express, and exactly the
     * confusion that cost a full session. */
    calRef(py[0], px[0]);
    calApplyGeomComp(0, &ringCy, &ringCx);
    for (i = 0; i < CAL_SHARED_N; i++) {
        ry[i] = py[i] + ringCy;
        rx[i] = px[i] + ringCx;
    }
    gamelibChainBigClosedPerimeter(ry, rx, CAL_SHARED_N, ringScale, CAL_INT_FIGURE);

    /* The connectors' references are still the IDEAL vertices, never the
     * ring's corrected ones - a reference that moved with the correction
     * under test could never reveal a residual (the same trap calScreenText()
     * documents in its own earlier fix). */
    for (i = 0; i < CAL_SHARED_N; i++) calRef(py[i], px[i]);

    for (i = 0; i < CAL_SHARED_N; i++) {
        float a  = (float)i * (6.283185307f / (float)CAL_SHARED_N);
        int32_t oy = (int32_t)(CAL_SHARED_OUT * sinf(a));
        int32_t ox = (int32_t)(CAL_SHARED_OUT * cosf(a));
        int32_t dy = py[i] - oy, dx = px[i] - ox;

        calLineAtComp(1 + i, oy, ox, dy, dx, CAL_INT_REF, connScale);
    }

}

/* ---------------------------------------------------------------------------
 * Screen 5 - ANGLE sweep. A NEW axis, added because making the rig generic
 * exposed that every game-specific screen this was originally derived from
 * was pure-X or pure-Y - which cannot see a defect that only appears on
 * diagonals.
 *
 * Equal-length spokes at equal angular steps, each independently drawn, each
 * with a reference tick at its ideal endpoint. A direction-dependent error
 * shows as ticks missed in some directions and hit in others.
 * ------------------------------------------------------------------------ */
static const int32_t CAL_ANGLE_R[3] = { 4000, 7000, 9500 };
#define CAL_ANGLE_N  12

static void calScreenAngle(uint8_t variant)
{
    int32_t R = CAL_ANGLE_R[variant % 3];
    int i;

    for (i = 0; i < CAL_ANGLE_N; i++) {
        float a = (float)i * (6.283185307f / (float)CAL_ANGLE_N);
        int32_t dy = (int32_t)(R * sinf(a));
        int32_t dx = (int32_t)(R * cosf(a));
        /* Added - live geometry comp, on the test spoke only,
         * never on the tick below. Twelve spokes at twelve different
         * angles - the widest angular coverage in the rig, so this is
         * the strongest test of whether a flat correction really is
         * direction-independent. */
        calLineAtComp(i, 0, 0, dy, dx, CAL_INT_FIGURE, CAL_BIG_SCALE);
        calTick(dy, dx, dy, dx, CAL_TICK_LEN, CAL_INT_REF);
    }
}

/* ---------------------------------------------------------------------------
 * Screen 6 - DEFLECTION sweep. The other NEW axis, and the one most likely to
 * change the shape of the whole calibration model.
 *
 * PiTrex's "ZERO BORDER" (resetToZeroDifMax) exists precisely because drift
 * depends on how far the beam is from centre. If that holds here too, then a
 * calibration measured only at screen centre is WRONG everywhere else - and
 * nothing this project currently has would reveal it.
 *
 * The identical small test figure (a chained square plus an independent mark
 * at its start corner) is drawn at five increasing deflections. Error growing
 * with deflection is the signature.
 * ------------------------------------------------------------------------ */
#define CAL_DEFLECT_STEPS  5
#define CAL_DEFLECT_SIDE   1600L

static void calScreenDeflect(uint8_t variant)
{
    int s;
    for (s = 0; s < CAL_DEFLECT_STEPS; s++) {
        int32_t f  = (int32_t)s;   /* 0 = centre .. 4 = full deflection */
        int32_t cy = 0, cx = 0;
        int32_t py[4], px[4];
        int i;

        switch (variant) {
        case 1:  cx = f * (CAL_CARD_HALF_X - CAL_DEFLECT_SIDE) / 4;            break;
        case 2:  cy = f * (CAL_CARD_HALF_Y - CAL_DEFLECT_SIDE) / 4;            break;
        default: cy = f * (CAL_CARD_HALF_Y - CAL_DEFLECT_SIDE) / 4;
                 cx = f * (CAL_CARD_HALF_X - CAL_DEFLECT_SIDE) / 4;            break;
        }

        py[0] = cy;                    px[0] = cx;
        py[1] = cy;                    px[1] = cx + CAL_DEFLECT_SIDE;
        py[2] = cy + CAL_DEFLECT_SIDE; px[2] = cx + CAL_DEFLECT_SIDE;
        py[3] = cy + CAL_DEFLECT_SIDE; px[3] = cx;
        (void)i;

        /* Added - live geometry comp on the closing segment. */
        calChainClosedPerimeterComp(py, px, 4, CAL_BIG_SCALE, CAL_INT_FIGURE, s);
        /* DIAGONAL tick: this marks
         * a square's CORNER, where a horizontal and a vertical edge meet - the
         * one case where both arms of a cross lay along the figure. A tick on
         * the (1,1) diagonal's perpendicular sits 45 degrees off both edges,
         * which is the best available separation at a right-angle corner. */
        calTick(py[0], px[0], 1, 1, CAL_TICK_LEN / 2, CAL_INT_REF);

    }
}

/* ---------------------------------------------------------------------------
 * Screens 7/8 - long text strings, horizontal and rotated.
 *
 * Added from real experience: a rotated-text routine had to be
 * hand-calibrated for a real game, and the calibration was found to depend
 * on WHERE on the screen the string sits and how many characters it has.
 * Neither dependency is measurable with any other screen in this rig.
 *
 * Why text is the worst case in the whole system, and why this gap mattered:
 * vxtSmartTextBegin() repositions once, and then the entire string is drawn
 * as a single chained run - every stroke of every glyph, plus the two moves
 * that advance between glyphs, all SM_continue_d with NO recenter anywhere.
 * A 24-character string of '8' (7 strokes, the densest glyph) is roughly 384
 * chained records from one reposition. For comparison the CHAIN screen's
 * worst case is 32. So if analog drift accumulates per record or per unit of
 * travel, long text is where it shows FIRST and WORST - and until now nothing
 * in this project measured it.
 *
 * The digital arithmetic here is exact (integer grid units times a constant),
 * so anything measured on these screens is analog, not rounding. That makes
 * them an unusually clean test.
 *
 * Rotation uses vxtSmartTextSetOrientation() (added to vxt_smart_text.c the
 * same day) rather than a private copy of the font - a third copy of that
 * glyph table is exactly the kind of hand-sync burden the file's own header
 * already warns about.
 * ------------------------------------------------------------------------ */
#define CAL_TEXT_ROWS      3
#define CAL_TEXT_GAP_PERP  2600L

/* Character counts, short to long, one test string per row.
 * The strings must be REPRESENTATIVE text, not a worst case. Skew builds up
 * per STROKE, but the correction is applied per CHARACTER, so a correction is
 * only right for text with the same strokes-per-character as the text it was
 * measured on. Measuring on the densest glyph ('8', 7 strokes/char) against
 * real UI text at ~3 (VX-COOP's own UI averages 2.86) gives a correction
 * ~2.5x too strong, pushing every real string up and to the right. These run
 * 3.00/2.88/2.92 strokes/char.
 *
 * Each string also STARTS on a glyph with a real bottom-left corner and ENDS
 * on one with a real bottom-right corner, so both marks have a visible point
 * to put the caret on - a letter like V, with no bottom-left corner, or a
 * trailing space, leaves the mark with nothing to land on.
 *
 * Changing any string makes every earlier TEXT H row stale - bump
 * VXT_CAL_TEXTH_METHOD (vxt_cal_load.h) whenever one changes.
 *
 * THREE rows, not four: 8+16+24 = 48 chars at ~6 records each is ~290
 * records, plus 6 reference ticks and the caret and readout - well inside the
 * 1536-record budget. Watch N= and OVF. */
static const int CAL_TEXT_LEN[CAL_TEXT_ROWS] = { 8, 16, 24 };
static const char *const CAL_TEXT_STRS[CAL_TEXT_ROWS] = {
    "DAC GAIN",                    /*  8 */
    "ACCUM BEAM CHAIN",            /* 16 */
    "ACCUM ANGLE CHAIN LASERS",    /* 24 */
};

/* Measurement-method version for TEXT H, written into every row's `method`
 * column. A row taken with an older method is dropped at load (so the
 * screen shows it as unmeasured and asks for a fresh reading) rather than
 * mixed with current rows. 0 = the old all-'8' strings. */
#define CAL_METHOD_TEXTH  VXT_CAL_TEXTH_METHOD   /* vxt_cal_load.h */

/* TEXT V draws the same strings, so it shares TEXT H's method number. Every
 * other screen is still on its original method, 0. */
static int calScreenMethod(int scr)
{
    return (scr == CAL_SCR_TEXTH || scr == CAL_SCR_TEXTV) ? CAL_METHOD_TEXTH : 0;
}

/* The error a TEXT H END reference owes to the string's own RUN: its reading
 * minus the START reference's reading for the same row, when that has been
 * measured. The start error is a fixed landing offset of the first glyph -
 * dividing it by the character count as if it were per-character drift
 * inflated the short rows (implied rate 14/12/10.7 for 8/16/24 chars on real
 * data, versus a flat 11.2/11.4/10.0 with it removed). Skew comp does not
 * move the first glyph, so the start reading is valid whether or not the
 * correction was on. Defined further down, after the slot table. */
static void calTextRunError(uint8_t variant, int endRef, int32_t dy, int32_t dx,
                            int32_t *runDy, int32_t *runDx);

/* Screen regions. The user's report is that the correction differs by
 * position, so these deliberately sample the corners AND the middle rather
 * than sweeping one axis. */
enum { CAL_POS_MID = 0, CAL_POS_TL, CAL_POS_TR, CAL_POS_BL, CAL_POS_BR };

static void calTextFill(char *buf, int row)
{
    int i;
    for (i = 0; i < CAL_TEXT_LEN[row]; i++) buf[i] = CAL_TEXT_STRS[row][i];
    buf[i] = '\0';
}

static void calScreenText(uint8_t variant, int vertical)
{
    char buf[32];
    int r;
    int rightSide  = (variant == CAL_POS_TR || variant == CAL_POS_BR);
    int bottomSide = (variant == CAL_POS_BL || variant == CAL_POS_BR);

    /* A 24-character string is 13,824 phys units long - WIDER than the screen's
     * half-width (13,500) and most of its half-height (18,000). So a region's
     * anchor is the end the string is pinned to, and which end that is depends
     * on both the region and the orientation:
     *   horizontal (advance +X): right-hand regions pin the END
     *   vertical   (advance -Y): bottom regions pin the END
     * Getting this wrong does not look like a bug - it looks like the string
     * is not there, or like a drawing defect at the screen edge. */
    int endAnchored = vertical ? bottomSide : rightSide;

    /* Fixed, real bug found via direct feedback ("corr on
     * doesn't change anything, RECORD doesn't make it realign"): `w` below
     * feeds directly into ey/ex, the REFERENCE mark's "ideal" position -
     * this function's own comment right above calMark(ey,ex,...) says that
     * mark must be the ideal target, independent of whatever is being
     * measured (this whole rig's foundational principle, see the file's
     * own top-of-file header). But vxtSmartTextWidthPhys() reads the
     * SHARED module-global skew-comp state (see its own earlier fix in
     * vxt_smart_text.c), and the caller used to set that state to the LIVE
     * correction for this ENTIRE function call before calling it - so the
     * "ideal" reference target was chasing whatever correction
     * was being tested, instead of staying fixed. A correction and its own
     * measuring stick moving together can never show a nonzero gap, right
     * or wrong.
     *
     * FIX: skew comp now gets turned on only around the actual
     * vxtSmartTextBegin()/vxtSmartTextStr() draw call below, INSIDE this
     * loop, per row - `w` (and therefore ey/ex) is always computed while
     * skew comp is at identity (0,0), so the reference mark is always the
     * TRUE nominal target regardless of what correction is active. Gated
     * to horizontal only (`!vertical`) - matches this whole feature's
     * TEXT-H-only scope, see cal_text_comp_cross's own header comment. The
     * external vxtSmartTextSetSkewComp() wrap that used to bracket the
     * whole calScreenText() call at its CAL_SCR_TEXTH call site is REMOVED
     * - this is the only place it may be set now. */
    int liveApply = (!vertical) && cal_text_apply_enabled;

    vxtSmartTextSetIntensity(CAL_INT_FIGURE);
    vxtSmartTextSetOrientation(vertical ? VXT_TEXT_ORIENT_CW
                                        : VXT_TEXT_ORIENT_HORIZ);

    for (r = 0; r < CAL_TEXT_ROWS; r++) {
        int32_t w = vxtSmartTextWidthPhys(CAL_TEXT_LEN[r]);   /* NOMINAL -
                                    * skew comp is guaranteed (0,0) here,
                                    * every time - see this function's own
                                    * fix comment above */
        int32_t perp = (int32_t)r * CAL_TEXT_GAP_PERP;
        int32_t oy, ox, ey, ex, inset;

        /* Right-hand regions are pinned at x=10000, not 12000. Uncorrected
         * text runs long by ~100 units per character on hardware, so a
         * 24-character row pinned at 12000 ended near x=14200 -
         * past the caret's own clamp (VXT_BOUNDS_HALF_X, 13500), where its
         * END mark could not be reached at all. */
        switch (variant) {
        case CAL_POS_TL: oy =  13000; ox = -12000; break;
        case CAL_POS_TR: oy =  13000; ox =  10000; break;
        case CAL_POS_BL: oy = -12000; ox = -12000; break;
        case CAL_POS_BR: oy = -12000; ox =  10000; break;
        default:         oy =   4000; ox =  -7000; break;
        }

        /* Stack the rows/columns AWAY from the nearest edge, so the third one
         * cannot walk off screen - at TR in vertical mode a naive +perp put
         * the last column at x=17,200 against a 13,500 half-width. */
        /* The END reference sits on the last glyph's own bottom-right
         * corner - a point that is drawn - not one gap past it
         * where the next character would begin, which is empty screen and
         * cannot be marked. Glyph strokes span x = 0..2 while each
         * character advances 4 (vxt_smart_text.c's VXT_TEXT_WIDTH +
         * VXT_TEXT_GAP), so the last glyph's right edge sits half an
         * advance short of `w`. The START reference is the text origin,
         * the first glyph's bottom-left corner.
         *
         * The last glyph has had one fewer per-character advance applied
         * than the character count the fits divide by; that ~1/len bias is
         * below the correction's own integer resolution and is left alone
         * rather than giving the rig and a game's Cal screen two different
         * divisors. */
        inset = w / ((int32_t)CAL_TEXT_LEN[r] * 2);

        if (vertical) {
            ox += rightSide ? -perp : perp;
            if (endAnchored) oy += w;       /* CW advance is -Y: runs DOWN */
            ey = oy - w + inset;  ex = ox;
        } else {
            oy += bottomSide ? perp : -perp;
            if (endAnchored) ox -= w;
            ey = oy;      ex = ox + w - inset;
        }

        /* Mark the START independently, so a mis-landed origin can be told
         * apart from drift accumulated ALONG the string - without this, the
         * two are indistinguishable and the reading is ambiguous.
         *
         * PERPENDICULAR to the string's own run: a cross put one arm straight down the line of text,
         * where it disappeared into the glyphs. */
        calTick(oy, ox, vertical ? 1 : 0, vertical ? 0 : 1,
                CAL_TICK_LEN / 2, CAL_INT_REF);

        if (liveApply) {
            vxtSmartTextSetSkewComp(cal_text_comp_cross, cal_text_comp_along);
        }
        vxtSmartTextBegin((int16_t)(oy / CAL_POS_SCALE),
                          (int16_t)(ox / CAL_POS_SCALE));
        calTextFill(buf, r);
        vxtSmartTextStr(buf);
        if (liveApply) {
            vxtSmartTextSetSkewComp(0, 0);   /* back to identity immediately -
                                    * the reference mark below, and the NEXT
                                    * row's own `w` query, must never see
                                    * live comp */
        }

        /* Reference at the IDEAL end, drawn independently AFTER the string so
         * it cannot inherit the string's own error. Perpendicular to the
         * string's run, same reasoning as the START mark above. */
        calTick(ey, ex, vertical ? 1 : 0, vertical ? 0 : 1,
                CAL_TICK_LEN / 2, CAL_INT_REF);

    }

    /* Leave the shared module back on the default - it is sticky, and any
     * game reading it would otherwise inherit whatever this screen last
     * set. */
    vxtSmartTextSetOrientation(VXT_TEXT_ORIENT_HORIZ);
}

/* ---------------------------------------------------------------------------
 * Screen 9 - PRIME: how many priming cycles are needed?
 *
 * Tests gamelibBeamPrime() against the effect that motivated it: the first
 * element drawn in a frame lands 2.5-4.8x further off than every later
 * element on the same screen.
 *
 * Four equal spokes from the origin, cardinal directions. **Ref 0 is the item
 * under test** - it is the first thing drawn this frame. Refs 1-3 are the
 * control group: identical geometry, but drawn after the beam has already
 * done real work, so they should show the ordinary ~3% shortfall regardless
 * of the priming setting.
 *
 * The variant IS the priming cycle count. So the experiment is: measure ref 0
 * at each variant and watch for the point where it stops being an outlier
 * against refs 1-3.
 *
 * PREDICTION (stated in advance so this can be wrong): at variant 0
 * ref 0 should read several hundred units while refs 1-3 read ~150-200; by
 * some small cycle count ref 0 should collapse into the same band. If ref 0
 * never improves, the first-element anomaly is not recenter settling and this
 * whole fix is the wrong tree - which is worth finding out in one screen.
 * ------------------------------------------------------------------------ */
static const uint8_t CAL_PRIME_CYCLES[5] = { 0, 1, 2, 4, 8 };
#define CAL_PRIME_N  4
#define CAL_PRIME_R  6000L
#define CAL_PRIME_SEG 2000L   /* short draw - the reposition is what is under
                               * test here, not the draw length */

/* ---------------------------------------------------------------------------
 * Screen 10 - REPOS. Distance-proportional or per-record?
 *
 * Added to settle a question two failed fixes could not.
 *
 * The first figure of a frame lands wrong, by an amount that grows with how
 * far its opening reposition goes: 205 units at zero travel, ~850 at 6000,
 * ~1250 at 10630. That looked proportional, so a 9% move gain was tried -
 * and did NOTHING on hardware.
 *
 * The reason is the discriminator. vxtSmartMoveBig() chains through
 * sm_chain_steps(), which emits ceil(|phys/scale|/100) RECORDS. Scaling the
 * distance by 0.91 leaves that count unchanged (2 stays 2, 3 stays 3), and
 * the error did not move either. Re-read against record count instead:
 *
 *     0 records -> 205      2 records -> ~850      3 records -> ~1250
 *
 * i.e. ~350 units per move record on a ~205 floor. Mechanically that is
 * plausible in a way the distance model never was: each SM_startMoveBig_d
 * carries hand-computed nop padding, so a slightly wrong pad contributes a
 * FIXED error once per record, no matter how far that record travels.
 *
 * The experiment. One figure only - it must be the first thing drawn to
 * carry the fault at all - repositioned a variant-selected distance, with an
 * independent mark at the reposition point itself (so the DRAW contributes
 * nothing to the reading). The distances straddle the record boundaries,
 * which at CAL_POS_SCALE 32 fall at phys 3200 / 6400 / 9600:
 *
 *     V0     0 -> 0 records        V4  6500 -> 3 records
 *     V1  3100 -> 1 record         V5  9500 -> 3 records
 *     V2  3300 -> 2 records        V6  9700 -> 4 records
 *     V3  6300 -> 2 records
 *
 * Distance-proportional predicts a smooth ramp across all seven.
 * Per-record predicts a STAIRCASE: V1~V2 apart despite only 200 units of
 * distance between them, V2~V3 together despite 3000 units apart, another
 * jump V4->V5->V6. The two models disagree loudly, which is the point.
 *
 * Corr is irrelevant here and should stay OFF - this measures raw behavior.
 * ------------------------------------------------------------------------ */
/* EXTENDED to SHORT distances. The 10x-step-size relation was only ever
 * measured from 3100 up; extrapolating it below that is what broke a real
 * game, whose first reposition of a frame is frequently a short one (a
 * reticle hash, a radar grid line). V1-V5 all sit inside a SINGLE step
 * (steps == 1 for anything under phys 3200), so within that block the
 * model predicts error rising in direct proportion to distance:
 *
 *     400 -> 120     1600 -> 500     3100 -> 960 (measured 1014, consistent)
 *     800 -> 250     2400 -> 750
 *
 * A straight ramp confirms the model down to the sizes a real game * uses. A
 * FLOOR instead - roughly constant a few hundred units regardless of distance
 * - means the model has a fixed term it never revealed at long range, and a
 * 24%-of-everything correction was always going to wreck short moves. */
static const int32_t CAL_REPOS_DIST[11] = {
    0, 400, 800, 1600, 2400,      /* NEW: all steps == 1, inside one record */
    3100, 3300, 6300, 6500, 9500, 9700   /* the original boundary straddle */
};
#define CAL_REPOS_SEG  1200L

static void calScreenRepos(uint8_t variant)
{
    int32_t d = CAL_REPOS_DIST[variant % 11];

    /* Mark the far end, not the reposition point. Changed after the first
     * time this screen was used on hardware.
     *
     * The reposition arrives BLANKED, so the reposition point itself has no
     * dwell dot - it is the dimmest, fuzziest point on the figure, and the
     * beam visibly curves there as it settles into the draw. Asking for a
     * caret on that end put the least placeable point in the rig on the one
     * reading that has to be precise.
     *
     * The far end carries the bright dwell dot, so mark that instead. The
     * reading then includes this segment's own draw error as well as the
     * reposition error - which does not matter here: CAL_REPOS_SEG is
     * identical across every variant, so the draw term is a constant offset
     * and cannot change the SHAPE of the curve, and shape is the entire
     * question (smooth ramp vs staircase). Trading a constant offset for a
     * placeable target is the right way round. */
    calLineAt(0, d, CAL_REPOS_SEG, 0, CAL_INT_FIGURE, CAL_BIG_SCALE);
    /* PERPENDICULAR tick: the
     * segment above is VERTICAL (dy=CAL_REPOS_SEG, dx=0), so a cross's
     * vertical arm ran straight down it. Passing the segment's own direction
     * puts the tick horizontal, across the end being measured. */
    calTick(CAL_REPOS_SEG, d, CAL_REPOS_SEG, 0, CAL_TICK_LEN, CAL_INT_REF);
}

static void calScreenPrime(uint8_t variant)
{
    int i;

    /* The priming under test. Runs BEFORE any geometry, which is the whole
     * point - gamelibBeamBegin() has already run in the handler. */
    gamelibBeamPrime(CAL_PRIME_CYCLES[variant % 5]);

    /* Redesigned: this screen could not see the defect it exists
     * to test.
     *
     * Every figure used to be drawn with calLineAt(0, 0, ...), i.e. starting
     * from a reposition to the ORIGIN. Measurement across the whole rig then
     * showed the first-element error depends almost entirely on how far that
     * first reposition TRAVELS: repositioning to (0,0) gives ~0-205 units of
     * error (ANGLE, DEFLECT), while repositioning to (0,6000) or (7000,-8000)
     * gives 690-2120 (CHAIN, SHARED, LADDER, SCALE). So the old layout tested
     * priming against the one case that has no error to begin with, and no
     * cycle count could ever have looked like an improvement.
     *
     * Each figure now repositions far first and draws a short segment back
     * toward the origin, which is the condition that provokes the
     * fault (the same shape SHARED's connectors and CHAIN's opening move
     * have). Ref 0 is the first drawn and is the item under test; refs 1-3
     * are identical geometry at other angles, drawn once the beam has already
     * done real work, and are the control group.
     *
     * The experiment: sweep the variant (priming cycles 0/1/2/4/8) and watch
     * whether ref 0's error collapses into the band refs 1-3 sit in. If it
     * does, priming is the fix and no per-figure compensation model is needed
     * anywhere - which would be a far better outcome than calibrating around
     * a fault we can prevent. If it never improves, the fault is not
     * integrator warm-up and compensation is the only route. */
    for (i = 0; i < CAL_PRIME_N; i++) {
        float a = (float)i * (6.283185307f / (float)CAL_PRIME_N);
        int32_t ay = (int32_t)(CAL_PRIME_R * sinf(a));   /* far anchor */
        int32_t ax = (int32_t)(CAL_PRIME_R * cosf(a));
        int32_t dy = -(int32_t)(CAL_PRIME_SEG * sinf(a));  /* back toward 0,0 */
        int32_t dx = -(int32_t)(CAL_PRIME_SEG * cosf(a));

        /* DELIBERATELY calLineAt(), not calLineAtComp() - PRIME measures the
         * RAW first-element error as a function of priming, so applying a
         * correction on top could only mask the effect under test. Corr has
         * no influence on this screen at all; the variant does. */
        calLineAt(ay, ax, dy, dx, CAL_INT_FIGURE, CAL_BIG_SCALE);
        calTick(ay + dy, ax + dx, dy, dx, CAL_TICK_LEN, CAL_INT_REF);
    }
}

/* ---------------------------------------------------------------------------
 * Screen 11 - ACCUM: per-record chain accumulation, ISOLATED from whole-figure
 * displacement.
 *
 * Chain already draws A closed polygon, but it compares
 * the chain's landing point against an independently repositioned mark, so
 * its reading is dominated by the ~900-unit first-element/whole-figure
 * displacement and cannot see per-segment accumulation underneath it. That is
 * visible in CHAIN's own numbers: 936/910/879/869/818 for N = 4/6/8/12/16, i.e.
 * flat-to-slightly-falling while the record count grows 4x. A per-record term
 * of even 30 units would have shown as +360 across that sweep and did not -
 * but it also could not have been distinguished from the displacement it is
 * buried in.
 *
 * The isolation trick: measure the chain's own start against the chain's OWN
 * END - both ends of a single chained run, with no independent reposition
 * anywhere in the comparison. A whole-figure displacement moves both by the
 * same amount and cancels exactly in the difference; only genuine
 * accumulation survives. Formally, with D = displacement and A = accumulation:
 *
 *     ref 0 (chain START) error = D
 *     ref 1 (chain END)   error = D + A
 *     ACCUMULATION        = (ref 1) - (ref 0)          <- subtract by hand
 *
 * This is the measurement an open arch-closure defect in a large closed-loop
 * model needs: its arch is ~17 chained segments that must land back on the
 * point a move established, and every non-accumulation explanation has
 * already been ruled out on hardware.
 *
 * The discriminator - the point of the variant layout. Two sweeps that a
 * fixed-per-record error and a length-proportional error answer OPPOSITELY:
 *
 *   V0-V3  record count sweep, N = 4/8/16/32 at fixed R - record count grows
 *          8x while path length stays within ~11% (an inscribed N-gon's
 *          perimeter is already near 2*pi*R by N=8).
 *            fixed-per-record  -> accumulation grows ~8x
 *            proportional      -> accumulation stays flat
 *
 *   V2,V4-V6  path length sweep, R = 2000/4000/6000/9000 at fixed N=16 -
 *          path length grows 4.5x at constant record count. (V2 is the shared
 *          corner of both sweeps: N=16, R=6000. Measure it once, use it in
 *          both columns.)
 *            fixed-per-record  -> accumulation stays flat
 *            proportional      -> accumulation grows ~4.5x
 *
 * Both growing means both terms are real and the fit needs both. Neither
 * growing means chain accumulation is not the arch's mechanism, which is
 * an equally useful result - it would send that investigation back to a
 * move-vs-draw gain disagreement instead.
 *
 * Deliberately raw - no calApplyGeomComp(), and Corr does nothing here, same
 * choice PRIME makes and for a sharper reason: the correction this rig can
 * apply is a whole-figure TRANSLATION, and a translation shifts ref 0 and
 * ref 1 EQUALLY, so it cancels in the very difference being measured. Wiring
 * comp in could not change the reading - it would only make the screen look
 * correctable when it is not. Record raw numbers here and interpret them; do
 * not try to converge this screen to zero.
 * ------------------------------------------------------------------------ */
#define CAL_ACCUM_MAX_N  32
static const uint8_t  CAL_ACCUM_N[7] = {  4,    8,   16,   32,   16,   16,   16 };
static const int32_t  CAL_ACCUM_R[7] = { 6000, 6000, 6000, 6000, 2000, 4000, 9000 };

/* The measuring pads. Redesigned twice, both times on hardware
 * feedback, and the second failure is worth keeping because the data proves
 * it rather than describing it.
 *
 * V1 registered both refs at vertex 0 and drew a CAL_CROSS_ARM anchor cross
 * on top of it: the cross covered the very point being read.
 *
 * V2 added a lead-in stub outside the circle and a lead-out stub inside it,
 * both RADIAL - which put them on the same line pointing the same way, so
 * together they drew one continuous horizontal line straight THROUGH the
 * vertex. There was no distinguishable "outer tip" and "inner tip"; there was
 * a line with the polygon's corner in the middle of it, and the corner is the
 * brightest feature there. The logged session shows exactly that: A = R1 - R0
 * came out 3033/3049/2884/2708/2760/2924/2802 against a pad separation of
 * exactly 3000 - the signature of measuring ONE point against two refs, since
 * then A collapses to (ref0 - ref1). Both readings sat at ~6820 for the
 * R=6000 variants, i.e. on the vertex (ideal 6000 + the ~800 displacement),
 * not on either tip.
 *
 * V3 put the tips on OPPOSITE 45-degree corners - (+PAD, r+PAD) and
 * (-PAD, r-PAD) - which is the same mistake rotated 45 DEGREES, and hardware
 * said so immediately ("for V0, the R0/1 are about the same spot"). Opposite
 * corners of one diagonal are COLLINEAR: the lead-in arrives heading
 * down-left and the lead-out departs heading down-left, so it drew one
 * straight line through the corner exactly as V2 did. "Opposite" is not the
 * same property as "not parallel", and only the second one matters here.
 *
 * V4 (this one) is a symmetric V - an arrowhead pointing inward at the
 * vertex, with both arms OUTSIDE the circle, 30 degrees either side of the
 * radial axis:
 *
 *      lead-in  tip -> (+PAD_T, r + PAD_R)   upper arm, outside
 *      lead-out tip -> (-PAD_T, r + PAD_R)   lower arm, outside
 *
 * Now checked rather than eyeballed, which is how the last two versions
 * shipped broken:
 *  - Not parallel: lead-in direction (-1250,-2165), lead-out (-1250,+2165),
 *    cross product -5412500, so they cannot draw as one line.
 *  - 2500 apart in y, mirror images about the radial axis, so "upper arm" and
 *    "lower arm" are unambiguous to read.
 *  - Clear of every edge, for every N - and this is the strong property. Both
 *    polygon edges meeting vertex 0 extend INWARD: their radial component is
 *    -6000/-1757/-457/-115 for N=4/8/16/32, negative at every N. The stubs
 *    extend OUTWARD (+2165). Edges and stubs therefore occupy opposite
 *    half-planes, and the two stub tips are the only geometry outside the
 *    circle anywhere near that vertex. (V2's radial stubs failed this by
 *    being collinear with each other, not by touching an edge; V3's
 *    45-degree stubs would additionally have lain along the polygon's own
 *    edges at N=4, where those edges sit at exactly 45 degrees.)
 *
 * Tip x reaches r+2165, i.e. 11165 at the largest radius - inside
 * VXT_BOUNDS_HALF_X (13500). No anchor cross is drawn; the stubs ARE the
 * guides. */
#define CAL_ACCUM_PAD_T  1250L   /* tangential component (2500*sin 30) */
#define CAL_ACCUM_PAD_R  2165L   /* radial    component (2500*cos 30) */

/* One chained step of the polygon, tracking the ACHIEVED delta - a verbatim
 * copy of the loop body in the game renderer this screen was built to
 * diagnose, deliberately, because that is the code path whose defect this
 * screen exists to measure. It
 * targets each vertex rounded to CAL_POS_SCALE and accumulates what was
 * really drawn, exactly as that renderer does - not
 * gamelibChainBigClosedPerimeter(), which targets raw coordinates for its
 * intermediate vertices and so is a different path from the one under test. */
static void calAccumStep(int32_t ty, int32_t tx, int32_t *curY, int32_t *curX)
{
    int32_t dy = gamelibRoundToScale(ty, CAL_POS_SCALE) - *curY;
    int32_t dx = gamelibRoundToScale(tx, CAL_POS_SCALE) - *curX;
    int32_t achievedDy, achievedDx;

    gamelibDispatchAutoDraw(dy, dx, CAL_BIG_SCALE, &achievedDy, &achievedDx);
    *curY += achievedDy;
    *curX += achievedDx;
}

static void calScreenAccum(uint8_t variant)
{
    int v = variant % 7;
    int n = (int)CAL_ACCUM_N[v];
    int32_t r = CAL_ACCUM_R[v];
    /* Symmetric V, both arms OUTSIDE the circle - see CAL_ACCUM_PAD_T's note.
     * Same radial component, MIRRORED tangential component: that mirroring is
     * what makes the two stubs non-parallel, which the two previous versions
     * both got wrong. */
    int32_t padInY  =  CAL_ACCUM_PAD_T, padInX  = r + CAL_ACCUM_PAD_R; /* upper arm */
    int32_t padOutY = -CAL_ACCUM_PAD_T, padOutX = r + CAL_ACCUM_PAD_R; /* lower arm */
    int32_t curY, curX, vy = 0, vx = 0;
    int i;

    /* Registered BEFORE the draw so their numbering is deterministic. */
    calRef(padInY,  padInX);    /* ref 0 - lead-IN  tip: displacement only     */
    calRef(padOutY, padOutX);   /* ref 1 - lead-OUT tip: displacement + accum  */

    /* Corr now works here. Added - it was left out when this
     * screen was written, on the reasoning that the rig's correction is a
     * whole-figure TRANSLATION and a translation shifts both tips equally, so
     * it cancels in A = R1 - R0 and could not change the answer.
     *
     * That arithmetic is right and the conclusion was wrong, because it is
     * not what Corr is FOR. Its value is iterative refinement: null the
     * reading, re-measure the residual, converge. On this screen that pays
     * twice over -
     *   - translate the figure so R=0 reads ~0, and R=1 then reads A
     *     DIRECTLY, off a single caret placement instead of a difference of
     *     two. That roughly halves the placement error, which matters here:
     *     A runs 5..600 units while a caret placement is worth about +-50.
     *   - and it can be ITERATED, which is exactly the "cure rough
     *     measurements" the mechanism exists for.
     *
     * Anchored on ref 0 (figure index 0) deliberately: nulling the lead-in
     * tip is what makes the lead-out tip read as the accumulation. */
    {
        int32_t cy = 0, cx = 0;
        calApplyGeomComp(0, &cy, &cx);
        padInY  += cy; padInX  += cx;
        padOutY += cy; padOutX += cx;
        vy = cy; vx = cx;   /* vertex and arc targets shift with the figure */
    }

    vxtSmartIntensity(CAL_INT_FIGURE);

    /* ONE reposition for the whole figure - everything after this is a single
     * chained run, which is what makes the displacement common to both pads
     * and therefore cancellable. */
    gamelibRepositionAbs(padInY, padInX);
    curY = gamelibRoundToScale(padInY, CAL_POS_SCALE);
    curX = gamelibRoundToScale(padInX, CAL_POS_SCALE);

    /* lead-in: pad -> vertex 0 */
    calAccumStep(vy, r + vx, &curY, &curX);

    /* the polygon itself, vertex 1..n-1 then back onto vertex 0 */
    for (i = 1; i <= n; i++) {
        int ti = i % n;
        float a = (float)ti * (6.283185307f / (float)n);
        calAccumStep(vy + (int32_t)(r * sinf(a)), vx + (int32_t)(r * cosf(a)), &curY, &curX);
    }

    /* lead-out: vertex 0 -> the MIRRORED arm. Same LENGTH as the lead-in (so
     * the pair's own error contribution is a constant identical in every
     * variant - which is why this screen is read as a SWEEP rather than as one
     * absolute number), but mirrored across the radial axis, which is what
     * makes the two arms non-parallel and the two tips impossible to confuse. */
    calAccumStep(padOutY, padOutX, &curY, &curX);

    gamelibBeamCloseRun();

}

/* ---------------------------------------------------------------------------
 * Screen 12 - CHORD: a real closed-arc topology, reproduced exactly. The
 * question that motivated it: known starting point (x,y) -> long line to
 * new point (x1,y1) -> multipoint arc back to original starting point
 * (x,y). Why does the return arc overshoot the starting point, ending at
 * an x further to the LEFT (more negative) than the original x value?
 *
 * Why accum could not answer this. ACCUM draws an EQUILATERAL polygon - every
 * segment the same length, so every segment takes the same dispatch path at
 * roughly the same scale. It measured that a uniform chain closes on itself
 * to within ~64 units at the model's own regime, which is far too small to
 * explain the photographed gap. The real arch is not uniform: it is ONE
 * Very long chord (~2.6x the model's own radius, forced onto the Huge path
 * at scale ~158) followed by fifteen SHORT arc segments at a much lower
 * scale. Mixed scales inside one chained run is the difference between the
 * two figures, and it is the last untested candidate - the other three
 * have already been ruled out in the game's own renderer.
 *
 * Also independently established: the model's point data is correct. It
 * plots from source data and draws the intended shape, so the defect is
 * entirely in the drawing path - which is what this screen probes.
 *
 * The figure - a capital D, the top half of the model with the detail
 * stripped away:
 *
 *      P0 = (0, -R)   left end of the horizontal diameter   <- start AND end
 *      P1 = (0, +R)   right end
 *      chord  P0 -> P1 across the full diameter (length 2R, the LONG one)
 *      arc    P1 -> ... -> P0 over the top, N equal segments
 *
 * Same two-tip measuring trick as ACCUM (see CAL_ACCUM_PAD_T): a mirrored V
 * outside the circle at P0, so the chain's START and its arc's end each get an
 * isolated, unmistakable landing point and the whole-figure displacement
 * cancels in the difference.
 *
 *     A = (R=1 reading) - (R=0 reading)      <- subtract by hand, per axis
 *
 * Directly comparable with accum: identical variant layout, identical radii,
 * identical stubs. The only difference is that one edge is the long chord
 * instead of another equal arc segment. So:
 *
 *     CHORD's A  ~=  ACCUM's A   -> the long segment is innocent; the mixed
 *                                   scale hypothesis is dead too, and the
 *                                   defect is somewhere else again
 *     CHORD's A  >>  ACCUM's A   -> found it. A long segment chained among
 *                                   short ones is the mechanism, and the fix
 *                                   belongs in the dispatcher's scale handling
 *                                   (not in more recenters)
 *
 * Expected sign, so the reading can be checked against the report: the arc
 * should end further LEFT (more negative x) than it started, i.e. A_dx
 * NEGATIVE. If it comes out positive on this unit, say so - that is a real
 * difference from the reporting machine, not a mistake.
 *
 * Deliberately raw - Corr does nothing here, same reasoning as ACCUM: the
 * rig's correction is a whole-figure translation, and a translation shifts
 * both tips equally, so it cancels in the very difference being measured.
 * ------------------------------------------------------------------------ */
#define CAL_CHORD_MAX_N  32
static const uint8_t CAL_CHORD_N[7] = {   4,    8,   16,   32,   16,   16,   16 };
static const int32_t CAL_CHORD_R[7] = { 6000, 6000, 6000, 6000, 2000, 4000, 9000 };

static void calScreenChord(uint8_t variant)
{
    int v = variant % 7;
    int n = (int)CAL_CHORD_N[v];
    int32_t r = CAL_CHORD_R[v];
    /* Stubs mirror ACCUM's, but P0 sits at x = -R, so "radially outward" is
     * More negative x - hence the minus on the radial term. */
    int32_t padInY  =  CAL_ACCUM_PAD_T, padInX  = -r - CAL_ACCUM_PAD_R;
    int32_t padOutY = -CAL_ACCUM_PAD_T, padOutX = -r - CAL_ACCUM_PAD_R;
    int32_t curY, curX, vy = 0, vx = 0;
    int i;

    calRef(padInY,  padInX);    /* ref 0 - UPPER tip: displacement only      */
    calRef(padOutY, padOutX);   /* ref 1 - LOWER tip: displacement + accum   */

    /* Whole-figure translation anchored on ref 0 - see calScreenAccum()'s own
     * note for why Corr matters on a difference-measuring screen even though
     * a translation cancels in the difference. Same reasoning, same anchor. */
    {
        int32_t cy = 0, cx = 0;
        calApplyGeomComp(0, &cy, &cx);
        padInY  += cy; padInX  += cx;
        padOutY += cy; padOutX += cx;
        vy = cy; vx = cx;
    }

    vxtSmartIntensity(CAL_INT_FIGURE);

    /* ONE reposition for the whole figure - everything after is a single
     * chained run, which is what makes the displacement common to both tips. */
    gamelibRepositionAbs(padInY, padInX);
    curY = gamelibRoundToScale(padInY, CAL_POS_SCALE);
    curX = gamelibRoundToScale(padInX, CAL_POS_SCALE);

    calAccumStep(vy, vx - r, &curY, &curX);   /* lead-in stub -> P0          */
    calAccumStep(vy, vx + r, &curY, &curX);   /* The long chord, P0 -> P1    */

    /* The arc back: P1 (angle 0) over the top to P0 (angle 180), N segments.
     * i runs to N so the final point is exactly P0 - the same "close onto a
     * point the figure already visited" that the real arch does. */
    for (i = 1; i <= n; i++) {
        float a = (float)i * (3.14159265f / (float)n);
        calAccumStep(vy + (int32_t)(r * sinf(a)), vx + (int32_t)(r * cosf(a)),
                     &curY, &curX);
    }

    calAccumStep(padOutY, padOutX, &curY, &curX);   /* P0 -> lead-out stub   */

    gamelibBeamCloseRun();
}

/* ---------------------------------------------------------------------------
 * Screen 13 - HOP : a HUD-meter tick column,
 * drawn the way a game draws a hopping meter, with NO re-anchoring - one reposition, then per tick a pure-vertical blanked
 * MoveBig hop at CAL_POS_SCALE and a serpentine tick at the accurate dispatch.
 * Geometry and the variant table come from vxt_cal_load.h's VXT_CAL_HOP_*, so
 * this screen and vxtCalLoadHopDrift() cannot disagree about what was drawn.
 *
 *   V0..V3 = 4, 6, 8, 15 ticks: Scene 1 levels 1 / 2 / 3-4, and Scene 2.
 *
 * Read it like accum: ref 0 is the LEFT end of the BOTTOM tick, ref 1 the LEFT
 * end of the TOP tick. Their difference is (ticks - 1) hops of drift with the
 * whole-figure displacement cancelled; the loader divides. Corr works as on
 * ACCUM - it translates the whole column by ref 0's correction, so once ref 0
 * reads ~0, ref 1 reads the accumulated drift directly.
 *
 * Deliberately not gamelibBeamCloseRun()-separated per tick, and no re-anchor:
 * the point is to measure the uninterrupted chain the game wants to draw.
 * ------------------------------------------------------------------------ */
static void calScreenHop(uint8_t variant)
{
    static const uint8_t kTicks[VXT_CAL_HOP_NVAR] = VXT_CAL_HOP_TICKS;
    int v = variant % VXT_CAL_HOP_NVAR;
    int n = (int)kTicks[v];
    int32_t segH = VXT_CAL_HOP_HEIGHT / n;
    int32_t left = VXT_CAL_HOP_BAR_X - VXT_CAL_HOP_HALF_W;
    int32_t cy = 0, cx = 0, tY = 0, tX = 0;
    int i;

    /* Registered BEFORE the draw, at the IDEAL (uncorrected) positions. */
    calRef(VXT_CAL_HOP_BASE_Y + segH / 2, left);                         /* ref 0 */
    calRef(VXT_CAL_HOP_BASE_Y + (int32_t)(n - 1) * segH + segH / 2, left); /* ref 1 */

    calApplyGeomComp(0, &cy, &cx);   /* whole-column translation, ref 0 anchor */

    vxtSmartIntensity(CAL_INT_FIGURE);
    for (i = 0; i < n; i++) {
        int32_t y   = VXT_CAL_HOP_BASE_Y + (int32_t)i * segH + segH / 2 + cy;
        int32_t sx  = ((i & 1) ? VXT_CAL_HOP_BAR_X + VXT_CAL_HOP_HALF_W
                               : VXT_CAL_HOP_BAR_X - VXT_CAL_HOP_HALF_W) + cx;
        int32_t len = (i & 1) ? -2 * VXT_CAL_HOP_HALF_W : 2 * VXT_CAL_HOP_HALF_W;
        int32_t oy, ox;
        if (i == 0) {
            gamelibRepositionAbs(y, sx);
            tY = gamelibRoundToScale(y, CAL_POS_SCALE);
            tX = gamelibRoundToScale(sx, CAL_POS_SCALE);
        } else {
            /* a blanked big jump to (y, tX) at BIG_DRAW_SCALE */
            int32_t jy = y - tY;
            vxtSmartScale(CAL_POS_SCALE);
            vxtSmartMoveBig(jy, 0, CAL_POS_SCALE);
            vxtSmartScale(CAL_BIG_SCALE);
            tY += gamelibRoundToScale(jy, CAL_POS_SCALE);
        }
        gamelibDispatchAutoDraw(0, len, GAMELIB_SCALE_ACCURATE, &oy, &ox);
        tY += oy; tX += ox;
    }
    (void)tX;

    gamelibBeamCloseRun();
}

/* ---------------------------------------------------------------------------
 * Measuring caret.
 *
 * The STM32 cannot see the screen, so the user is the sensor: park the caret
 * on where the beam landed and read the offset from nominal. Motion
 * is QUADRATIC in stick deflection - a linear map good enough for a game
 * cursor is far too coarse to place a caret to within the few tens of units
 * these errors measure (Sec.72's worst gap was 61 units).
 * ------------------------------------------------------------------------ */
#define CAL_CARET_DEADZONE   8
#define CAL_CARET_MAX_STEP   90L

static int32_t calCaretStep(int8_t v)
{
    int32_t a = (v < 0) ? -(int32_t)v : (int32_t)v;
    int32_t step;

    if (a <= CAL_CARET_DEADZONE) return 0;
    a -= CAL_CARET_DEADZONE;
    step = (a * a * CAL_CARET_MAX_STEP) / ((127 - CAL_CARET_DEADZONE) *
                                           (127 - CAL_CARET_DEADZONE));
    if (step == 0) step = 1;   /* full fine control: one unit per frame */
    return (v < 0) ? -step : step;
}

/* A box around the reference the readout is currently reporting against.
 * Added: recording a measurement is worthless if the log cannot say
 * WHICH element it measured, and telling the user "R=3" without showing them
 * which one that is just moves the ambiguity. Drawn as a closed perimeter
 * (~14 records) rather than four independent lines (~48). */
#define CAL_SEL_HALF  700L

static void calDrawSelected(int32_t y, int32_t x)
{
    int32_t py[4], px[4];

    py[0] = y - CAL_SEL_HALF; px[0] = x - CAL_SEL_HALF;
    py[1] = y - CAL_SEL_HALF; px[1] = x + CAL_SEL_HALF;
    py[2] = y + CAL_SEL_HALF; px[2] = x + CAL_SEL_HALF;
    py[3] = y + CAL_SEL_HALF; px[3] = x - CAL_SEL_HALF;
    gamelibChainBigClosedPerimeter(py, px, 4, CAL_BIG_SCALE, CAL_INT_REF);
}

/* Added to make the "caret was never * moved off the reference" mistake impossible to make by accident.
 *
 * TWO things are drawn, both cheap:
 *  1. A LEASH from the current reference to the caret. Its LENGTH is the
 *     measurement. A caret still sitting on its auto-snapped start position
 *     draws no leash at all, which is unmistakable - previously that state
 *     looked identical to a genuine, correctly-measured zero, and that
 *     ambiguity cost a whole hardware session (a 40-unit correction was
 *     fitted to what turned out to be an ~800-unit gap).
 *  2. A short tick at every ALREADY-MEASURED reference's recorded landing
 *     point, reconstructed from that ref's stored correction (marked point =
 *     ref - comp, since comp was stored as -(caret - ref)). So "where did I
 *     say this one landed" is visible for every item at once, and a
 *     misplaced earlier reading can be spotted and re-taken instead of
 *     quietly poisoning the correction set.
 *
 * Geometry screens only - TEXT H/V use the separate per-character text comp,
 * which these marks would misrepresent, and they are also the two screens
 * with no record budget to spare (see calScreenText()'s own header). */
#define CAL_MEAS_TICK  350L

static void calDrawMeasureAids(void)
{
    int k;

    if (cal_screen == CAL_SCR_TEXTH || cal_screen == CAL_SCR_TEXTV) return;

    for (k = 0; k < cal_ref_n && k < CAL_MAX_REFS; k++) {
        int32_t my, mx;
        if (!cal_geom_comp_set[k]) continue;
        my = cal_ref_y[k] - cal_geom_comp_dy[k];
        mx = cal_ref_x[k] - cal_geom_comp_dx[k];
        /* One short diagonal - visually distinct from every axis-aligned
         * cross/tick already on screen, and one line rather than two keeps
         * this affordable on ANGLE (12 refs). */
        calLineAt(my - CAL_MEAS_TICK, mx - CAL_MEAS_TICK,
                  2 * CAL_MEAS_TICK, 2 * CAL_MEAS_TICK,
                  CAL_INT_REF, CAL_BIG_SCALE);
    }

    if (cal_ref_n > 0 && cal_sel < cal_ref_n) {
        int32_t dy = cal_caret_y - cal_ref_y[cal_sel];
        int32_t dx = cal_caret_x - cal_ref_x[cal_sel];
        if (dy || dx) {
            calLineAt(cal_ref_y[cal_sel], cal_ref_x[cal_sel], dy, dx,
                      CAL_INT_GRID, CAL_BIG_SCALE);
        }
    }
}

/* Seeds this screen+variant's per-reference corrections from the measurement
 * log (cal_meas[], loaded from /calmeas.csv at boot), so a previous session's
 * work is picked up and Corr ON works immediately without re-marking.
 *
 * must run AFTER the screen has drawn, because it matches on cal_ref_n - the
 * number of references the screen registered THIS frame - against
 * the count stored with each row. That match is what makes a row
 * self-invalidating when a screen's reference layout changes; see
 * CalMeas.nrefs for the real bug that motivated it. Seeding one frame late is
 * imperceptible and is the cost of having cal_ref_n be a measured fact rather
 * than a hand-maintained per-screen constant that could itself go stale. */
static void calSeedGeomComp(void)
{
    int k;

    if (!cal_geom_seed_pending) return;
    cal_geom_seed_pending = 0;
    if (cal_ref_n <= 0) return;

    for (k = 0; k < (int)CAL_REFS[cal_screen]; k++) {
        const CalMeas *m = calMeasSlot(cal_screen, cal_variant[cal_screen], k);
        int target = -1;

        if (!m || !m->valid) continue;   /* never measured - nothing to seed */

        /* WHICH reference does this row belong to?
         *
         * Simplified by the dense slot table. The row's reference
         * NUMBER is now the slot's own address, so it can never be wrong and
         * the old "exact layout match vs coordinate match" fallback pair is
         * gone. What remains is the check that still matters: has this
         * screen's reference LAYOUT moved since the row was taken? The row
         * stores the ideal coordinate it was measured against, so if that no
         * longer matches where reference k sits, the row describes
         * different geometry and is skipped rather than misapplied.
         *
         * This is the durable half of an earlier SHARED incident (adding
         * the ring renumbered six connectors and every correction went to the
         * wrong figure). Renumbering is now structurally impossible; a moved
         * reference is still caught here.
         *
         * Net effect, and the point: a screen you have already measured stays
         * measured across sessions and across builds. You should never have
         * to re-mark an entry that is already in the log. */
        if (k < cal_ref_n && cal_ref_y[k] == (int32_t)m->refY
                          && cal_ref_x[k] == (int32_t)m->refX) {
            target = k;
        } else {
            /* Coordinate-match fallback, restored - the slot-table
             * rewrite deleted it and replaced it with a bare `continue`,
             * which is why Corr stopped picking up prior work on so many
             * screens: any shift in a screen's reference layout * dropped every stored correction for it instead of following it.
             * The card overlay registering itself as reference 0 shifted
             * every card-on variant by one, so on those it dropped
             * EVERYTHING.
             *
             * A reference's own nominal COORDINATE is a far more durable key
             * than its index, which is the lesson of the earlier SHARED
             * incident (adding the ring renumbered six connectors by +1;
             * matching on coordinates follows them correctly, matching on
             * index feeds every correction to the wrong figure). Applied only
             * when exactly ONE current reference has those coordinates - if
             * the layout became ambiguous, skip rather than guess.
             *
             * Net effect, and the point: a screen you have already measured
             * stays measured across sessions AND across builds. You should
             * never have to re-mark an entry that is already in the log. */
            int j, hits = 0;
            for (j = 0; j < cal_ref_n && j < CAL_MAX_REFS; j++) {
                if (cal_ref_y[j] == (int32_t)m->refY &&
                    cal_ref_x[j] == (int32_t)m->refX) { target = j; hits++; }
            }
            if (hits != 1) continue;
        }

        if (target < 0 || target >= CAL_MAX_REFS) continue;
        cal_geom_comp_dy[target]  = (int16_t)(m->compY - m->dy);
        cal_geom_comp_dx[target]  = (int16_t)(m->compX - m->dx);
        cal_geom_comp_set[target] = 1;
    }
}

/* TEXT H's own seeding. Its correction is a different quantity from the
 * geometry screens' - ONE global pair, in PER-CHARACTER units - so it cannot
 * share calSeedGeomComp()'s per-reference machinery, but the same rule
 * applies: a screen already measured should not need re-marking.
 *
 * The stored total is always (logged comp + this row's own per-character
 * contribution), identical in form to the live RECORD fold-in, so a row is
 * usable whether it was taken raw or as a residual.
 *
 * Seeds from the LONGEST measured string available, not the most recent: the
 * per-character rate is error/characters, so a 24-character row divides its
 * measurement error by three times as much as an 8-character one and is the
 * better estimator. Rows are END references only (odd ref - only those have a
 * known character count), matching the fold-in's own rule.
 *
 * Runs only when the correction is still at identity, so it never overwrites
 * work in progress - and deliberately does not reset per variant, so a
 * correction fitted at one screen position can still be carried to another to
 * test whether it generalises, which is what TEXT H's five positions exist
 * for. */
static void calTextRunError(uint8_t variant, int endRef, int32_t dy, int32_t dx,
                            int32_t *runDy, int32_t *runDx)
{
    const CalMeas *st = calMeasSlot(CAL_SCR_TEXTH, variant, endRef - 1);

    *runDy = dy;
    *runDx = dx;
    if (st && st->valid) {
        *runDy -= st->dy;
        *runDx -= st->dx;
    }
}

static void calSeedTextComp(void)
{
    int k, bestLen = 0;
    float bestCross = 0.0f, bestAlong = 0.0f;

    if (cal_text_comp_cross || cal_text_comp_along) return;   /* in progress */

    for (k = 0; k < (int)CAL_REFS[CAL_SCR_TEXTH]; k++) {
        const CalMeas *m = calMeasSlot(CAL_SCR_TEXTH, cal_variant[cal_screen], k);
        int row, len;

        if (!m || !m->valid)                       continue;
        if (!(k & 1))                              continue;   /* END refs only */
        row = k / 2;
        if (row < 0 || row >= CAL_TEXT_ROWS)       continue;
        len = CAL_TEXT_LEN[row];
        if (len <= bestLen)                        continue;

        {
            int32_t runDy, runDx;
            calTextRunError(cal_variant[cal_screen], k, m->dy, m->dx, &runDy, &runDx);
            bestLen   = len;
            bestCross = (float)m->compY
                      - (float)runDy / (float)len / (float)CAL_TEXT_COMP_SCALE_DIV;
            bestAlong = (float)m->compX
                      - (float)runDx / (float)len / (float)CAL_TEXT_COMP_SCALE_DIV;
        }
    }

    if (bestLen > 0) {
        cal_text_comp_cross = calClampComp(bestCross);
        cal_text_comp_along = calClampComp(bestAlong);
    }
}

/* Reverted to the original PLUS. The 45-degree X was a
 * misreading of the request - the caret was never the thing complained
 * about. Left as a plain +, as it has always been. */
static void calDrawCaret(void)
{
    calLineAt(cal_caret_y, cal_caret_x - CAL_CARET_ARM,
              0, 2 * CAL_CARET_ARM, CAL_INT_CARET, CAL_BIG_SCALE);
    calLineAt(cal_caret_y - CAL_CARET_ARM, cal_caret_x,
              2 * CAL_CARET_ARM, 0, CAL_INT_CARET, CAL_BIG_SCALE);
}

/* ---------------------------------------------------------------------------
 * Readout
 * ------------------------------------------------------------------------ */
#define CAL_TEXT_LEFT   (-12000L)
#define CAL_TEXT_ROW1   (-13000L)
#define CAL_TEXT_ROW2   (-14600L)
#define CAL_TEXT_ROW3   (-16200L)   /* CAL_TEXT_ROW4 (-17800L) REMOVED
                                    * Round 2 - see ROW1's own
                                    * Corr on/OFF comment in
                                    * calDrawReadout() for why: folded onto
                                    * ROW1 instead of its own row, both to
                                    * show on every screen and to avoid the
                                    * record-budget/overscan risk a 4th row
                                    * carried on TEXT H specifically */

/* ---------------------------------------------------------------------------
 * Writing the log.
 *
 * Rewrites the whole file on every RECORD from the in-RAM cal_meas[]
 * working set - see calSaveLog()'s own comment, further down, for why
 * this (not append-only) is the correct design: a calibration value must
 * be exactly ONE number per (screen,variant,ref), never a history to
 * disambiguate later. cal_meas[] is already deduplicated by
 * calMeasUpsert(), so this rewrite is cheap and never loses distinct
 * data - it's always writing out exactly what's currently the best-known
 * value for everything measured so far.
 *
 * One row is built at a time on the stack; no multi-kilobyte buffer in bss
 * (convention 16 - bss comes straight out of the stack, and this module has no
 * business claiming 3 KB for string formatting).
 * ------------------------------------------------------------------------ */
static int calAppendInt(char *b, int i, int32_t v)
{
    char t[8];
    int k = 0;
    uint32_t m;

    if (v < 0) { b[i++] = '-'; m = (uint32_t)(-v); } else m = (uint32_t)v;
    do { t[k++] = (char)('0' + (m % 10u)); m /= 10u; } while (m);
    while (k) b[i++] = t[--k];
    return i;
}

static int calAppendStr(char *b, int i, const char *s)
{
    while (*s) b[i++] = *s++;
    return i;
}

/* Builds one CSV row from `m` into `line`, returns its length. */
/* Changed for the dense slot table: screen/variant/ref/card are no
 * longer STORED in the row (the slot's address is what they are), so the
 * caller passes them in. `card` is derived, not stored - it is whether
 * the variant index is in the second, card-ON half of the sequence.
 *
 * Column order is append-only. vxt_cal_load.c parses TEXT H rows by position,
 * so the twelve original columns keep their exact places and everything new
 * goes on the end; an older reader ignores the tail, and a newer reader gets 0
 * from calParseField() for a legacy row that stops early. */
static int calBuildLogLine(const CalMeas *m, int screen, int variant, int ref,
                           char *line)
{
    int i = 0;
    i = calAppendStr(line, i, CAL_SCREEN_NAME[screen]);
    line[i++] = ',';  i = calAppendInt(line, i, variant);
    line[i++] = ',';  i = calAppendInt(line, i, ref);
    line[i++] = ',';  i = calAppendInt(line, i, CAL_CARD_ON(screen, variant) ? 1 : 0);
    line[i++] = ',';  i = calAppendInt(line, i, m->refY);
    line[i++] = ',';  i = calAppendInt(line, i, m->refX);
    line[i++] = ',';  i = calAppendInt(line, i, m->dy);
    line[i++] = ',';  i = calAppendInt(line, i, m->dx);
    line[i++] = ',';  i = calAppendInt(line, i, (int32_t)m->n);
    line[i++] = ',';  i = calAppendInt(line, i, m->compY);
    line[i++] = ',';  i = calAppendInt(line, i, m->compX);
    line[i++] = ',';  i = calAppendInt(line, i, (int32_t)CAL_REFS[screen]);
    /* --- what makes a row self-describing --- */
    line[i++] = ',';  i = calAppendInt(line, i, m->drawGain);
    line[i++] = ',';  i = calAppendInt(line, i, m->moveGain);
    line[i++] = ',';  i = calAppendInt(line, i, m->moveSettle);
    line[i++] = ',';  i = calAppendInt(line, i, (int32_t)m->session);
    line[i++] = ',';  i = calAppendInt(line, i, (int32_t)m->fwStamp);
    line[i++] = ',';  i = calAppendInt(line, i, calScreenMethod(screen));
    line[i++] = '\n';
    return i;
}

/* Reverted (round 4): round 3's
 * append-only calAppendLogRow() was a real mistake, not just a size
 * tradeoff - re-recording the same (screen,variant,ref) appended a SECOND
 * line for it rather than replacing the first, so the file itself could
 * hold multiple, ambiguous readings for one calibration target with no
 * marked "this one is current." A calibration value has to be exactly
 * ONE number per parameter, never a history to disambiguate at apply
 * time. Back to a full rewrite - but unlike round 1's version, this is
 * now SAFE and no longer wasteful, because cal_meas[] is already
 * deduplicated by calMeasUpsert() (round 3): re-measuring something
 * updates its ONE existing slot in place, it does not grow the array. So
 * the file this writes is always in EXACT 1:1 correspondence with the
 * in-RAM working set - at most one row per distinct
 * (screen,variant,ref), always the latest value, never ambiguous. Still
 * capped at CAL_MAX_MEAS (64) DISTINCT calibration targets at once - see
 * that constant's own comment - not 64 measurements total. */
/* Set by calLoadLog() when the log could not be READ for a reason other than
 * "no such file" (a busy card, the Mac mounting it). calSaveLog() rewrites the
 * whole file from the in-RAM table with FA_CREATE_ALWAYS, so if the boot load
 * failed the table is empty and the next RECORD would replace a real
 * measurement log with one row. While this is set, saves are refused. Cleared
 * by the next successful calLoadLog() (every 6809 boot). */
static int cal_log_unsafe = 0;

static void calSaveLog(void)
{
    FIL f;
    UINT bw;
    int scr, v, r;

    if (cal_log_unsafe) return;   /* see cal_log_unsafe above - never clobber
                                   * a log we could not read */

    if (f_open(&f, VXT_CAL_RIG_FILE, FA_CREATE_ALWAYS | FA_WRITE) != FR_OK) return;

    {
        static const char hdr[] = "screen,variant,ref,card,refY,refX,dy,dx,records,"
                                  "compy,compx,nrefs,drawgain,movegain,movesettle,"
                                  "session,fw,method\n";
        f_write(&f, hdr, sizeof(hdr) - 1, &bw);
    }

    /* Changed: walks the slot table in screen/variant/ref order
     * rather than a ring in arrival order, so the file comes out sorted and
     * diffable instead of shuffled by whatever order things happened to be
     * measured in. Only valid slots are written - an unmeasured item is
     * absent from the file, exactly as before, so the file does not bloat to
     * 434 rows the moment anything is recorded. */
    for (scr = 0; scr < CAL_SCR_COUNT; scr++) {
        for (v = 0; v < CAL_VARIANT_TOTAL(scr); v++) {
            for (r = 0; r < (int)CAL_REFS[scr]; r++) {
                const CalMeas *m = calMeasSlot(scr, v, r);
                char line[128];
                int len;
                if (!m || !m->valid) continue;
                len = calBuildLogLine(m, scr, v, r, line);
                f_write(&f, line, (UINT)len, &bw);
            }
        }
    }

    f_sync(&f);
    f_close(&f);
    flashDoWriteback();   /* same final step settings.c's own save performs */
}

/* ---------------------------------------------------------------------------
 * Loading the log back. Added: calSaveLog() above opens with
 * FA_CREATE_ALWAYS, which TRUNCATES the file - so a fresh boot that
 * recorded even one new measurement without first reading the EXISTING
 * /calmeas.csv back into cal_meas[] would destroy every prior
 * session's data. Called once from
 * vxt_cal_init_handler(). Same "read line-by-line, no multi-KB buffer"
 * discipline as the writer's own per-row stack buffer (convention 16 -
 * bss/stack is tight).
 * ------------------------------------------------------------------------ */
static int calMatchScreenName(const char *s, int len)
{
    int i, j;
    for (i = 0; i < CAL_SCR_COUNT; i++) {
        const char *n = CAL_SCREEN_NAME[i];
        for (j = 0; j < len && n[j] == s[j]; j++) { }
        if (j == len && n[j] == '\0') return i;
    }
    return -1;   /* unknown/corrupt row - caller skips it */
}

/* Parses one comma- or newline-terminated integer field starting at *pos,
 * advances *pos past the field AND its trailing comma (if any) - so a
 * caller can chain calls back to back across a whole CSV row. */
static int32_t calParseField(const char *s, int *pos)
{
    int32_t sign = 1, v = 0;
    if (s[*pos] == '-') { sign = -1; (*pos)++; }
    while (s[*pos] >= '0' && s[*pos] <= '9') {
        v = v * 10 + (int32_t)(s[*pos] - '0');
        (*pos)++;
    }
    if (s[*pos] == ',') (*pos)++;
    return sign * v;
}

static void calLoadLog(void)
{
    FIL f;
    char line[96];
    int first = 1;

    FRESULT fr = f_open(&f, VXT_CAL_RIG_FILE, FA_READ);

    cal_log_unsafe = 0;
    if (fr != FR_OK) {
        /* Only a ABSENT file is "no prior data - fine, cal_meas_n
         * stays 0, same as any other fresh boot". Any other failure says
         * nothing about whether a log exists: refuse to overwrite it. */
        if (fr != FR_NO_FILE && fr != FR_NO_PATH) cal_log_unsafe = 1;
        return;
    }

    /* Changed for the dense slot table. The old ring needed
     * calMeasUpsert() to collapse duplicate rows for the same
     * (screen,variant,ref) down to the latest reading; now the slot ADDRESS
     * does that - a later row for the same item overwrites its own
     * slot, so an iterated converge loop's repeated rows collapse for free
     * and in file order (i.e. last one wins, which is what you want).
     *
     * A row whose (screen,variant,ref) no longer exists - a screen removed,
     * or its variant/ref count reduced - returns NULL from calMeasSlot() and
     * is SKIPPED rather than clamped into some neighbouring item's slot. That
     * is the durable form of the earlier SHARED renumbering incident. */
    while (f_gets(line, (int)sizeof(line), &f) != 0) {
        int nameLen = 0, pos, scr, variant, ref;
        CalMeas *m;

        if (first) { first = 0; continue; }   /* skip the header row */

        while (line[nameLen] && line[nameLen] != ',') nameLen++;
        if (line[nameLen] != ',') continue;          /* malformed - skip */
        scr = calMatchScreenName(line, nameLen);
        if (scr < 0) continue;                       /* unknown name - skip */

        pos = nameLen + 1;
        variant = (int)calParseField(line, &pos);
        ref     = (int)calParseField(line, &pos);
        m = calMeasSlot(scr, variant, ref);
        if (!m) continue;        /* item no longer exists in this build */

        (void)calParseField(line, &pos);   /* card - DERIVED from variant now,
                                            * see calBuildLogLine(); parsed
                                            * only to keep column alignment */
        m->refY    = (int16_t)calParseField(line, &pos);
        m->refX    = (int16_t)calParseField(line, &pos);
        m->dy      = (int16_t)calParseField(line, &pos);
        m->dx      = (int16_t)calParseField(line, &pos);
        m->n       = (uint16_t)calParseField(line, &pos);
        /* Trailing columns, absent in files written before this field existed -
         * calParseField() returns 0 for a missing field, which is exactly
         * right: those rows were all taken with no geometry correction
         * applied, so comp 0 makes (comp - error) reduce to the raw -error
         * they already meant. Old logs stay readable and correct. */
        m->compY   = (int16_t)calParseField(line, &pos);
        m->compX   = (int16_t)calParseField(line, &pos);
        (void)calParseField(line, &pos);   /* nrefs - now CAL_REFS[screen],
                                            * emitted for readers but no
                                            * longer stored or trusted */
        /* Appended later. A legacy row parses these as 0: this row does not
         * say what gains were active.
         * drawGain 0 is not a legal gain (1000 is identity), so 0 reads
         * unambiguously as UNKNOWN rather than as a measured value - which is
         * the whole point, given that a row taken before the rig forced
         * identity gain is 5.3% off one taken after and nothing in the old
         * format could tell them apart. */
        m->drawGain   = (int16_t)calParseField(line, &pos);
        m->moveGain   = (int16_t)calParseField(line, &pos);
        m->moveSettle = (int16_t)calParseField(line, &pos);
        m->session    = (uint16_t)calParseField(line, &pos);
        m->fwStamp    = (uint32_t)calParseField(line, &pos);
        /* Stale-method rows (a legacy row parses as method 0) are dropped:
         * the slot stays unmeasured, so the screen asks for a fresh reading,
         * and the next save leaves the old row out of the file. */
        if ((int)calParseField(line, &pos) != calScreenMethod(scr)) { m->valid = 0; continue; }
        m->valid      = 1;

        /* This boot's session id is one past the highest ever recorded, so
         * "which session was this from" survives power cycles without an RTC
         * and without a monotonic clock of any kind. */
        if (m->session >= cal_session) cal_session = (uint16_t)(m->session + 1);
    }
    f_close(&f);
}

/* Removed (round 2), real bug found via direct feedback ("the
 * correction is not moving DY/DX closer"): this used to seed
 * cal_text_comp_cross/along at boot by AVERAGING every historical TEXT H
 * end-of-row reading in calmeas.csv, flat, with no way to tell which rows
 * were measured raw (correction off) versus already-corrected (a small
 * residual, correction on). Once the log has a mix of both - which it
 * will, the moment anyone iterates the converge loop even once - blending
 * a handful of near-zero residual readings in with the original full-size
 * raw readings understates the correction on every subsequent
 * boot. That is a real, structural bug, not just "needs more data" - not
 * something a bigger sample size fixes. Per direct instruction ("it
 * should always read the most current measurement"), the correction is
 * now built only from what THIS session's own RECORD presses compute live
 * (see the RECORD handler's own add/replace fold-in, inside
 * vxt_cal_handler() below) - a fresh 6809 boot always starts at identity
 * (0,0)/OFF, and the only way a nonzero correction exists is one you
 * watched get computed this session. calLoadLog() still runs at boot -
 * that one is still correct and needed, to populate the in-RAM working
 * set from the durable file (see its own comment) - only the
 * auto-seed-a-starting-correction-from-it step is gone. */

static void calDrawReadout(void)
{
    int have = (cal_ref_n > 0 && cal_sel < cal_ref_n);
    int32_t ry = have ? cal_ref_y[cal_sel] : 0;
    int32_t rx = have ? cal_ref_x[cal_sel] : 0;

    vxtSmartTextSetIntensity(CAL_INT_TEXT);

    CAL_TEXT_ROW(CAL_TEXT_ROW1, CAL_TEXT_LEFT);
    vxtSmartTextStr(CAL_SCREEN_NAME[cal_screen]);
    /* The raw variant, not the geometry variant. Changed after it
     * caused a real mix-up: this used to show CAL_GEOM_VARIANT(), which is
     * `raw % CAL_VARIANTS[screen]`, so a screen with 3 geometry variants
     * displayed "V0 V1 V2 V0 V1 V2" across its six steps while calmeas.csv
     * logged 0..5. Asking for "ANGLE V5" then matched nothing on screen -
     * you had to know it meant the sixth step, shown as V2 with CARD.
     *
     * The log's variant column and the number on screen must be the same
     * number, or a row cannot be found again to re-measure it. The card-on
     * half is still identifiable at a glance: it is the half that says CARD. */
    vxtSmartTextStr(" V");
    calNumber((int32_t)cal_variant[cal_screen]);
    /* CARD is an experimental variable, not decoration - see calDrawCard()'s
     * own note. It is shown here and logged with every measurement because it
     * was observed to change where the figure lands. */
    if (cal_overlay) vxtSmartTextStr(" CARD");

    /* Added, MOVED onto this existing row same day (round 2) per
     * direct feedback ("needs to go on all the screens, its missing on the
     * horizontal text screen"): was its own separate 4th row
     * (CAL_TEXT_ROW4, now removed), shown on TEXT H only. Two real problems
     * with that: (1) TEXT H is by far the most record-hungry screen in this
     * whole rig (calScreenText()'s own header: ~1150 of the 1536 budget for
     * the geometry alone) - a whole extra row was the most likely thing to
     * overflow away, and would do so on exactly the one screen
     * this status line matters most on; (2) CAL_TEXT_ROW4 sat at Y=-17800,
     * only 200 units inside VXT_BOUNDS_HALF_Y (18000) - deep enough into
     * likely overscan territory that this rig's OWN reference card
     * deliberately stays much further inside that bound (CAL_CARD_HALF_Y,
     * 10000 - see calDrawCard()'s own comment on why). Folding it onto this
     * ALREADY-DRAWN row costs no new reposition and no new row, and shows
     * it on every screen, not just TEXT H, per the direct request - the
     * underlying cal_text_apply_enabled/cal_text_comp_cross/along are
     * shared global state regardless of which screen is selected, so
     * showing them everywhere is accurate, not misleading (they only ever
     * affect the actual DRAW on TEXT H, per this feature's own scope, but
     * knowing the stored value while browsing other screens is exactly
     * what was asked for). */
    /* Extended, REVISED same day - shows whichever comp is
     * relevant to the CURRENT screen: TEXT H's own C=/A= pair, or the
     * flat, shared cal_geom_comp_dy/dx (now Y=/X=, not C=/A= - it's a
     * screen-space offset, not a direction-relative pair, see
     * cal_geom_comp_dy's own header comment for why the model changed)
     * on every geometry screen. */
    if (cal_screen == CAL_SCR_TEXTH) {
        vxtSmartTextStr(cal_text_apply_enabled ? " CORR ON C=" : " CORR OFF C=");
        calNumber((int32_t)cal_text_comp_cross);
        vxtSmartTextStr(" A=");
        calNumber((int32_t)cal_text_comp_along);
    } else {
        /* Shows the CURRENT item's own correction (per-ref since round 3),
         * plus how many of this screen's items have been measured at all -
         * "3-6" reads as "3 of 6 fitted", so a half-corrected screen is
         * visible rather than looking like a correction that failed. */
        int k, nSet = 0;
        for (k = 0; k < cal_ref_n && k < CAL_MAX_REFS; k++) {
            if (cal_geom_comp_set[k]) nSet++;
        }
        vxtSmartTextStr(cal_geom_apply_enabled ? " CORR ON Y=" : " CORR OFF Y=");
        calNumber(have ? (int32_t)cal_geom_comp_dy[cal_sel] : 0);
        vxtSmartTextStr(" X=");
        calNumber(have ? (int32_t)cal_geom_comp_dx[cal_sel] : 0);
        vxtSmartTextStr(" FIT=");
        calNumber((int32_t)nSet);
        vxtSmartTextStr("-");
        calNumber((int32_t)cal_ref_n);
    }

    /* R= names WHICH reference the reading belongs to. Without it a logged
     * row is ambiguous the moment a screen has more than one measurable point,
     * which is most of them. The matching box drawn on-screen (calDrawSelected)
     * is the visual half of the same answer. */
    CAL_TEXT_ROW(CAL_TEXT_ROW2, CAL_TEXT_LEFT);
    vxtSmartTextStr("R=");
    calNumber((int32_t)cal_sel);
    vxtSmartTextStr("-");
    calNumber((int32_t)cal_ref_n);      /* item i of N - shows coverage, so a
                                         * screen cannot be left half-measured
                                         * without it being visible */
    vxtSmartTextStr(" DY=");
    calNumber(cal_caret_y - ry);
    vxtSmartTextStr(" DX=");
    calNumber(cal_caret_x - rx);

    /* Added (round 2) - diagnostic aid, per troubleshooting
     * "the correction isn't moving": RECORD only folds into the live
     * correction on a TEXT H END reference (odd R=, the one with a known
     * character count - see calScreenText()'s own start-then-end calMark()
     * pairing). Recording on a START reference still logs to calmeas.csv
     * but does nothing to the correction - this makes that
     * visible instead of a silent no-op that looks identical to a bug. */
    if (cal_screen == CAL_SCR_TEXTH) {
        vxtSmartTextStr((cal_sel & 1) ? " END" : " START");
    }
    /* Added - SHARED's reference 0 is the RING itself, not a
     * connector, and refs 0 and 1 sit at the same ideal coordinate while
     * measuring two different drawn points. Without this label they are
     * indistinguishable on screen, which is the exact ambiguity that made
     * a ~1000-unit ring displacement look like a failed connector
     * correction for an entire session. */
    if (cal_screen == CAL_SCR_SHARED) {
        vxtSmartTextStr((cal_sel == cal_ref_base) ? " RING" : " CONN");
    }
    /* Added - on PRIME the variable under test is the VARIANT
     * (priming cycles), not Corr, and that was not visible anywhere: the
     * readout showed "V2" with no indication it meant 2 priming cycles.
     * Reported as "testing PRIME, no improvement with CORR=on" - the right
     * control was not discoverable. Show the actual cycle count. */
    if (cal_screen == CAL_SCR_PRIME) {
        vxtSmartTextStr(" CYCLES=");
        calNumber((int32_t)CAL_PRIME_CYCLES[
            CAL_GEOM_VARIANT(cal_screen, cal_variant[cal_screen]) % 5]);
    }

    /* Figure cost in records, EXCLUDING the caret and this text - measured,
     * not estimated. S = rows currently in the log. */
    CAL_TEXT_ROW(CAL_TEXT_ROW3, CAL_TEXT_LEFT);
    vxtSmartTextStr("N=");
    calNumber((int32_t)cal_fig_records);
    vxtSmartTextStr(" S=");
    /* COVERAGE, not a running count. Changed so it also shows which
     * measures are blank and can be supplemented: this used to show
     * cal_meas_n, the number of rows the
     * ring happened to hold, which answered "how much have I done" but never
     * "how much is LEFT". The slot table knows the denominator, so it can now
     * show filled/total for THIS screen - walk the variants and any pair that
     * is not N/N still has blanks to fill. */
    {
        int tot = 0, filled = calScreenFilled(cal_screen, &tot);
        calNumber((int32_t)filled);
        vxtSmartTextStr("-");
        calNumber((int32_t)tot);
    }
    /* The ring's silent-eviction failure is now structurally impossible - one
     * permanent slot per item, no capacity to exhaust - so " FULL" is gone.
     * What replaces it is the one way the new scheme CAN fail: the screen
     * tables needing more slots than CAL_TOTAL_SLOTS was sized for, which
     * happens the moment a screen is added without updating it. Loud, not
     * silent - that was the whole complaint about the ring. */
    if (cal_slots_over) vxtSmartTextStr(" SLOTS!");
    if (vxtSmartOverflowed()) vxtSmartTextStr(" OVF");
}

/* ---------------------------------------------------------------------------
 * Frame handler
 * ------------------------------------------------------------------------ */
void vxt_cal_handler(uint8_t id, volatile uint8_t *parm)
{
    int8_t joyX = vxtInJoyX(parm);
    int8_t joyY = vxtInJoyY(parm);
    int prevScreen = cal_screen;
    uint8_t prevVariant = cal_variant[cal_screen];
    (void)id;

    /* First real frame: capture the stick's resting position as center,
     * before anything else reads joyX/joyY. Assumes the stick is untouched
     * immediately after power-on, the same assumption every other joystick
     * consumer on this rig already makes implicitly. */
    if (!cal_joy_centered) {
        cal_joy_y_center = joyY;
        cal_joy_x_center = joyX;
        cal_joy_centered = 1;
    }

    /* Skip/record are DISCRETE actions, so they use the raw per-button edge
     * bytes directly. Do not switch these to the learn-a-held-bitmask
     * pattern - that fires repeatedly across the ~10 frames one physical
     * tap spans, which reads as several presses instead of one.
     *
     * Remapped to free a button for RECORD. "Previous screen" is
     * gone; nine screens cycle forward quickly enough, and being able to
     * capture a reading matters more than reverse navigation. */
    /* Both of these are acted on AFTER the screen has drawn - only then do this
     * frame's reference points exist, so only then is "the next item" defined. */
    cal_skip_now   = vxtInBtn3(parm) ? 1 : 0;
    cal_record_now = vxtInBtn4(parm) ? 1 : 0;

    /* Button 1: TAP (release within CAL_BTN1_TAP_MAX_FRAMES) cycles the
     * screen; HELD + joystick left/right cycles the current screen's
     * variant instead. REPLACES the old plain button-2-tap for variant
     * cycling - see this feature's own header comment above
     * cal_text_comp_cross for why (button 2 freed up for the apply
     * toggle). The screen-cycle fires on RELEASE, not press, specifically
     * so starting a hold-for-variant gesture never also cycles the screen
     * once as a side effect. */
    {
        int btn1Held = calBtnHeld(parm, &cal_btn1_mask, parm[VXT_IN_BTN1_1]);

        if (btn1Held) {
            cal_btn1_hold_frames++;
        } else {
            if (cal_btn1_hold_frames > 0 && cal_btn1_hold_frames <= CAL_BTN1_TAP_MAX_FRAMES) {
                cal_screen = (cal_screen + 1) % CAL_SCR_COUNT;
            }
            cal_btn1_hold_frames = 0;
        }

        if (btn1Held) {
            if (joyX > CAL_VARIANT_JOY_THRESHOLD || joyX < -CAL_VARIANT_JOY_THRESHOLD) {
                if (cal_var_joy_armed) {
                    int total = (int)CAL_VARIANT_TOTAL(cal_screen);
                    int step  = (joyX > 0) ? 1 : -1;
                    cal_variant[cal_screen] = (uint8_t)(((int)cal_variant[cal_screen] + step + total) % total);
                    cal_var_joy_armed = 0;
                }
            } else if (joyX > -CAL_VARIANT_JOY_REARM && joyX < CAL_VARIANT_JOY_REARM) {
                cal_var_joy_armed = 1;   /* stick back near center - ready
                                         * for the next step */
            }
        } else {
            cal_var_joy_armed = 1;   /* always armed outside the hold gesture */
        }
    }

    /* Button 2: plain TAP toggles whether the accumulated correction is
     * applied - text-skew comp on TEXT H, the shared flat geometry comp on
     * every other screen. Changed (round 2) from a hold-2+3
     * chord, reported not working on hardware - see this feature's own
     * header comment above cal_text_comp_cross. Extended, real
     * gap found via direct feedback ("Corr on/off using button2 [has no
     * effect] for the other test"): this used to toggle
     * cal_text_apply_enabled unconditionally regardless of screen, * doing nothing everywhere but TEXT H. REVISED same day, second real
     * gap found via direct feedback ("apply the fixes to the other calib
     * screens too"): SHARED-only wiring answered one screen's worth of
     * data; routed to the shared cal_geom_apply_enabled flag instead, which
     * every geometry screen's draw now reads (calApplyGeomComp()), so Corr
     * on/off does something real on LADDER/SCALE/CHAIN/SHARED/ANGLE/
     * DEFLECT, not just one of them. CENTRE/PRIME have no measurable test
     * figure of the kind this applies to - still a no-op there, correctly. */
    if (vxtInBtn2(parm)) {
        if (cal_screen == CAL_SCR_TEXTH) {
            cal_text_apply_enabled = (uint8_t)!cal_text_apply_enabled;
        } else {
            cal_geom_apply_enabled = (uint8_t)!cal_geom_apply_enabled;
        }
    }

    /* Fixed - real bug, found while auditing the storage rewrite:
     * The reference card has never drawn, on any variant, on any screen.
     *
     * This passed CAL_GEOM_VARIANT(...) into CAL_CARD_ON(...). The former
     * returns `v % CAL_VARIANTS[s]`, which is by definition always LESS than
     * CAL_VARIANTS[s]; the latter tests `v >= CAL_VARIANTS[s]`. So the test
     * was unconditionally false, cal_overlay was permanently 0, and
     * calDrawCard() was never reached (CENTRE draws its own card by a
     * different path, which is why the feature looked alive).
     *
     * CAL_CARD_ON wants the RAW variant - the whole point of the earlier
     * design is that the sequence's second half IS the card-on half, so the
     * modulo that picks the GEOMETRY is exactly what must not be applied here.
     *
     * Consequence, and why every logged row reads card=0: half of every
     * screen's variant sequence has been a silent duplicate of the other
     * half. The A/B comparison the card twins exist to provide has never
     * been taken. Of the rows logged so far only ANGLE variant 5
     * sits in the card-ON half, so exactly one existing row was measured
     * card-off while now being labelled card-on - re-take it rather than
     * trusting its card column. */
    cal_overlay = CAL_CARD_ON(cal_screen, cal_variant[cal_screen]);

    /* Added - drop every per-ref correction the moment the
     * geometry it was fitted to changes. Done HERE, from the actual live
     * (screen,variant) rather than inside each button handler, so no future
     * navigation path can bypass it - a correction surviving onto
     * different geometry is precisely the stale-calibration failure this rig
     * exists to prevent. See cal_geom_comp_dy[]'s own header comment. */
    if (cal_screen != cal_geom_comp_screen ||
        cal_variant[cal_screen] != cal_geom_comp_variant) {
        int k;
        for (k = 0; k < CAL_MAX_REFS; k++) {
            cal_geom_comp_dy[k]  = 0;
            cal_geom_comp_dx[k]  = 0;
            cal_geom_comp_set[k] = 0;
        }
        cal_geom_comp_screen  = (int8_t)cal_screen;
        cal_geom_comp_variant = cal_variant[cal_screen];
        cal_geom_seed_pending = 1;   /* seeded AFTER this frame's draw, when
                                      * cal_ref_n is valid - see calSeedGeomComp() */
    }

    /* The caret is CONTINUOUS, driven by the stick rather than a button, so
     * the bitmask pattern does not apply to it. Deflection is measured from
     * the sampled rest position, not raw zero - see cal_joy_y_center's own
     * comment for why an uncorrected rest offset creeps the caret with the
     * stick untouched. */
    {
        int dy = (int)joyY - (int)cal_joy_y_center;
        int dx = (int)joyX - (int)cal_joy_x_center;
        if (dy >  127) dy =  127;
        if (dy < -127) dy = -127;
        if (dx >  127) dx =  127;
        if (dx < -127) dx = -127;
        cal_caret_y += calCaretStep((int8_t)dy);
        cal_caret_x += calCaretStep((int8_t)dx);
    }
    if (cal_caret_y >  VXT_BOUNDS_HALF_Y) cal_caret_y =  VXT_BOUNDS_HALF_Y;
    if (cal_caret_y < -VXT_BOUNDS_HALF_Y) cal_caret_y = -VXT_BOUNDS_HALF_Y;
    if (cal_caret_x >  VXT_BOUNDS_HALF_X) cal_caret_x =  VXT_BOUNDS_HALF_X;
    if (cal_caret_x < -VXT_BOUNDS_HALF_X) cal_caret_x = -VXT_BOUNDS_HALF_X;

    vxtSmartBegin(CAL_REGION_OFFSET, CAL_REGION_RECORDS);
    gamelibBeamBegin(CAL_POS_SCALE, CAL_DRAW_SCALE);
    /* Force identity gain, every frame - a likely source of this rig's
     * unexplained measurement variance if left inherited.
     *
     * gb_draw_gain is a file-static in gamelib_beam.c that only a game
     * writes (gamelibBeamSetDrawGain(1053)). The STM32 does not reboot when
     * the 6809 changes carts, so booting straight into this rig measures at
     * gain 1000, while running that game first and then switching to the
     * rig measures at 1053 - a silent 5.3% difference in every drawn
     * length, about 318 units on a 6000-unit figure. That is the same
     * order as the repeat-reading spread that made the data unfittable
     * (CHAIN 854->690, SHARED ring 816->929).
     *
     * A measuring instrument must not inherit state from whatever ran
     * before it. Identity here means the rig always reports RAW hardware
     * behavior, and any gain a game applies is then a separate, deliberate
     * layer on top rather than a hidden term inside the measurement. */
    gamelibBeamSetDrawGain(CAL_RIG_DRAW_GAIN);
    /* Move gain back to identity. 910 was tested on hardware and did
     * nothing: it shrinks the emitted distance ~9% but does not change how
     * many records vxtSmartMoveBig emits (ceil(|phys/32|/100) - 2 stays 2,
     * 3 stays 3), and the error tracks record count, not distance. A rig
     * must not correct what it is measuring. */
    gamelibBeamSetMoveGain(CAL_RIG_MOVE_GAIN);
    /* Back to raw. The correction (k=1000) was confirmed here at 3100-9700
     * - REPOS 1014/506/975/676/1034/722 -> 22/36/-30/21/4/26, CHAIN 920 ->
     * 9, SHARED ring 919 -> -55 - and then broke the game, because its
     * first reposition is often far SHORTER than anything measured.
     *
     * The open question is now whether the 10x-step relation holds at short
     * distances, and that can only be answered against RAW hardware. Set this
     * to 1000 to re-verify the correction instead; leave it 0 to measure. */
    gamelibBeamSetMoveSettle(CAL_RIG_MOVE_SETTLE);

    /* IDENTITY here, before any screen runs - never the loaded UI value.
     * calScreenText() computes its reference marks through
     * vxtSmartTextWidthPhys(), which reads the GLOBAL skew state, and those
     * marks are the measurement targets: they must be NOMINAL. Applying the
     * loaded correction at this point shifts the TEXT H targets by the correction itself, so the rig measures
     * the caret against the wrong point, converges to the wrong value, and
     * records it - which loads into the next boot's correction and shifts the
     * targets further. The loaded value is applied only around
     * calDrawReadout() below, the one place this rig draws its own UI text. */
    vxtSmartTextSetSkewComp(0, 0);

    cal_ref_n = 0;   /* references are re-registered by whichever screen runs */

    if (cal_overlay && cal_screen != CAL_SCR_CENTRE) calDrawCard(0);
    cal_ref_base = cal_ref_n;   /* whatever the card registered, the screen's
                                 * own figures start after it */

    switch (cal_screen) {
    case CAL_SCR_LADDER:  calScreenLadder(CAL_GEOM_VARIANT(cal_screen, cal_variant[cal_screen]));  break;
    case CAL_SCR_SCALE:   calScreenScale(CAL_GEOM_VARIANT(cal_screen, cal_variant[cal_screen]));   break;
    case CAL_SCR_CHAIN:   calScreenChain(CAL_GEOM_VARIANT(cal_screen, cal_variant[cal_screen]));   break;
    case CAL_SCR_SHARED:  calScreenShared(CAL_GEOM_VARIANT(cal_screen, cal_variant[cal_screen]));  break;
    case CAL_SCR_ANGLE:   calScreenAngle(CAL_GEOM_VARIANT(cal_screen, cal_variant[cal_screen]));   break;
    case CAL_SCR_DEFLECT: calScreenDeflect(CAL_GEOM_VARIANT(cal_screen, cal_variant[cal_screen])); break;
    /* Changed (round 2), real bug found via direct feedback
     * ("corr on doesn't change anything, RECORD doesn't make it realign"):
     * CAL_SCR_TEXTH used to bracket the whole calScreenText() call with
     * vxtSmartTextSetSkewComp(), which contaminated the reference marks'
     * own "ideal target" computation with the live correction
     * (vxtSmartTextWidthPhys() reads that same global state) - see
     * calScreenText()'s own fix comment for the full root cause. Skew comp
     * is now set only inside calScreenText(), narrowly around the actual
     * string draw, never around the reference-mark math. */
    case CAL_SCR_REPOS:   calScreenRepos(CAL_GEOM_VARIANT(cal_screen, cal_variant[cal_screen]));   break;
    case CAL_SCR_ACCUM:   calScreenAccum(CAL_GEOM_VARIANT(cal_screen, cal_variant[cal_screen]));   break;
    case CAL_SCR_CHORD:   calScreenChord(CAL_GEOM_VARIANT(cal_screen, cal_variant[cal_screen]));   break;
    case CAL_SCR_HOP:     calScreenHop(CAL_GEOM_VARIANT(cal_screen, cal_variant[cal_screen]));     break;
    case CAL_SCR_TEXTH:   calScreenText(CAL_GEOM_VARIANT(cal_screen, cal_variant[cal_screen]), 0); break;
    case CAL_SCR_TEXTV:   calScreenText(CAL_GEOM_VARIANT(cal_screen, cal_variant[cal_screen]), 1); break;
    case CAL_SCR_PRIME:   calScreenPrime(CAL_GEOM_VARIANT(cal_screen, cal_variant[cal_screen])); break;
    default:              calScreenCentre(CAL_GEOM_VARIANT(cal_screen, cal_variant[cal_screen]));  break;
    }

    /* Snap the caret to the screen's FIRST reference mark whenever the figure
     * changes, so every measurement starts from a reading of exactly 0 and the
     * user drives it to the observed landing point. Done AFTER the screen ran,
     * because that is what registers the references.
     *
     * Fixed (reported from the first hardware session): this used
     * to trigger on a SCREEN change only, but changing a screen's VARIANT
     * moves its target too (different length, angle, radius or N). The caret
     * stayed put, so DY/DX went on displaying an offset from the PREVIOUS
     * variant's target - a stale number that looks exactly like a real
     * reading. Same failure class this file already refuses to accept from
     * vxtSmartTextNumber()'s silent clamp: in a measurement instrument, a
     * wrong number is worse than no number. */
    if (cal_screen != prevScreen || cal_variant[cal_screen] != prevVariant) {
        cal_sel = 0;
        cal_caret_y = (cal_ref_n > 0) ? cal_ref_y[0] : 0;
        cal_caret_x = (cal_ref_n > 0) ? cal_ref_x[0] : 0;
    }

    cal_fig_records = vxtSmartRecordCount();

    /* Item cursor, record and skip - all AFTER the screen has run, because only
     * now do this frame's reference points exist. */
    {
        int itemChanged = 0;

        if (cal_ref_n <= 0) {
            cal_sel = 0;
        } else {
            if (cal_sel >= cal_ref_n) { cal_sel = 0; itemChanged = 1; }

            /* THE `cal_record_now` gate is load-BEARING - do not remove it.
             * Restored after it was dropped by the slot-table
             * rewrite the day before, which is a mistake worth naming because
             * the symptom was nothing like the cause: the calibration screen
             * "flashed through black about every half second". Without this
             * gate the body below runs every frame, so calSaveLog() performed
             * a full SD rewrite plus flashDoWriteback() every frame; the STM32
             * blocked for ~100ms while the 6809 sat waiting in its RAM-
             * resident RPC stub, and the screen went
             * dark for the duration. It also overwrote the current
             * slot with whatever the caret happened to be sitting on.
             *
             * What the slot table DID make unconditional is capacity - every
             * item has a permanent slot, so there is no "log full" case to
             * gate on (that was a real bug where a
             * `cal_meas_n < CAL_MAX_MEAS` test made RECORD do
             * nothing). Capacity being unconditional is not the same as the
             * WRITE being unconditional, and conflating the two is exactly
             * how the gate got deleted. */
            if (cal_record_now) {
                CalMeas *m = calMeasSlot(cal_screen, cal_variant[cal_screen], cal_sel);
                if (m) {
                m->refY = (int16_t)cal_ref_y[cal_sel];
                m->refX = (int16_t)cal_ref_x[cal_sel];
                m->dy   = (int16_t)(cal_caret_y - cal_ref_y[cal_sel]);
                m->dx   = (int16_t)(cal_caret_x - cal_ref_x[cal_sel]);
                m->n    = (uint16_t)cal_fig_records;
                /* Stamp the correction that was ACTIVE for this reading, so
                 * the row is self-describing - see CalMeas.compY.
                 *
                 * Restored: the slot-table rewrite flattened this
                 * into a single geometry-comp expression and LOST the TEXT H
                 * branch below. That branch is not decoration - it fixes a
                 * real bug found while chasing "TEXT Corr does
                 * nothing", and dropping it reintroduced that bug. */
                m->compY = 0;
                m->compX = 0;
                /* WHICH correction was applied - not this
                 * reference's own stored one. Fixed, real defect
                 * caught by dry-running vxtCalLoadClosureComp() against a real
                 * log before shipping it.
                 *
                 * ACCUM and CHORD translate the whole figure by reference 0's
                 * correction (see calScreenAccum()), so reference 1 is drawn
                 * through ref 0's value, not its own. Stamping cal_sel's here
                 * made ref 1's row claim a correction that was never applied
                 * to it, and any raw-error reconstruction from that row came
                 * out wrong - visibly so: one CHORD variant reconstructed to
                 * 8.76% against ~3.5% for its identical twins.
                 *
                 * A row must record what the beam did. */
                if (cal_screen == CAL_SCR_ACCUM || cal_screen == CAL_SCR_CHORD ||
                    cal_screen == CAL_SCR_HOP) {   /* HOP: same ref-0 anchoring */
                    if (cal_geom_apply_enabled) {
                        m->compY = cal_geom_comp_dy[0];
                        m->compX = cal_geom_comp_dx[0];
                    }
                } else if (cal_screen == CAL_SCR_TEXTH) {
                    /* TEXT H has its own, per-character correction and does
                     * not read cal_geom_comp - record THAT one here, or the
                     * row would be stamped with an unrelated geometry screen's
                     * value (cal_geom_apply_enabled is global and stays set
                     * when you navigate here from SHARED, so this wrote a
                     * SHARED correction into TEXT rows). Units are the
                     * per-character comp, matching what cal_text_comp_cross/
                     * along hold. */
                    if (cal_text_apply_enabled) {
                        m->compY = (int16_t)cal_text_comp_cross;
                        m->compX = (int16_t)cal_text_comp_along;
                    }
                } else if (cal_geom_apply_enabled && cal_sel < CAL_MAX_REFS) {
                    m->compY = cal_geom_comp_dy[cal_sel];
                    m->compX = cal_geom_comp_dx[cal_sel];
                }
                /* Added - the gamelib gains ACTIVE for this
                 * reading, and the build that took it. The rig forces all
                 * three to identity every frame (see the vxtSmartBegin block
                 * in vxt_cal_handler), so these SHOULD read 1000/1000/0 - but
                 * recording them is the point: a row that ever reads anything
                 * else is a row measured against a corrected beam, and that is
                 * precisely the ambiguity that made an earlier LADDER-vs-
                 * 1053 validation impossible to settle. */
                m->drawGain   = CAL_RIG_DRAW_GAIN;
                m->moveGain   = CAL_RIG_MOVE_GAIN;
                m->moveSettle = CAL_RIG_MOVE_SETTLE;
                m->session    = cal_session;
                m->fwStamp    = CAL_FW_STAMP;
                m->valid      = 1;

                /* The SD write blocks for a frame or so. That is
                 * architecturally safe here - the 6809 is waiting in its
                 * RAM-resident RPC stub, not fetching
                 * from emulated ROM - but it IS a visible hitch, and it is
                 * unverified on this path. If it misbehaves on hardware,
                 * buffer and flush on exit instead of writing per record.
                 * Reverted (round 4) back to calSaveLog() (a
                 * full rewrite from cal_meas[]) - round 3's append-only
                 * calAppendLogRow() was wrong, see calSaveLog()'s own
                 * comment for why: it let the file hold MULTIPLE,
                 * ambiguous readings for the same calibration target. */
                calSaveLog();

                /* Added - fold this reading into the live
                 * text-skew correction. See this whole feature's own
                 * header comment above cal_text_comp_cross for the
                 * add-vs-replace reasoning; TEXT H only, end-of-row
                 * references only (odd cal_sel - see calScreenText()'s own
                 * start-then-end calMark() pairing). */
                if (cal_screen == CAL_SCR_TEXTH && (cal_sel & 1)) {
                    int row = cal_sel / 2;
                    if (row >= 0 && row < CAL_TEXT_ROWS) {
                        int32_t runDy, runDx;
                        float rawCross, rawAlong;
                        calTextRunError(cal_variant[cal_screen], cal_sel, m->dy, m->dx, &runDy, &runDx);
                        rawCross = -(float)runDy / (float)CAL_TEXT_LEN[row] / (float)CAL_TEXT_COMP_SCALE_DIV;
                        rawAlong = -(float)runDx / (float)CAL_TEXT_LEN[row] / (float)CAL_TEXT_COMP_SCALE_DIV;

                        if (cal_text_apply_enabled) {
                            /* Correction was ACTIVE for the string just
                             * measured - this reading is a RESIDUAL, add
                             * the extra nudge on top of what's already
                             * accumulated. */
                            cal_text_comp_cross = calClampComp((float)cal_text_comp_cross + rawCross);
                            cal_text_comp_along = calClampComp((float)cal_text_comp_along + rawAlong);
                        } else {
                            /* Correction was OFF - this reading is the
                             * FULL raw error, so it REPLACES whatever was
                             * accumulated rather than stacking on top of
                             * an unrelated earlier round. */
                            cal_text_comp_cross = calClampComp(rawCross);
                            cal_text_comp_along = calClampComp(rawAlong);
                        }
                    }
                }

                /* Added, revised twice the same day - converge
                 * loop for the geometry comp, on every screen wired to use
                 * it (see calApplyGeomComp()'s callers). Round 1 was
                 * SHARED-only and decomposed into each connector's own
                 * along/cross frame (hardware-disproven); round 2 was one
                 * flat global pair (structurally could not close more than
                 * one item at a time); round 3 - this one - is flat AND
                 * per-reference, so measuring every item on a screen closes
                 * every item. See cal_geom_comp_dy[]'s own header comment
                 * for the full reasoning and what each outcome now proves. */
                if (cal_sel < CAL_MAX_REFS &&
                    (cal_screen == CAL_SCR_LADDER || cal_screen == CAL_SCR_SCALE ||
                     cal_screen == CAL_SCR_CHAIN  || cal_screen == CAL_SCR_SHARED ||
                     cal_screen == CAL_SCR_ANGLE  || cal_screen == CAL_SCR_DEFLECT ||
                     /* Added - these two were drawing without comp
                      * AND excluded from the fold-in, so Corr was inert on
                      * them in both directions. Both now translate the whole
                      * figure, so both belong here. */
                     cal_screen == CAL_SCR_ACCUM  || cal_screen == CAL_SCR_CHORD ||
                     cal_screen == CAL_SCR_HOP)) {
                    float rawDy = -(float)m->dy;
                    float rawDx = -(float)m->dx;

                    /* Add-vs-replace keyed on whether THIS ref was already
                     * corrected while being measured - not merely on the
                     * global apply flag. With per-ref corrections the two
                     * differ: apply can be ON while this particular ref is
                     * still unmeasured (comp 0), in which case the reading
                     * is a full raw error and must REPLACE, not accumulate
                     * onto a zero that was never fitted. */
                    if (cal_geom_apply_enabled && cal_geom_comp_set[cal_sel]) {
                        cal_geom_comp_dy[cal_sel] =
                            calClampComp16((float)cal_geom_comp_dy[cal_sel] + rawDy);
                        cal_geom_comp_dx[cal_sel] =
                            calClampComp16((float)cal_geom_comp_dx[cal_sel] + rawDx);
                    } else {
                        cal_geom_comp_dy[cal_sel] = calClampComp16(rawDy);
                        cal_geom_comp_dx[cal_sel] = calClampComp16(rawDx);
                    }
                    cal_geom_comp_set[cal_sel] = 1;
                }
                }   /* if (m) - slot exists; a NULL means the screen
                     * registered more refs than CAL_REFS[] declares, which
                     * is a table bug, not user input */
            }

            /* SKIP and RECORD both advance to the next item. Skipping writes
             * no row - "confusing, moved on" is a deliberate absence in the
             * log, not a value that could later be mistaken for a reading. */
            if (cal_record_now || cal_skip_now) {
                cal_sel = (cal_sel + 1) % cal_ref_n;
                itemChanged = 1;
            }

            /* Snap the caret onto the new item so the next measurement starts
             * from exactly 0 0, the same contract a screen/variant change has. */
            if (itemChanged) {
                cal_caret_y = cal_ref_y[cal_sel];
                cal_caret_x = cal_ref_x[cal_sel];
            }
            calDrawSelected(cal_ref_y[cal_sel], cal_ref_x[cal_sel]);
        }
    }

    gamelibBeamCloseRun();   /* close the figure's last draw run before the
                              * caret's reposition, so the beam does not sit
                              * LIT through it - the bright-dwell-dot
                              * mechanism (roadmap item 9), which would put a
                              * spurious bright dot on the very geometry being
                              * measured. */
    calSeedGeomComp();   /* after the draw - cal_ref_n is valid only now */
    if (cal_screen == CAL_SCR_TEXTH) calSeedTextComp();
    calDrawMeasureAids();
    calDrawCaret();
    gamelibBeamCloseRun();
    /* This rig's own UI text - title, labels, readout - is all drawn here,
     * after every measurement figure and reference mark, so the loaded
     * correction can be applied for readability without ever touching a
     * measurement target. Restored to identity afterwards so the NEXT frame's
     * screen dispatch starts nominal. */
    vxtSmartTextSetSkewComp(cal_ui_text_cross, cal_ui_text_along);
    calDrawReadout();
    vxtSmartTextSetSkewComp(0, 0);

    vxtSmartEnd();
}

void vxt_cal_init_handler(uint8_t id, volatile uint8_t *parm)
{
    int i;
    int8_t cross, along;
    (void)id; (void)parm;

    /* Load once, for this rig's own general text only - never touches
     * cal_text_comp_cross/along, the TEXT H/V screens' own live candidate. */
    cal_ui_text_cross = 0;
    cal_ui_text_along = 0;
    if (vxtCalLoadTextComp(&cross, &along)) {
        cal_ui_text_cross = cross;
        cal_ui_text_along = along;
    }

    cal_joy_centered = 0;   /* re-sample the stick's rest position next frame */

    /* THIS rig owns /calmeas.csv, and its CHORD rows are padded by its own
     * CAL_ACCUM_PAD_R. Declared explicitly every boot rather than inherited:
     * the STM32 is not reset when the 6809 changes carts, so a game cart run
     * before this one will have pointed the loaders at its own file. */
    vxtCalLoadSetSource(VXT_CAL_RIG_FILE, VXT_CAL_RIG_TMP,
                        VXT_CAL_RIG_CHORD_PAD);

    cal_screen  = CAL_SCR_CENTRE;
    cal_overlay = 0;
    cal_caret_y = 0;
    cal_caret_x = 0;
    cal_fig_records = 0;
    cal_ref_n = 0;
    {   /* Changed: clear every slot (valid=0 => "never measured",
         * which is what the coverage readout and the seeders both key on),
         * then recompute the per-screen bases from the CURRENT screen tables
         * so adding or resizing a screen re-lays the table automatically. */
        int i;
        for (i = 0; i < CAL_TOTAL_SLOTS; i++) cal_meas[i].valid = 0;
        cal_session = 0;
        calSlotInit();
    }
    cal_record_now = 0;
    cal_skip_now = 0;
    cal_sel = 0;
    for (i = 0; i < CAL_SCR_COUNT; i++) cal_variant[i] = 0;

    /* Added - live text-skew apply/converge state (see its own
     * header comment above cal_text_comp_cross). Always starts at identity
     * (0,0)/OFF - Changed (round 2), see calLoadLog()'s own
     * following comment for why this is no longer auto-seeded from
     * calmeas.csv history. cal_meas_n above is already reset to 0 BEFORE
     * this runs, so calLoadLog() populates it fresh from whatever the SD
     * card already has - not stale in-RAM state from a previous 6809
     * session (the 6809 reboots independently of the STM32, this RPC
     * exists precisely so that doesn't leave this rig showing mid-session
     * numbers from before the reset). */
    cal_text_comp_cross = 0;
    cal_text_comp_along = 0;
    cal_text_apply_enabled = 0;
    for (i = 0; i < CAL_MAX_REFS; i++) {
        cal_geom_comp_dy[i]  = 0;
        cal_geom_comp_dx[i]  = 0;
        cal_geom_comp_set[i] = 0;
    }
    cal_geom_comp_screen   = -1;
    cal_geom_seed_pending  = 0;
    cal_geom_apply_enabled = 0;
    cal_btn1_mask = 0;
    cal_btn1_hold_frames = 0;
    cal_var_joy_armed = 1;
    calLoadLog();   /* still needed - see this call's own comment near its
                    * definition - just no longer followed by an auto-seed */
}
