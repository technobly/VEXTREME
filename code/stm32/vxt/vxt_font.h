/*
 * vxt_font.h - VXT toolkit: font-table data contract.
 *
 * vxt_smart_text.c is a DRIVER, not a font: it knows how to walk a stroke
 * list and turn it into SmartList move/draw records, but it carries no
 * glyph data of its own. A font is a plain data module - one array of
 * VxtFontGlyph entries, each a character plus up to VXT_FONT_MAX_STROKES
 * straight-line strokes on a fixed 3-wide, 5-tall integer grid
 * (x: 0-2, y: 0-4; curves are approximated with straight segments, same
 * compromise every glyph in every font built against this contract makes).
 *
 * To supply a font: define a static const VxtFontGlyph array (unknown
 * characters are absent - vxtSmartTextChar() draws
 * nothing for them) in its own .c/.h pair, link that module's .o
 * alongside vxt_smart_text.o, and call vxtSmartTextSetFontTable() once at
 * startup with the table and its entry count.
 *
 * A font MAY additionally supply ligatures - multi-character sequences
 * drawn as one glyph rather than several (for example the two-letter
 * digraphs of some alphabets). Register a ligature lookup
 * via vxtSmartTextRegisterOverlayFont() alongside the base table; a font
 * with no ligature concept passes NULL and is unaffected.
 */
#ifndef VXT_FONT_H
#define VXT_FONT_H

#include <stdint.h>

#define VXT_FONT_MAX_STROKES  7

typedef struct { int8_t x1, y1, x2, y2; } VxtFontStroke;

typedef struct {
    char ch;
    uint8_t nstrokes;
    VxtFontStroke strokes[VXT_FONT_MAX_STROKES];
} VxtFontGlyph;

/* Ligature (multi-character) lookup, for a font that needs one. Returns
 * the stroke count (0 if `c1,c2` is not a recognized ligature in this
 * font - caller falls back to a single-character lookup) and points
 * *outStrokes at the ligature glyph's strokes. */
typedef uint8_t (*VxtFontLigatureFn)(char c1, char c2, const VxtFontStroke **outStrokes);

#endif /* VXT_FONT_H */
