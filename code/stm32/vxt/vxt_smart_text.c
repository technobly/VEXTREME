/*
 * vxt_smart_text.c - see vxt_smart_text.h for the full design rationale.
 *
 * DRIVER, not A FONT: this file has no glyph data of its own. It walks
 * whatever table vxtSmartTextSetFontTable() registered (vxt_font.h) and
 * turns each glyph's strokes into SmartList move/draw records. An
 * OVERLAY font may additionally be registered via
 * vxtSmartTextRegisterOverlayFont() and toggled at runtime with
 * vxtSmartTextSetFont() - when active, the overlay is tried first
 * (including its own ligature lookup, if it has one) and anything it
 * doesn't map (digits, punctuation it omits) falls through to the base
 * table. See vxt_font_western.c for the base font.
 */
#include "vxt_smart_text.h"
#include "vxt_smart.h"

#define VXT_TEXT_HORIZ_DEFAULT  0x12   /* IDENTICAL to vxt_text.c's values - see  */
#define VXT_TEXT_VERT_DEFAULT   0x12   /* that file's header if these ever need  */
#define VXT_TEXT_SCALE      0x08   /* to change; kept in sync by hand, not a */
/* VXT_TEXT_POS_SCALE now lives in vxt_smart_text.h - callers must divide
 * their origins by it, so it is part of this module's contract. */
#define VXT_TEXT_GAP        1
#define VXT_TEXT_WIDTH      3

/*
 * an 11-unit advance) can drive this same chained emitter. The defaults are the constants above, so a caller that never calls
 * vxtSmartTextSetMetrics() gets byte-identical records to before. See vxtSmartTextSetMetrics() in the header. */
static int8_t  text_rate_h = VXT_TEXT_HORIZ_DEFAULT;    /* rate units per glyph-grid unit, along the reading direction */
static int8_t  text_rate_v = VXT_TEXT_VERT_DEFAULT;     /* ... and across it                                            */
static int8_t  text_advance = VXT_TEXT_WIDTH + VXT_TEXT_GAP;   /* glyph-grid units from one cell origin to the next     */
static int8_t  text_space_advance = VXT_TEXT_WIDTH + VXT_TEXT_GAP;   /* the same for a space                            */
#define VXT_TEXT_HORIZ text_rate_h
#define VXT_TEXT_VERT  text_rate_v

static const VxtFontGlyph *base_table;    static unsigned base_count;
static const VxtFontGlyph *overlay_table; static unsigned overlay_count;
static VxtFontLigatureFn   overlay_ligature;
static int                 overlay_active;

void vxtSmartTextSetFontTable(const VxtFontGlyph *table, unsigned count)
{
    base_table = table;
    base_count = count;
}

void vxtSmartTextRegisterOverlayFont(const VxtFontGlyph *table, unsigned count,
                                     VxtFontLigatureFn ligatureFn)
{
    overlay_table = table;
    overlay_count = count;
    overlay_ligature = ligatureFn;
}

static int8_t pen_x, pen_y;
static uint8_t text_intensity = 40;   /* VXT_INTENSITY_DIM's value - a
                                       * reasonable default; callers should
                                       * set explicitly via vxtSmartTextSetIntensity() */

/* Tracks which SmartList "run" is currently open, so consecutive strokes of
 * the same kind (move-then-move, or draw-then-draw) chain via SM_continue_d
 * (cheap - no shift-register touch) rather than each paying a fresh
 * SM_startMove_d/SM_startDraw_d. -1 = none open yet (forces a fresh start
 * on the very first stroke). */
#define MODE_NONE  -1
#define MODE_MOVE   0
#define MODE_DRAW   1
static int run_mode = MODE_NONE;

/*
 * units (the same space vxtSmartTextBegin()'s origin*VXT_TEXT_POS_SCALE lives
 * in), so vxtSmartTextBeginHop() can reach the next string with one short
 * blanked move instead of a recenter + long move from the centre. Updated by
 * every move/draw this module emits; only meaningful while the LAST beam
 * operation was this module's own (the caller's contract - see BeginHop). */
static int32_t txt_beam_y, txt_beam_x;

/* See vxtSmartTextSetScale()'s own header comment
 * (vxt_smart_text.h) for why this exists and the nop-padding safety note.
 * Replaces the fixed VXT_TEXT_SCALE constant everywhere it used to be used
 * directly for a stroke/advance move - defaults to that same value, so no
 * existing caller's output changes until one explicitly calls
 * vxtSmartTextSetScale(). */
static uint8_t text_scale = VXT_TEXT_SCALE;

void vxtSmartTextSetScale(uint8_t scale)
{
    text_scale = scale;
}

void vxtSmartTextSetMetrics(uint8_t scale, int8_t rateV, int8_t rateH, uint8_t advance, uint8_t spaceAdvance)
{
    text_scale = scale;
    text_rate_v = rateV;
    text_rate_h = rateH;
    text_advance = (int8_t)advance;
    text_space_advance = (int8_t)spaceAdvance;
}

void vxtSmartTextSetFont(uint8_t font)
{
    overlay_active = (font == VXT_TEXT_FONT_OVERLAY) && overlay_table;
}

void vxtSmartTextSetIntensity(uint8_t intensity)
{
    text_intensity = intensity;
}

static const VxtFontGlyph *findInTable(const VxtFontGlyph *table, unsigned count, char c)
{
    unsigned i;
    for (i = 0; i < count; i++) {
        if (table[i].ch == c) return &table[i];
    }
    return 0;
}

/* Overlay first (if active), base table as fallback - unmapped in the
 * overlay (digits, punctuation it omits) resolves from the base font,
 * same rule vxtSmartTextChar() below follows. */
static const VxtFontGlyph *findGlyph(char c)
{
    const VxtFontGlyph *g;
    if (overlay_active) {
        g = findInTable(overlay_table, overlay_count, c);
        if (g) return g;
    }
    return findInTable(base_table, base_count, c);
}

/* See vxt_smart_text.h for the full rationale. Read-only accessor to the
 * currently active font's glyph stroke table (overlay-then-base, same
 * fallback vxtSmartTextChar() uses), for a caller that needs each
 * character's raw local-space strokes to transform/project as its own
 * geometry, rather than going through this module's own flat, fixed-scale
 * pen. Deliberately does not do ligature (digraph) merging the way
 * vxtSmartTextStr() does - a caller consuming one character at a time for
 * its own per-character projection draws a digraph pair as two separate
 * single-letter glyphs here instead of one ligature glyph. */
uint8_t vxtSmartTextGlyphStrokes(char c, int8_t strokesOut[VXT_TEXT_MAX_STROKES][4])
{
    unsigned i;
    const VxtFontGlyph *g = findGlyph(c);

    if (!g) return 0;
    for (i = 0; i < g->nstrokes; i++) {
        strokesOut[i][0] = g->strokes[i].x1;
        strokesOut[i][1] = g->strokes[i].y1;
        strokesOut[i][2] = g->strokes[i].x2;
        strokesOut[i][3] = g->strokes[i].y2;
    }
    return g->nstrokes;
}

/*h for why this exists.
 *
 * Rotation is applied to the EMITTED deltas only, at the single choke point
 * both smartMoveTo() and smartDrawTo() pass through - which is also how the
 * per-character advance gets rotated for free, since that advance is itself
 * a smartMoveTo() call (see vxtSmartTextChar()'s tail). Glyph coordinates
 * and pen_x/pen_y stay in unrotated glyph space, which is what makes this
 * a ~10-line change rather than a second copy of the font.
 *
 * VXT_TEXT_ORIENT_HORIZ takes the early return, so the default path emits
 * byte-identical records to before this function existed. */
static uint8_t text_orient = VXT_TEXT_ORIENT_HORIZ;

void vxtSmartTextSetOrientation(uint8_t orient)
{
    text_orient = (orient > VXT_TEXT_ORIENT_180) ? VXT_TEXT_ORIENT_HORIZ : orient;
}

static void applyOrient(int8_t *dy, int8_t *dx)
{
    int8_t y = *dy, x = *dx;

    switch (text_orient) {
    case VXT_TEXT_ORIENT_CW:   *dy = (int8_t)-x; *dx = y;            break;
    case VXT_TEXT_ORIENT_CCW:  *dy = x;          *dx = (int8_t)-y;   break;
    case VXT_TEXT_ORIENT_180:  *dy = (int8_t)-y; *dx = (int8_t)-x;   break;
    default:                                                         break;
    }
}

static void smartMoveTo(int8_t x, int8_t y)
{
    int8_t dy, dx;
    if (x == pen_x && y == pen_y) return;   /* same skip as vxt_text's moveTo */
    dy = (int8_t)((y - pen_y) * VXT_TEXT_VERT);
    dx = (int8_t)((x - pen_x) * VXT_TEXT_HORIZ);
    applyOrient(&dy, &dx);
    if (run_mode == MODE_MOVE) vxtSmartCont(dy, dx);
    else { vxtSmartMove(dy, dx); run_mode = MODE_MOVE; }
    txt_beam_y += (int32_t)dy * text_scale;
    txt_beam_x += (int32_t)dx * text_scale;
    pen_x = x; pen_y = y;
}

static void smartDrawTo(int8_t x, int8_t y)
{
    int8_t dy, dx;
    dy = (int8_t)((y - pen_y) * VXT_TEXT_VERT);
    dx = (int8_t)((x - pen_x) * VXT_TEXT_HORIZ);
    applyOrient(&dy, &dx);
    if (run_mode == MODE_DRAW) vxtSmartCont(dy, dx);
    else {
        /* A fresh draw run needs the CURRENT text_intensity applied. Unlike
         * vxt_draw's format (intensity travels WITH every record), SmartList
         * separates intensity into its own SM_setIntensity record - so it is
         * (re-)emitted here, once per draw-run-start, change-gated inside
         * vxtSmartIntensity() itself (a no-op if already the active value -
         * see vxt_smart.c). This mirrors the box-grid pattern exactly. */
        vxtSmartIntensity(text_intensity);
        vxtSmartDraw(dy, dx);
        run_mode = MODE_DRAW;
    }
    txt_beam_y += (int32_t)dy * text_scale;
    txt_beam_x += (int32_t)dx * text_scale;
    pen_x = x; pen_y = y;
}

/* PER-character skew compensation. Closes a real gap:
 * this module draws through its OWN smartMoveTo()/smartDrawTo() chain,
 * entirely independent of gamelib_beam.c - so `gamelibBeamSetDrawGain()`
 * (a game's first applied calibration correction) never touched text at
 * all, even though the calibration rig's TEXT H/TEXT V screens measured
 * text as the LARGEST error found so far: a 24-character string ends up
 * roughly 1,800 units low and 950 right of its ideal end point - about
 * three character heights.
 *
 * String STARTS measured clean (the vxtSmartTextBegin() reposition is fine);
 * the error accumulates per character as the string draws, at a fairly
 * consistent ~-80 Y / +40 X per glyph (calib.2's 16- and 24-char rows agree
 * to within a few percent). That per-CHARACTER-count behaviour, not
 * per-drawn-DISTANCE, is why this needs its own mechanism rather than
 * reusing gamelib_beam.c's gain (which is fitted as % of distance and
 * confirmed roughly constant over an 8x length range on plain lines - a
 * different, and differently-caused, error).
 *
 * Applied once per character, folded directly into the existing once-per-
 * glyph advance move (smartAdvanceChar(), below) - so this costs ZERO extra
 * records (convention 18): the correction changes the numbers inside a
 * record that was already being emitted, exactly the same technique as the
 * gamelib_beam.c gain fix.
 *
 * Expressed in LOCAL (pre-rotation) space - "cross" is the axis
 * PERPENDICULAR to reading direction, "along" is parallel to it - and
 * applied BEFORE applyOrient() rotates the delta. That means one constant
 * pair is meant to serve every orientation (_HORIZ/_CW/_CCW/_180) via the
 * same rotation the module already does, rather than needing a separate
 * fitted pair per orientation.
 *
 * Honest caveat: the (cross, along) pair below was fitted only from TEXT H
 * (horizontal) data. TEXT V's measured magnitudes (~33% along-axis error
 * vs ~7% cross-axis, over a much longer effective run since it advances at
 * VXT_TEXT_VERT not _HORIZ) do not obviously confirm this is a clean
 * rotation-invariant constant - that would need TEXT V's own numbers
 * checked against what rotating this pair 90 degrees predicts, which has
 * not been done. Treat this as a first-order correction for horizontal
 * text, unconfirmed (not contradicted, just unconfirmed) for rotated text.
 * Sticky like vxtSmartTextSetOrientation() - not reset per string. */
static int8_t text_comp_cross = 0;
static int8_t text_comp_along = 0;

void vxtSmartTextSetSkewComp(int8_t crossComp, int8_t alongComp)
{
    text_comp_cross = crossComp;
    text_comp_along = alongComp;
}

/* The once-per-glyph advance, local (0,0) -> (WIDTH+GAP, 0) - i.e. dy=0,
 * dx=(WIDTH+GAP)*HORIZ before compensation - with text_comp_cross/along
 * folded directly into that same delta, in local space, before applyOrient()
 * rotates it. Deliberately not a generic smartMoveTo() call: this is the
 * ONE specific move the compensation targets (see the comment above), not
 * every move this module makes - the intra-glyph stroke moves and the
 * "return to (0,0)" reset stay uncompensated, since only the ONE cumulative
 * per-character step is what the calibration rig measured. */
static void smartAdvanceChar(void)
{
    int8_t x = text_advance;
    int8_t dy, dx;

    if (x == pen_x && 0 == pen_y) return;
    dy = (int8_t)((0 - pen_y) * VXT_TEXT_VERT + text_comp_cross);
    dx = (int8_t)((x - pen_x) * VXT_TEXT_HORIZ + text_comp_along);
    applyOrient(&dy, &dx);
    if (run_mode == MODE_MOVE) vxtSmartCont(dy, dx);
    else { vxtSmartMove(dy, dx); run_mode = MODE_MOVE; }
    {
        /* Track the IDEAL advance, not the emitted one. text_comp_cross/along
         * exist (calibrated from calmeas.csv's TEXT H rows) precisely to cancel
         * a measured per-character drift, so the beam physically lands on the
         * uncompensated advance. Counting the compensation as travel put every
         * hop off by chars x comp - seen on hardware 2026-10-10 as the score row
         * sitting right of the timer row. Follows whatever calibration the
         * console loaded; nothing hardcoded. */
        int8_t iy = (int8_t)((0 - pen_y) * VXT_TEXT_VERT);
        int8_t ix = (int8_t)((x - pen_x) * VXT_TEXT_HORIZ);
        applyOrient(&iy, &ix);
        txt_beam_y += (int32_t)iy * text_scale;
        txt_beam_x += (int32_t)ix * text_scale;
    }
    pen_x = x; pen_y = 0;
}

/* Closes an open draw run so that whatever the caller emits next may Recenter safely (see the confirmed hardware fix in vxtSmartTextBegin). */
void vxtSmartTextEnd(void)
{
    if (run_mode == MODE_DRAW) {
        vxtSmartMove(0, 0);
    }
    run_mode = MODE_NONE;
}

void vxtSmartTextBegin(int16_t originY, int16_t originX)
{
    /* Confirmed hardware fix (found via an independent game diagnostic):
     * vxtSmartRecenter() called immediately
     * after an OPEN (unclosed) draw run leaks a brief unintended visible
     * stroke. This module's own run_mode still reflects its last stroke
     * (this variable is not reset per-frame, only at the end of THIS
     * function) - close it before recentering if so. Applies directly to
     * Test 6c, which calls vxtSmartTextBegin() 4 times per frame
     * (LINES=/CH1=/CH2=/CH3=), each one following the previous text's own
     * open draw. */
    if (run_mode == MODE_DRAW) {
        vxtSmartMove(0, 0);
    }

    /* Recenter + position, at POS_SCALE - reuses SM_startMoveBig_d via
     * vxtSmartMoveBig, the same scale-32-safe routine already built and
     * verified for the box grid's row repositioning (see vxt_smart.h). */
    vxtSmartRecenter();
    vxtSmartScale(VXT_TEXT_POS_SCALE);
    vxtSmartMoveBig(originY * (int32_t)VXT_TEXT_POS_SCALE,
                    originX * (int32_t)VXT_TEXT_POS_SCALE,
                    VXT_TEXT_POS_SCALE);
    vxtSmartScale(text_scale);   /* strokes run at this scale from here on */
    txt_beam_y = (int32_t)originY * VXT_TEXT_POS_SCALE;   /* exact: a multiple */
    txt_beam_x = (int32_t)originX * VXT_TEXT_POS_SCALE;   /* of the move scale */
    pen_x = 0; pen_y = 0;
    run_mode = MODE_NONE;   /* force a fresh start/draw for the first stroke */
}

/*
 * at VXT_TEXT_POS_SCALE from where the previous string left the beam. For a run
 * of strings drawn back to back (a HUD score block), this replaces each string's
 * recenter + long move out from the centre with one short hop.
 *
 * CONTRACT: the last beam operation must have been this module's own (a string
 * drawn since a Begin/BeginHop) - anything drawn in between moves the beam
 * without this module knowing. Text placement uses the same uncorrected moves
 * vxtSmartTextBegin() does, so a hop lands where Begin would, give or take one
 * VXT_TEXT_POS_SCALE rounding step (the hop's start is not always on that grid)
 * and whatever analog drift a skipped recenter allows - small for short hops. */
void vxtSmartTextBeginHop(int16_t originY, int16_t originX)
{
    int32_t ty = (int32_t)originY * VXT_TEXT_POS_SCALE;
    int32_t tx = (int32_t)originX * VXT_TEXT_POS_SCALE;
    int32_t dy = ty - txt_beam_y, dx = tx - txt_beam_x;
    int32_t ry = dy >= 0 ? (dy + VXT_TEXT_POS_SCALE / 2) / VXT_TEXT_POS_SCALE
                         : -((-dy + VXT_TEXT_POS_SCALE / 2) / VXT_TEXT_POS_SCALE);
    int32_t rx = dx >= 0 ? (dx + VXT_TEXT_POS_SCALE / 2) / VXT_TEXT_POS_SCALE
                         : -((-dx + VXT_TEXT_POS_SCALE / 2) / VXT_TEXT_POS_SCALE);

    if (run_mode == MODE_DRAW) {
        vxtSmartMove(0, 0);   /* close the open draw run, as Begin does */
    }
    vxtSmartScale(VXT_TEXT_POS_SCALE);
    vxtSmartMoveBig(dy, dx, VXT_TEXT_POS_SCALE);
    vxtSmartScale(text_scale);
    txt_beam_y += ry * VXT_TEXT_POS_SCALE;   /* where the move really lands */
    txt_beam_x += rx * VXT_TEXT_POS_SCALE;
    pen_x = 0; pen_y = 0;
    run_mode = MODE_NONE;
}

/* Numbers stay on the base font even when an overlay is active: a digit
 * lookup misses the overlay (an alphabet overlay typically has no '0'-'9'
 * entries) and falls through to the base table via findGlyph(),
 * which is exactly the desired behavior, not a bug being masked. */
void vxtSmartTextChar(char c)
{
    unsigned i;
    const VxtFontGlyph *g = findGlyph(c);

    if (g) {
        for (i = 0; i < g->nstrokes; i++) {
            smartMoveTo(g->strokes[i].x1, g->strokes[i].y1);
            smartDrawTo(g->strokes[i].x2, g->strokes[i].y2);
        }
    }
    smartMoveTo(0, 0);
    if (c == ' ' && text_space_advance != text_advance) {
        /* a space with its own (shorter) advance - the same move, without the skew compensation (nothing was drawn) */
        int8_t dy = 0, dx = (int8_t)(text_space_advance * VXT_TEXT_HORIZ);
        applyOrient(&dy, &dx);
        if (run_mode == MODE_MOVE) vxtSmartCont(dy, dx);
        else { vxtSmartMove(dy, dx); run_mode = MODE_MOVE; }
        pen_x = 0; pen_y = 0;
        return;
    }
    smartAdvanceChar();   /* folds the per-character skew compensation
                          * into this same record, see smartAdvanceChar()'s
                          * own comment. */
    pen_x = 0; pen_y = 0;
}

/* When the active overlay font supplies a ligature lookup, greedily
 * checks each position for one of its multi-character sequences BEFORE
 * falling back to a single-character lookup - so "THE" draws as [Th][E],
 * two glyphs, not [T][H][E], three, when such an overlay is active. A font with
 * no ligature function (or no overlay active) skips straight to the
 * per-character loop, byte-identical to before ligatures existed. */
void vxtSmartTextStr(const char *s)
{
    while (*s) {
        if (overlay_active && overlay_ligature && s[1]) {
            const VxtFontStroke *as;
            uint8_t n = overlay_ligature(s[0], s[1], &as);
            if (n > 0) {
                unsigned i;
                for (i = 0; i < n; i++) {
                    smartMoveTo(as[i].x1, as[i].y1);
                    smartDrawTo(as[i].x2, as[i].y2);
                }
                smartMoveTo(0, 0);
                smartAdvanceChar();
                pen_x = 0; pen_y = 0;
                s += 2;
                continue;
            }
        }
        vxtSmartTextChar(*s++);
    }
}

void vxtSmartTextNumber(int16_t value)
{
    char buf[5];
    int i = 0;
    uint16_t mag;
    int neg = (value < 0);

    mag = (uint16_t)(neg ? -value : value);
    if (mag > 999) mag = 999;

    if (mag >= 100) buf[i++] = (char)('0' + mag / 100), mag = (uint16_t)(mag % 100);
    if (i || mag >= 10) buf[i++] = (char)('0' + mag / 10), mag = (uint16_t)(mag % 10);
    buf[i++] = (char)('0' + mag);
    buf[i] = '\0';

    if (neg) vxtSmartTextChar('-');
    vxtSmartTextStr(buf);
}

/* See this function's own header comment in
 * vxt_smart_text.h for why it's separate from vxtSmartTextNumber() rather
 * than widening that one. Same digit-printing shape, just int32_t/5
 * digits instead of int16_t/3. */
void vxtSmartTextNumber32(int32_t value)
{
    char buf[6];
    int i = 0;
    uint32_t mag;
    int neg = (value < 0);

    mag = (uint32_t)(neg ? -value : value);
    if (mag > 99999UL) mag = 99999UL;

    if (mag >= 10000UL) buf[i++] = (char)('0' + mag / 10000UL), mag = mag % 10000UL;
    if (i || mag >= 1000UL) buf[i++] = (char)('0' + mag / 1000UL), mag = mag % 1000UL;
    if (i || mag >= 100UL) buf[i++] = (char)('0' + mag / 100UL), mag = mag % 100UL;
    if (i || mag >= 10UL) buf[i++] = (char)('0' + mag / 10UL), mag = mag % 10UL;
    buf[i++] = (char)('0' + mag);
    buf[i] = '\0';

    if (neg) vxtSmartTextChar('-');
    vxtSmartTextStr(buf);
}

int32_t vxtSmartTextWidthPhys(int nchars)
{
    /* Real bug found from hardware feedback ("HUD text is
     * skewed wrong"): this formula computed the NOMINAL per-character
     * advance, with no knowledge that smartAdvanceChar() (added earlier
     * for skew compensation) can emit a different actual advance per
     * character. Every caller that centers or right-aligns text
     * (a menu title/options, HUD readouts, showcase titles) was
     * therefore computing its origin from a width that no longer matched
     * what got drawn - the same assumed-vs-achieved mismatch this
     * project root-caused once already elsewhere, this time
     * self-inflicted by the skew-comp addition. Different callers use this
     * differently (HUD right-aligns, menu/showcase center), which is why
     * the two symptoms looked different on hardware despite one root cause.
     *
     * Now reflects the ACTUAL per-character advance including whatever skew
     * compensation is currently set - text_comp_along is 0 by default
     * (identity), so this is byte-identical to the old formula until a
     * caller sets a nonzero compensation. */
    return (int32_t)nchars * (text_advance * VXT_TEXT_HORIZ
                              + text_comp_along) * text_scale;
}
