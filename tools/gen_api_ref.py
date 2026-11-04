#!/usr/bin/env python3
"""gen_api_ref.py - generate docs/vxe/API_REFERENCE.md from the toolkit headers.

Reads each public header under code/stm32/ and writes, per header, its own
introductory comment and then every function prototype and #define constant with
the comment block that precedes it. The reference is therefore always the headers'
own text - there is nothing to keep in step by hand.

    python3 tools/gen_api_ref.py [output.md]        (default: docs/vxe/API_REFERENCE.md)
"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
STM = os.path.join(ROOT, "code", "stm32")

# (path under code/stm32, section title, one line on what it is for)
HEADERS = [
    ("vxe/vxe_api.h", "The kernel boundary (vxe_api.h)", "What a payload may call in the firmware, the payload header, and the .vxe bundle layout."),
    ("vxe/vxe_game.h", "Your game's entry point (vxe_game.h)", "The one function every game provides."),
    ("vxt/vxt_rpc.h", "RPC dispatch (vxt_rpc.h)", "Register the handlers the 6809 calls."),
    ("vxt/vxt_smart.h", "SmartList drawing (vxt_smart.h)", "The draw engine: moves, lines and long lines as 4-byte records the 6809 plays back."),
    ("vxt/vxt_smart_text.h", "Text (vxt_smart_text.h)", "Strings and numbers drawn as vectors, two built-in fonts."),
    ("vxt/vxt_input.h", "Controller input (vxt_input.h)", "Joystick and button bytes the 6809 reports every frame."),
    ("vxt/vxt_sound.h", "Sound (vxt_sound.h)", "AY-3-8910 register writes queued for the 6809 to play."),
    ("vxt/vxt_music.h", "Music (vxt_music.h)", "A three-channel song player."),
    ("vxt/vxt_frame.h", "Served-image access (vxt_frame.h)", "Writes into the image the 6809 sees."),
    ("vxt/vxt_bounds.h", "Screen bounds (vxt_bounds.h)", "Playfield limits and bounce/wrap helpers."),
    ("vxt/vxt_font.h", "Font data format (vxt_font.h)", "How a vector font is described."),
    ("vxt/vxt_cal_load.h", "Per-console calibration (vxt_cal_load.h)", "Reading and saving this console's measured beam corrections."),
    ("gamelib/gamelib_beam.h", "Beam helpers (gamelib_beam.h)", "Repositioning, independent and chained long lines, draw-gain compensation."),
    ("gamelib/gamelib_proj3d.h", "3D projection (gamelib_proj3d.h)", "Perspective projection of model vertices."),
    ("gamelib/gamelib_clip.h", "Window culling and clipping (gamelib_clip.h)", "Classify objects against a disc or convex window with an optional HUD hole, clip segments, clip edges at the near plane."),
    ("gamelib/gamelib_cull.h", "Rotation and backface culling (gamelib_cull.h)", "Exact perspective backface culling for convex meshes."),
    ("delay.h", "Time (delay.h)", "Milliseconds since boot (through the kernel table)."),
    ("flash.h", "Flash write-back (flash.h)", "Only flashDoWriteback() is available to a payload."),
    ("led.h", "Cartridge LEDs (led.h)", "Needs <stdbool.h> and <stdint.h> included first."),
    ("xprintf.h", "Debug output (xprintf.h)", "xprintf to the serial console."),
    ("highscore.h", "The multicart's high-score store (highscore.h)", "Per-game best scores kept by the menu."),
]

COMMENT = re.compile(r"/\*.*?\*/", re.S)


def clean(c):
    """A /* ... */ block as plain text."""
    c = c.strip()[2:-2]
    out = []
    for line in c.split("\n"):
        line = line.rstrip()
        line = re.sub(r"^\s*\*\s?", "", line) if re.match(r"^\s*\*", line) else line.strip()
        out.append(line)
    # drop leading/trailing blank lines and a lone line of dashes/equals
    while out and not out[0].strip(): out.pop(0)
    while out and not out[-1].strip(): out.pop()
    return "\n".join(out)


def first_paragraph(text, maxlines=14):
    lines = text.split("\n")
    para = []
    for ln in lines:
        if not ln.strip() and para:
            break
        para.append(ln)
    if len(para) > maxlines:
        para = para[:maxlines] + ["..."]
    return "\n".join(para)


def parse(path):
    """-> (intro text, [(declaration, comment text, inline note)])"""
    src = open(path, encoding="utf-8", errors="replace").read()
    comments = [(m.start(), m.end(), clean(m.group(0))) for m in COMMENT.finditer(src)]
    # the same text with every comment blanked (offsets and newlines preserved)
    blank = COMMENT.sub(lambda m: re.sub(r"[^\n]", " ", m.group(0)), src)
    intro = comments[0][2] if comments and src[:comments[0][0]].strip() == "" else ""
    decl = re.compile(
        r"^(?P<d>(?:[ \t]*#define[ \t]+[A-Za-z_]\w*[^\n]*)"                       # a #define
        r"|(?:[A-Za-z_][\w \t\*]*?\b[A-Za-z_]\w*[ \t]*\([^;{}]*\)[ \t]*;))",  # or a prototype
        re.M)
    items, used = [], set()
    for mm in decl.finditer(blank):
        d = " ".join(mm.group("d").split())
        if d.startswith("typedef") or "(*" in d or d.startswith("extern"):
            continue
        if d.startswith("#define"):
            name = d.split()[1].split("(")[0]
            if name.endswith("_H") or name.startswith("_"):
                continue
        start, end = mm.start(), mm.end()
        eol = blank.find("\n", end)
        eol = len(blank) if eol < 0 else eol
        # a comment that starts on the declaration's own line, after it: the declaration's note
        inline = ""
        for i, (cs, ce, ct) in enumerate(comments):
            if end <= cs < eol and i not in used:
                inline = ct; used.add(i); break
        # a comment directly above: only blanks/newlines between its end and the declaration
        above = ""
        for i in range(len(comments) - 1, -1, -1):
            cs, ce, ct = comments[i]
            if ce <= start and blank[ce:start].strip() == "":
                # not the trailing note of the previous line's declaration
                if i not in used and src.rfind("\n", 0, cs) >= 0 and blank[src.rfind("\n", 0, cs):cs].strip() == "":
                    above = ct; used.add(i)
                break
        items.append((d, above, inline))
    return intro, items


def main():
    out_path = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "docs", "vxe", "API_REFERENCE.md")
    lines = [
        "# VXE / VXT API reference",
        "",
        "GENERATED by `tools/gen_api_ref.py` from the headers' own comments - do not edit by hand;",
        "re-run the script after changing a header. Where a comment is long, only its first paragraph",
        "is shown here: the header has the rest. Everything below is available to a payload",
        "(game code) except where a section says otherwise. A payload reaches the firmware ONLY through",
        "the table in `vxe/vxe_api.h`; see `SDK_GUIDE.md`.",
        "",
        "## Contents",
        "",
    ]
    body = []
    for rel, title, blurb in HEADERS:
        path = os.path.join(STM, rel)
        if not os.path.exists(path):
            continue
        anchor = re.sub(r"[^a-z0-9 -]", "", title.lower()).replace(" ", "-")
        lines.append("* [%s](#%s)" % (title, anchor))
        intro, items = parse(path)
        body += ["", "## " + title, "", "*" + blurb + "*  Header: `" + rel + "`.", ""]
        if intro:
            body += ["```text", first_paragraph(intro, 30), "```", ""]
        funcs = [(d, c, n) for d, c, n in items if not d.startswith("#define")]
        defs = [(d, c or n) for d, c, n in items if d.startswith("#define")]
        if funcs:
            body += ["### Functions", ""]
            for d, c, note in funcs:
                body += ["```c", re.sub(r"\s+", " ", d), "```"]
                text = c if c else note
                if c and note:
                    text = first_paragraph(c) + "\n\n(" + note.replace("\n", " ") + ")"
                body += ["", first_paragraph(text) if text else "", ""] if text else [""]
        if defs:
            body += ["### Constants and macros", "", "| Definition | Notes |", "|---|---|"]
            for d, c in defs:
                note = first_paragraph(c, 3).replace("\n", " ").replace("|", "\\|") if c else ""
                dd = re.sub(r"\s+", " ", d).replace("|", "\\|")
                if len(dd) > 110: dd = dd[:107] + "..."
                body.append("| `%s` | %s |" % (dd, note))
            body.append("")
    open(out_path, "w", encoding="utf-8").write("\n".join(lines + body) + "\n")
    print("wrote %s (%d lines)" % (out_path, len(lines) + len(body)))


if __name__ == "__main__":
    main()
