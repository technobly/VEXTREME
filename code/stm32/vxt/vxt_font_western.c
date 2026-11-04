/*
 * vxt_font_western.c - see vxt_font_western.h and vxt_font.h. Digits,
 * the full alphabet, and a practical punctuation set, each glyph on the
 * fixed 3x5 grid the driver expects. Not independently hardware-verified
 * on every glyph - tune the stroke coordinates below after seeing them
 * drawn, same as any font written against this contract.
 *
 * DUPLICATED from vxt_text.c's own glyph table (a separate, older text
 * path, vxt_draw-based rather than vxt_smart-based) - see vxt_smart_text.h's
 * note on why. Keep the two tables in sync by hand if the font ever changes;
 * vxt_text.c's own copy has already drifted (missing the T-Y addition below).
 */
#include "vxt_font_western.h"

const VxtFontGlyph vxtFontWestern[] = {
    { '0', 4, {{0,4,2,4},{2,4,2,0},{2,0,0,0},{0,0,0,4}} },
    { '1', 1, {{2,4,2,0}} },
    { '2', 5, {{0,4,2,4},{2,4,2,2},{2,2,0,2},{0,2,0,0},{0,0,2,0}} },
    { '3', 5, {{0,4,2,4},{2,4,2,2},{2,2,0,2},{2,2,2,0},{0,0,2,0}} },
    { '4', 4, {{0,4,0,2},{0,2,2,2},{2,4,2,2},{2,2,2,0}} },
    { '5', 5, {{0,4,2,4},{0,4,0,2},{0,2,2,2},{2,2,2,0},{0,0,2,0}} },
    { '6', 6, {{0,4,2,4},{0,4,0,2},{0,2,2,2},{0,2,0,0},{2,2,2,0},{0,0,2,0}} },
    { '7', 3, {{0,4,2,4},{2,4,2,2},{2,2,2,0}} },
    { '8', 7, {{0,4,2,4},{2,4,2,2},{2,2,2,0},{0,0,2,0},{0,0,0,2},{0,2,0,4},{0,2,2,2}} },
    { '9', 6, {{0,4,2,4},{2,4,2,2},{2,2,2,0},{0,0,2,0},{0,4,0,2},{0,2,2,2}} },
    { 'N', 3, {{0,4,0,0},{0,4,2,0},{2,0,2,4}} },
    { 'U', 3, {{0,4,0,0},{0,0,2,0},{2,0,2,4}} },
    { 'M', 4, {{0,0,0,4},{0,4,1,2},{1,2,2,4},{2,4,2,0}} },
    { 'B', 6, {{0,0,0,4},{0,4,2,4},{2,4,2,2},{2,2,0,2},{2,2,2,0},{2,0,0,0}} },
    { 'E', 4, {{0,4,2,4},{0,4,0,0},{0,2,2,2},{0,0,2,0}} },
    { 'R', 5, {{0,0,0,4},{0,4,2,4},{2,4,2,2},{2,2,0,2},{0,2,2,0}} },
    { 'O', 4, {{0,4,2,4},{2,4,2,0},{2,0,0,0},{0,0,0,4}} },
    { 'F', 3, {{0,4,2,4},{0,4,0,0},{0,2,2,2}} },
    { 'L', 2, {{0,4,0,0},{0,0,2,0}} },
    { 'I', 1, {{1,4,1,0}} },
    { 'S', 5, {{2,4,0,4},{0,4,0,2},{0,2,2,2},{2,2,2,0},{2,0,0,0}} },
    { '=', 2, {{0,3,2,3},{0,1,2,1}} },
    { '-', 1, {{0,2,2,2}} },
    { ' ', 0, {{0,0,0,0}} },
    { 'T', 2, {{0,4,2,4},{1,4,1,0}} },
    { 'G', 5, {{2,4,0,4},{0,4,0,0},{0,0,2,0},{2,0,2,2},{2,2,1,2}} },
    { 'H', 3, {{0,4,0,0},{2,4,2,0},{0,2,2,2}} },
    { 'A', 3, {{0,0,1,4},{1,4,2,0},{0,2,2,2}} },
    { 'D', 6, {{0,0,0,4},{0,4,1,4},{1,4,2,3},{2,3,2,1},{2,1,1,0},{1,0,0,0}} },
    { 'V', 2, {{0,4,1,0},{1,0,2,4}} },
    { 'C', 3, {{2,4,0,4},{0,4,0,0},{0,0,2,0}} },
    { 'X', 2, {{0,4,2,0},{0,0,2,4}} },
    { 'P', 4, {{0,0,0,4},{0,4,2,4},{2,4,2,2},{2,2,0,2}} },
    { 'Y', 3, {{0,4,1,2},{2,4,1,2},{1,2,1,0}} },
    /* Completes the alphabet (J,K,Q,W,Z) and adds a practical punctuation
     * set (.,?!:') beyond the existing =/-/space. W is LITERALLY 'M' with
     * every point's y mirrored (y -> 4-y) - not a separately-designed
     * glyph. */
    { 'J', 2, {{2,4,2,0},{2,0,0,0}} },
    { 'K', 3, {{0,0,0,4},{2,4,0,2},{0,2,2,0}} },
    { 'Q', 5, {{0,4,2,4},{2,4,2,0},{2,0,0,0},{0,0,0,4},{1,1,2,0}} },
    { 'W', 4, {{0,4,0,0},{0,0,1,2},{1,2,2,0},{2,0,2,4}} },
    { 'Z', 3, {{0,4,2,4},{2,4,0,0},{0,0,2,0}} },
    { '.', 1, {{1,0,1,1}} },
    { ',', 1, {{1,1,0,0}} },
    { '?', 4, {{0,4,2,4},{2,4,2,3},{2,3,1,2},{1,2,1,1}} },
    { '!', 2, {{1,4,1,2},{1,0,1,1}} },
    { ':', 2, {{1,3,1,4},{1,0,1,1}} },
    { '\'', 1, {{1,3,1,4}} },
    /* A diagonal stroke with a short tick at each end, reading as the
     * classic "two circles + bar" percent shape at this font's crude
     * resolution. */
    { '%', 3, {{0,4,0,3},{0,0,2,4},{2,1,2,0}} },
    /* A genuine circle-C, not literal "(C)" parens, for a copyright-line
     * readout. This font's plain 'C' is 3 straight strokes forming an
     * open box (top, left, bottom - open on the right), the same
     * "straight lines approximate curves" compromise every round shape
     * here already makes ('O' is a rectangle, 'D' cuts one corner). This
     * glyph is that same open-box idea, but traced with DIAGONALS instead
     * of right angles - top-left/left/bottom-left as one continuous
     * rounded arc, then two short diagonal stubs closing MOST of the
     * right side and leaving the same right-side gap real C's have - so
     * it reads as "a rounded C" (circle-with-opening) rather than the
     * square C already in this table, at the same resolution. Mapped to
     * '@' (unused in this font, a spare ASCII byte repurposed for one
     * special glyph) - deliberately not added to an overlay font: a
     * copyright mark is a universal symbol, not a letter that gets
     * translated, and the driver's western fallback already covers it
     * when an overlay font is active, the same as digits. */
    { '@', 5, {{1,4,0,3},{0,3,0,1},{0,1,1,0},{1,4,2,3},{1,0,2,1}} },
};

const unsigned vxtFontWesternCount = sizeof(vxtFontWestern) / sizeof(vxtFontWestern[0]);
