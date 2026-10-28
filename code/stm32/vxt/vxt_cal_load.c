/*
 * vxt_cal_load.c - see vxt_cal_load.h for what this is and why.
 */
#include "vxt_cal_load.h"
#include "../fatfs/ff.h"
#include <math.h>   /* sqrt - ANGLE needs the spoke radius */
#include <string.h> /* strcmp - is the active source the rig's own file? */

/* Mirrors vxt_cal.c's own CAL_TEXT_LEN[] (the rig's 8/16/24-char TEXT H
 * test rows) - kept in sync by hand, same tradeoff this project already
 * accepts for small private tables (e.g. vxt_smart_text.c's own
 * VXT_TEXT_SCALE) rather than exporting a rig-internal constant for one
 * caller. */
static const int VXT_CAL_LOAD_TEXT_LEN[3] = { 8, 16, 24 };

/* Mirrors vxt_cal.c's own CAL_TEXT_COMP_SCALE_DIV (== vxt_smart_text.c's
 * private VXT_TEXT_SCALE) - same "kept in sync by hand" note. */
#define VXT_CAL_LOAD_SCALE_DIV  8

/* ---------------------------------------------------------------------------
 * Active source - see vxtCalLoadSetSource() in the header for why the file
 * and the chord pad travel together, and why every consumer must set this
 * explicitly at boot instead of inheriting whatever the previous cart left.
 * Defaults to the rig's own file so an unconverted caller behaves as before.
 * ------------------------------------------------------------------------ */
static const char *vxt_cal_csv = VXT_CAL_RIG_FILE;
static const char *vxt_cal_tmp = VXT_CAL_RIG_TMP;
static int32_t     vxt_cal_chord_pad = VXT_CAL_RIG_CHORD_PAD;
/* Only the rig stamps a `method` column (see VXT_CAL_TEXTH_METHOD), so only
 * the rig's own file is checked for stale TEXT H rows. A game's Cal screen
 * writes one row per correction and has no older method to go stale. */
static int         vxt_cal_is_rig = 1;

void vxtCalLoadSetSource(const char *csvPath, const char *tmpPath,
                         int32_t chordPadR)
{
    vxt_cal_csv = csvPath ? csvPath : VXT_CAL_RIG_FILE;
    vxt_cal_tmp = tmpPath ? tmpPath : VXT_CAL_RIG_TMP;
    vxt_cal_chord_pad = chordPadR;
    vxt_cal_is_rig = (strcmp(vxt_cal_csv, VXT_CAL_RIG_FILE) == 0);
}

/* Parses one comma- or newline-terminated integer field starting at *pos,
 * advances *pos past the field AND its trailing comma (if any) - so a
 * caller can chain calls back to back across a whole CSV row. Identical
 * technique to vxt_cal.c's own calParseField() - not shared directly
 * since that one is `static` to its own translation unit and this reader
 * intentionally has no other dependency on vxt_cal.c/vxt_cal.h (a game
 * cart should not need to link the calibration rig's own state machine
 * just to read the log file it produces). */
static int32_t vxtCalLoadParseField(const char *s, int *pos)
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

int vxtCalLoadTextComp(int8_t *cross, int8_t *along)
{
    FIL f;
    char line[96];
    int first = 1, n = 0;
    float sumCross = 0.0f, sumAlong = 0.0f;
    /* Each row's START reading, so its END reading can be reduced to the
     * error of the run alone - see the subtraction below. The rig writes a
     * row's start before its end (the file is sorted by ref). */
    int32_t startDy[8][3], startDx[8][3];
    uint8_t startOk[8][3];

    memset(startOk, 0, sizeof(startOk));

    if (f_open(&f, vxt_cal_csv, FA_READ) != FR_OK) return 0;   /* no
                                    * calibration data on THIS SD card -
                                    * caller defaults to identity, per
                                    * this function's own header comment */

    while (f_gets(line, (int)sizeof(line), &f) != 0) {
        int pos;
        int32_t variant, ref, card, dy, dx, compY, compX, method;
        int row, k;

        if (first) { first = 0; continue; }   /* header row */

        /* Only "TEXT H," rows matter here - a literal prefix match, much
         * simpler than vxt_cal.c's own full screen-name table since this
         * reader only ever cares about one screen. */
        if (line[0] != 'T' || line[1] != 'E' || line[2] != 'X' || line[3] != 'T'
            || line[4] != ' ' || line[5] != 'H' || line[6] != ',') {
            continue;
        }

        pos = 7;   /* right after "TEXT H," */
        variant = vxtCalLoadParseField(line, &pos);   /* the average is
                                    * taken across every screen position;
                                    * variant only pairs a start with its end */
        ref   = vxtCalLoadParseField(line, &pos);
        card  = vxtCalLoadParseField(line, &pos);
        (void)vxtCalLoadParseField(line, &pos);   /* refY - not needed */
        (void)vxtCalLoadParseField(line, &pos);   /* refX - not needed */
        dy    = vxtCalLoadParseField(line, &pos);
        dx    = vxtCalLoadParseField(line, &pos);
        (void)vxtCalLoadParseField(line, &pos);   /* records - not needed */
        compY = vxtCalLoadParseField(line, &pos);
        compX = vxtCalLoadParseField(line, &pos);
        /* skip nrefs, drawgain, movegain, movesettle, session, fw */
        for (k = 0; k < 6; k++) (void)vxtCalLoadParseField(line, &pos);
        method = vxtCalLoadParseField(line, &pos);   /* 0 if absent */

        /* A rig row measured with an older TEXT H method (the all-'8'
         * strings, which overcorrected real text ~2.5x) is stale - skipped,
         * never averaged with current rows. */
        if (vxt_cal_is_rig && method != VXT_CAL_TEXTH_METHOD) continue;

        if (card != 0) continue;         /* reference-card overlay was on
                                          * for this reading - excluded,
                                          * see this module's own header
                                          * comment */
        row = (int)(ref / 2);
        if (row < 0 || row >= 3) continue;   /* defensive - malformed row */
        if ((ref & 1) == 0) {            /* START reference - no known
                                          * character count, only END
                                          * references carry one. Kept
                                          * to reduce its END to the run. */
            if (variant >= 0 && variant < 8) {
                startDy[variant][row] = dy;
                startDx[variant][row] = dx;
                startOk[variant][row] = 1;
            }
            continue;
        }

        /* The first glyph's own landing error is a fixed offset, not
         * per-character drift - dividing it by the character count inflated
         * the short rows (implied 14/12/10.7 for 8/16/24 chars on real data,
         * a flat 11.2/11.4/10.0 with it removed). Skew comp never moves the
         * first glyph, so the start reading is valid either way. A game's Cal
         * screen writes no start row, so its value is used unchanged. */
        if (variant >= 0 && variant < 8 && startOk[variant][row]) {
            dy -= startDy[variant][row];
            dx -= startDx[variant][row];
        }

        /* Real bug found by applying a lesson learned elsewhere back to
         * this older, already-deployed loader. `dy` alone conflates a RAW
         * reading with a RESIDUAL: vxt_cal.c's own converge loop (button
         * 2, "corr on/OFF") lets a real rig session record a TEXT H row
         * while a correction is already applied - when it does, dy is
         * only the LEFTOVER error, and the row's own `compY`/`compX`
         * field records what was active (see vxt_cal.c's own
         * CalMeas.compY comment - "record THAT one here, or the row would
         * be stamped with an unrelated geometry screen's value"). This
         * reader never read compY/compX at all until now, so any
         * historical row recorded with Corr ON has been treated
         * as if the whole error were that small residual on every 6809
         * boot since this field was added - exactly the bug this fix
         * addresses, just never checked here before now.
         *
         * Unit note, easy to get wrong: unlike vxtCalLoadDrawGain()/
         * ClosureComp()'s own compY/compX (a raw physical delta, same
         * units as dy - straight `dy-compY` subtraction is correct
         * THERE), TEXT H's compY/compX is a per-character RATE (same
         * units cal_text_comp_cross/along themselves hold), per vxt_cal.c's
         * own comment - `dy` needs the same /len/DIV scaling compY does
         * not need. The correct reconstruction is
         * `compY + (-dy/len/DIV)`, not `(dy-compY)/len/DIV` (that would
         * mix a rate and a physical delta in one subtraction).
         * Verified this reduces to the ORIGINAL formula when compY=0
         * (every Corr-OFF row, and every row vxtCalSaveRow() writes,
         * which always writes compY=0 and bakes the full value into dy
         * instead) - so this fix is safe for both old and new data. */
        sumCross += (float)compY - (float)dy / (float)VXT_CAL_LOAD_TEXT_LEN[row] / (float)VXT_CAL_LOAD_SCALE_DIV;
        sumAlong += (float)compX - (float)dx / (float)VXT_CAL_LOAD_TEXT_LEN[row] / (float)VXT_CAL_LOAD_SCALE_DIV;
        n++;
    }
    f_close(&f);

    if (n == 0) return 0;   /* file existed but had no usable TEXT H data */

    {
        int32_t c = (int32_t)(sumCross / (float)n + (sumCross >= 0.0f ? 0.5f : -0.5f));
        int32_t a = (int32_t)(sumAlong / (float)n + (sumAlong >= 0.0f ? 0.5f : -0.5f));
        if (c > 127) c = 127;
        if (c < -128) c = -128;
        if (a > 127) a = 127;
        if (a < -128) a = -128;
        *cross = (int8_t)c;
        *along = (int8_t)a;
    }
    return 1;
}

/* ===========================================================================
 * PHASE B - the two geometry corrections. See vxt_cal_load.h.
 * ======================================================================== */

/* Shared row filter. Returns 1 and fills the parsed fields if the line is a
 * usable row of `want`, else 0.
 *
 * "Usable" means: right screen, and drawgain == 1000. That second test is the
 * whole point of the provenance columns - a legacy row parses those as 0,
 * which is not a legal gain, so it fails here rather than * contributing a reading taken through an unknown correction. */
static int vxtCalLoadRow(const char *line, const char *want,
                         int *variant, int *ref, int *card,
                         int32_t *refY, int32_t *refX,
                         int32_t *dy, int32_t *dx,
                         int32_t *compY, int32_t *compX)
{
    int pos = 0, i;
    for (i = 0; want[i]; i++) if (line[i] != want[i]) return 0;
    if (line[i] != ',') return 0;
    pos = i + 1;

    *variant = (int)vxtCalLoadParseField(line, &pos);
    *ref     = (int)vxtCalLoadParseField(line, &pos);
    *card    = (int)vxtCalLoadParseField(line, &pos);
    *refY    = vxtCalLoadParseField(line, &pos);
    *refX    = vxtCalLoadParseField(line, &pos);
    *dy      = vxtCalLoadParseField(line, &pos);
    *dx      = vxtCalLoadParseField(line, &pos);
    (void)vxtCalLoadParseField(line, &pos);           /* records */
    *compY   = vxtCalLoadParseField(line, &pos);
    *compX   = vxtCalLoadParseField(line, &pos);
    (void)vxtCalLoadParseField(line, &pos);           /* nrefs   */
    return (vxtCalLoadParseField(line, &pos) == 1000);   /* drawgain */
}

/* MEDIAN, not mean. A calibration log is field data: it contains readings
 * taken mid-converge, with a caret parked slightly wrong, or on a screen whose
 * geometry has since moved. One such row can drag a mean well off - dry-running
 * this against a real log, two bad CHORD variants pulled the closure fit from
 * 3.5% to 4.0%, past the value already confirmed on hardware. A median ignores
 * them and reproduced 3.5% exactly. Insertion sort: n is at most a few dozen. */
#define VXT_CAL_LOAD_MAX_SAMP  64

static float vxtCalLoadMedian(float *v, int n)
{
    int i, j;
    for (i = 1; i < n; i++) {
        float k = v[i];
        for (j = i - 1; j >= 0 && v[j] > k; j--) v[j + 1] = v[j];
        v[j + 1] = k;
    }
    return (n & 1) ? v[n / 2] : 0.5f * (v[n / 2 - 1] + v[n / 2]);
}

int vxtCalLoadDrawGain(int16_t *per1000)
{
    FIL f;
    char line[160];
    int first = 1, pass;
    float samp[2][VXT_CAL_LOAD_MAX_SAMP];
    int   n[2]   = { 0, 0 };

    if (f_open(&f, vxt_cal_csv, FA_READ) != FR_OK) return 0;
    while (f_gets(line, (int)sizeof(line), &f) != 0) {
        int v, r, card;
        int32_t ry, rx, dy, dx, cy, cx;
        float er, rad;
        if (first) { first = 0; continue; }
        if (!vxtCalLoadRow(line, "ANGLE", &v, &r, &card, &ry, &rx, &dy, &dx, &cy, &cx))
            continue;
        /* Spoke 0 is the FIRST figure drawn and carries the whole
         * first-element displacement - excluded from the card-off fit, kept
         * for card-on where the card has already absorbed it. */
        if (!card && r == 0) continue;
        rad = (float)ry * (float)ry + (float)rx * (float)rx;
        if (rad < 1.0f) continue;
        rad = (float)sqrt((double)rad);
        /* radial component of the RAW error, as a fraction of the radius */
        er = (((float)(dy - cy) * (float)ry) + ((float)(dx - cx) * (float)rx)) / rad;
        {
            int b = card ? 1 : 0;
            if (n[b] < VXT_CAL_LOAD_MAX_SAMP) samp[b][n[b]++] = er / rad;
        }
    }
    f_close(&f);

    /* Prefer the card-on fit - it isolates the draw term. 6 is half a full
     * 12-spoke sweep: enough to average, low enough to accept a partial one. */
    pass = (n[1] >= 6) ? 1 : 0;
    if (n[pass] == 0) pass = n[1] ? 1 : 0;
    if (n[pass] == 0) return 0;

    {
        float e = vxtCalLoadMedian(samp[pass], n[pass]);   /* e.g. -0.0560 */
        float g;
        if (e <= -0.5f || e >= 0.5f) return 0;      /* nonsense - refuse */
        g = 1000.0f / (1.0f + e);
        if (g < 700.0f)  g = 700.0f;               /* same clamp gamelib uses */
        if (g > 1300.0f) g = 1300.0f;
        *per1000 = (int16_t)(g + 0.5f);
    }
    return 1;
}

/* CHORD's measuring stubs sit CAL_ACCUM_PAD_R units radially outside the
 * circle, so a row's own refX recovers the radius: |refX| = R + PAD_R. Kept in
 * sync by hand with vxt_cal.c's CAL_ACCUM_PAD_R, same tradeoff as
 * VXT_CAL_LOAD_TEXT_LEN above - and guarded below, since a layout change would
 * make the derived radius implausible rather than subtly wrong. */
/* Superseded by vxtCalLoadSetSource()'s chordPadR - the rig's own value
 * now lives in the header as VXT_CAL_RIG_CHORD_PAD, because a game Cal
 * screen pads by a different amount and assuming the rig's rescales every
 * closure it reconstructs from that game's rows. */

int vxtCalLoadClosureComp(int16_t *per1000)
{
    FIL f;
    char line[160];
    int first = 1, i;
    /* one slot per CHORD variant (7 geometry x 2 card twins) */
    int32_t r0dx[14], r1dx[14];
    int8_t  has0[14], has1[14];
    int32_t rad[14];
    float samp[14];
    int n = 0;

    for (i = 0; i < 14; i++) { has0[i] = has1[i] = 0; rad[i] = 0; }

    if (f_open(&f, vxt_cal_csv, FA_READ) != FR_OK) return 0;
    while (f_gets(line, (int)sizeof(line), &f) != 0) {
        int v, r, card;
        int32_t ry, rx, dy, dx, cy, cx, R;
        if (first) { first = 0; continue; }
        if (!vxtCalLoadRow(line, "CHORD", &v, &r, &card, &ry, &rx, &dy, &dx, &cy, &cx))
            continue;
        if (v < 0 || v >= 14 || (r != 0 && r != 1)) continue;
        R = (rx < 0 ? -rx : rx) - vxt_cal_chord_pad;
        if (R < 1000 || R > 12000) continue;   /* layout moved - do not guess */
        rad[v] = R;
        if (r == 0) { r0dx[v] = dx - cx; has0[v] = 1; }
        else        { r1dx[v] = dx - cx; has1[v] = 1; }
    }
    f_close(&f);

    for (i = 0; i < 14; i++) {
        if (!has0[i] || !has1[i]) continue;    /* need both halves of A */
        /* A = R1 - R0, as a fraction of the chord (= 2R). The CORRECTION is
         * its negation, which is what the game applies. */
        samp[n++] = -(float)(r1dx[i] - r0dx[i]) / (2.0f * (float)rad[i]);
    }
    if (n == 0) return 0;

    {
        float c = vxtCalLoadMedian(samp, n);    /* e.g. +0.0353 */
        if (c <= -0.25f || c >= 0.25f) return 0;   /* nonsense - refuse */
        *per1000 = (int16_t)(c * 1000.0f + (c >= 0.0f ? 0.5f : -0.5f));
    }
    return 1;
}

/* CENTRE has exactly one reference (CAL_REFS[CAL_SCR_CENTRE] == 1), always
 * ref 0, always at nominal (0,0) - so unlike TEXT H/CHORD, no per-length or
 * per-radius division is needed: a row's own (dy,dx) directly IS the
 * measured offset. Averaged (median) across every usable variant/card
 * combination found - a pure DAC-zero shift should not depend on which
 * reference overlay happened to be on screen when it was measured. */
int vxtCalLoadOffset(int16_t *offY, int16_t *offX)
{
    FIL f;
    char line[160];
    int first = 1, i;
    float sampY[VXT_CAL_LOAD_MAX_SAMP], sampX[VXT_CAL_LOAD_MAX_SAMP];
    int n = 0;

    if (f_open(&f, vxt_cal_csv, FA_READ) != FR_OK) return 0;
    while (f_gets(line, (int)sizeof(line), &f) != 0) {
        int v, r, card;
        int32_t ry, rx, dy, dx, cy, cx;
        if (first) { first = 0; continue; }
        if (!vxtCalLoadRow(line, "CENTRE", &v, &r, &card, &ry, &rx, &dy, &dx, &cy, &cx))
            continue;
        if (r != 0) continue;              /* only reference CENTRE has */
        if (ry != 0 || rx != 0) continue;  /* layout moved - do not guess */
        if (n < VXT_CAL_LOAD_MAX_SAMP) {
            sampY[n] = (float)(dy - cy);
            sampX[n] = (float)(dx - cx);
            n++;
        }
    }
    f_close(&f);
    if (n == 0) return 0;

    {
        /* vxtCalLoadMedian() sorts in place, so a COPY of Y is taken before
         * X reuses the same working array - both need the pre-sort n rows. */
        float tmp[VXT_CAL_LOAD_MAX_SAMP];
        float my, mx;
        for (i = 0; i < n; i++) tmp[i] = sampY[i];
        my = vxtCalLoadMedian(tmp, n);
        for (i = 0; i < n; i++) tmp[i] = sampX[i];
        mx = vxtCalLoadMedian(tmp, n);

        if (my > 2000.0f || my < -2000.0f) return 0;   /* nonsense - refuse,
                                    * same clamp gamelibBeamSetOffset() uses */
        if (mx > 2000.0f || mx < -2000.0f) return 0;
        *offY = (int16_t)(my + (my >= 0.0f ? 0.5f : -0.5f));
        *offX = (int16_t)(mx + (mx >= 0.0f ? 0.5f : -0.5f));
    }
    return 1;
}

/* HOP - see the header. Rows are paired per geometry variant (refs 0 and 1),
 * so the layout check below re-derives each ref's ideal position from the
 * shared VXT_CAL_HOP_* geometry: a row whose refY/refX no longer matches came
 * from a different layout and is skipped rather than reinterpreted. */
int vxtCalLoadHopDrift(const char *csvPath, int16_t dyPerHop[VXT_CAL_HOP_NVAR],
                       int16_t dxPerHop[VXT_CAL_HOP_NVAR],
                       uint8_t have[VXT_CAL_HOP_NVAR])
{
    static const uint8_t kTicks[VXT_CAL_HOP_NVAR] = VXT_CAL_HOP_TICKS;
    FIL f;
    char line[160];
    int first = 1, i, found = 0;
    int32_t ey[2][VXT_CAL_HOP_NVAR], ex[2][VXT_CAL_HOP_NVAR];
    uint8_t got[2][VXT_CAL_HOP_NVAR];

    for (i = 0; i < VXT_CAL_HOP_NVAR; i++) { got[0][i] = got[1][i] = 0; have[i] = 0; }

    if (f_open(&f, csvPath, FA_READ) != FR_OK) return 0;
    while (f_gets(line, (int)sizeof(line), &f) != 0) {
        int v, r, card, ticks;
        int32_t ry, rx, dy, dx, cy, cx, segH, wantY;
        if (first) { first = 0; continue; }
        if (!vxtCalLoadRow(line, "HOP", &v, &r, &card, &ry, &rx, &dy, &dx, &cy, &cx))
            continue;
        if (card || v < 0 || v >= VXT_CAL_HOP_NVAR || (r != 0 && r != 1)) continue;
        ticks = kTicks[v];
        segH  = VXT_CAL_HOP_HEIGHT / ticks;
        wantY = VXT_CAL_HOP_BASE_Y + (r ? (int32_t)(ticks - 1) * segH : 0) + segH / 2;
        if (ry != wantY || rx != VXT_CAL_HOP_BAR_X - VXT_CAL_HOP_HALF_W) continue;
        ey[r][v] = dy - cy;     /* RAW error: undo whatever Corr was applied */
        ex[r][v] = dx - cx;
        got[r][v] = 1;
    }
    f_close(&f);

    for (i = 0; i < VXT_CAL_HOP_NVAR; i++) {
        int32_t hops = (int32_t)kTicks[i] - 1;
        float py, px;
        if (!got[0][i] || !got[1][i]) continue;   /* need both halves of A */
        py = (float)(ey[1][i] - ey[0][i]) / (float)hops;
        px = (float)(ex[1][i] - ex[0][i]) / (float)hops;
        /* A hop is a few hundred units; a per-hop error anywhere near that is
         * a misplaced caret, not drift - refuse it. */
        if (py > 200.0f || py < -200.0f || px > 200.0f || px < -200.0f) continue;
        dyPerHop[i] = (int16_t)(py + (py >= 0.0f ? 0.5f : -0.5f));
        dxPerHop[i] = (int16_t)(px + (px >= 0.0f ? 0.5f : -0.5f));
        have[i] = 1;
        found++;
    }
    return found;
}

/* ===========================================================================
 * Write side - see vxt_cal_load.h for the full design rationale.
 * ======================================================================== */

/* Mirrors vxt_cal.c's own calAppendInt() - decimal ASCII, optional leading
 * '-'. Not shared directly (that one is `static` to its own translation
 * unit), same "small private helper, kept in sync by hand" tradeoff this
 * file already accepts for VXT_CAL_LOAD_TEXT_LEN etc. */
static int vxtCalSaveAppendInt(char *b, int i, int32_t v)
{
    char t[12];
    int k = 0;
    uint32_t m;

    if (v < 0) { b[i++] = '-'; m = (uint32_t)(-v); } else m = (uint32_t)v;
    do { t[k++] = (char)('0' + (m % 10u)); m /= 10u; } while (m);
    while (k) b[i++] = t[--k];
    return i;
}

/* Does `line` start with `screen` followed by ',', and do its next two
 * integer fields (variant, ref) equal `variant`/`ref`? Used to find the ONE
 * existing row (if any) this call is replacing. */
static int vxtCalSaveRowMatches(const char *line, const char *screen,
                                int32_t variant, int32_t ref)
{
    int pos = 0, i;
    for (i = 0; screen[i]; i++) {
        if (line[pos] != screen[i]) return 0;
        pos++;
    }
    if (line[pos] != ',') return 0;
    pos++;
    if (vxtCalLoadParseField(line, &pos) != variant) return 0;
    if (vxtCalLoadParseField(line, &pos) != ref)      return 0;
    return 1;
}

int vxtCalSaveRow(const char *screen, int32_t variant, int32_t ref,
                  int32_t refY, int32_t refX, int32_t dy, int32_t dx,
                  int32_t compY, int32_t compX, int32_t drawGain)
{
    FIL fin, fout;
    char line[160];
    char row[160];
    int rowLen, haveInput, ok;
    UINT bw;
    static const char hdr[] = "screen,variant,ref,card,refY,refX,dy,dx,records,"
                              "compy,compx,nrefs,drawgain,movegain,movesettle,"
                              "session,fw\n";

    /* Build the replacement row once - exact column order calBuildLogLine()
     * uses, see this function's own header comment for what each fixed
     * field (card/records/nrefs/movegain/movesettle/session/fw) means here. */
    rowLen = 0;
    {
        int i;
        for (i = 0; screen[i]; i++) row[rowLen++] = screen[i];
    }
    row[rowLen++] = ',';
    rowLen = vxtCalSaveAppendInt(row, rowLen, variant);
    row[rowLen++] = ',';
    rowLen = vxtCalSaveAppendInt(row, rowLen, ref);
    row[rowLen++] = ',';
    rowLen = vxtCalSaveAppendInt(row, rowLen, 0);        /* card */
    row[rowLen++] = ',';
    rowLen = vxtCalSaveAppendInt(row, rowLen, refY);
    row[rowLen++] = ',';
    rowLen = vxtCalSaveAppendInt(row, rowLen, refX);
    row[rowLen++] = ',';
    rowLen = vxtCalSaveAppendInt(row, rowLen, dy);
    row[rowLen++] = ',';
    rowLen = vxtCalSaveAppendInt(row, rowLen, dx);
    row[rowLen++] = ',';
    rowLen = vxtCalSaveAppendInt(row, rowLen, 0);        /* records */
    row[rowLen++] = ',';
    rowLen = vxtCalSaveAppendInt(row, rowLen, compY);
    row[rowLen++] = ',';
    rowLen = vxtCalSaveAppendInt(row, rowLen, compX);
    row[rowLen++] = ',';
    rowLen = vxtCalSaveAppendInt(row, rowLen, 1);        /* nrefs */
    row[rowLen++] = ',';
    rowLen = vxtCalSaveAppendInt(row, rowLen, drawGain);
    row[rowLen++] = ',';
    rowLen = vxtCalSaveAppendInt(row, rowLen, 1000);     /* movegain */
    row[rowLen++] = ',';
    rowLen = vxtCalSaveAppendInt(row, rowLen, 0);        /* movesettle */
    row[rowLen++] = ',';
    rowLen = vxtCalSaveAppendInt(row, rowLen, 0);        /* session */
    row[rowLen++] = ',';
    rowLen = vxtCalSaveAppendInt(row, rowLen, 0);        /* fw */
    row[rowLen++] = '\n';

    if (f_open(&fout, vxt_cal_tmp, FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)
        return 0;

    {
        /* Only an ABSENT file may be treated as "no prior data". Any other
         * open failure (a busy card, a host that has it mounted) says nothing
         * about whether calibration exists, and carrying on would write a
         * one-row file and then unlink+replace the real one - * wiping every other row. Refuse instead; the caller just sees a
         * failed save. */
        FRESULT fr = f_open(&fin, vxt_cal_csv, FA_READ);
        haveInput = (fr == FR_OK);
        if (!haveInput && fr != FR_NO_FILE && fr != FR_NO_PATH) {
            f_close(&fout);
            f_unlink(vxt_cal_tmp);
            return 0;
        }
    }
    ok = (f_write(&fout, hdr, sizeof(hdr) - 1, &bw) == FR_OK);

    if (haveInput) {
        int first = 1, wroteRow = 0;
        while (ok && f_gets(line, (int)sizeof(line), &fin) != 0) {
            if (first) { first = 0; continue; }   /* drop old header - ours
                                    * is already written above, and a
                                    * differently-ordered/spaced header from
                                    * an older build must not survive */
            if (!wroteRow && vxtCalSaveRowMatches(line, screen, variant, ref)) {
                ok = (f_write(&fout, row, (UINT)rowLen, &bw) == FR_OK);
                wroteRow = 1;
                continue;   /* the OLD version of this row is dropped -
                            * replaced, never duplicated */
            }
            {
                int len2 = 0;
                while (line[len2] && line[len2] != '\n' && line[len2] != '\r') len2++;
                line[len2++] = '\n';
                if (ok) ok = (f_write(&fout, line, (UINT)len2, &bw) == FR_OK);
            }
        }
        f_close(&fin);
        if (ok && !wroteRow) ok = (f_write(&fout, row, (UINT)rowLen, &bw) == FR_OK);
    } else {
        /* No prior file on this card at all - the new row is the whole
         * body, right after the header just written. */
        ok = ok && (f_write(&fout, row, (UINT)rowLen, &bw) == FR_OK);
    }

    f_sync(&fout);
    f_close(&fout);
    if (!ok) { f_unlink(vxt_cal_tmp); return 0; }

    /* Swap the temp file over the original - f_rename() does not overwrite
     * an existing destination, so the old file is unlinked first. Between
     * these two calls there is a brief window with no /calmeas.csv at all;
     * acceptable here because this only ever runs from a one-shot RECORD
     * button press, never the frame path, same exposure
     * the rig's own calSaveLog() already accepts on every one of its
     * RECORD presses. */
    f_unlink(vxt_cal_csv);
    if (f_rename(vxt_cal_tmp, vxt_cal_csv) != FR_OK) return 0;
    return 1;
}
