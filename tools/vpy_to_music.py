#!/usr/bin/env python3
"""
vpy_to_music.py - offline converter: Vectrex Studio .vpy (JSON note-event
export, imported from MIDI) -> a C source/header pair for VEXTREME's
vxt_music player (code/stm32/vxt/vxt_music.h/.c).

Copyright (C) 2026 Caelotronics.

This conversion runs offline because the STM32 firmware has no MIDI/JSON
parser. .vpy's "notes" list is already decoded (MIDI note number,
start/duration in ticks, velocity, channel), with exactly 3 channels
(0/1/2) matching the AY's 3 tone channels; each channel should be close to
monophonic, since the AY has no polyphony within a channel. This script
re-quantizes that data onto VEXTREME's 50Hz frame grid and converts MIDI
note numbers to AY 12-bit periods. The firmware contains no MIDI or JSON
code anywhere.

AY PERIOD FORMULA: period = round(clock / (16 * freq)), clock = 1.5MHz -
hardware-confirmed via a 4-note tuner calibration, not an assumed
datasheet nominal.

OUTPUT: one C array of {frame, period, amp} events per channel (amp==0 is
a note-off/rest), monotonically increasing in `frame`, plus a VxtMusicSong
struct instance wiring them together with vxt_music.h. `frame` is stored
as a DELTA from the previous event in the same channel (uint16_t, plenty
of headroom for any realistic gap) - see vxt_music.h for why (keeps the
struct small and avoids the STM32 doing anything more than "advance a
counter, compare, decrement" per channel per frame).

USAGE:
    python3 tools/vpy_to_music.py docs/VX-COOP/music_example.vpy \\
        code/stm32/vxcoop/vxcoop_music_example.c \\
        code/stm32/vxcoop/vxcoop_music_example.h \\
        music_example vxc
"""
import json
import sys

FRAME_HZ = 50.0
AY_CLOCK = 1500000.0
MIN_EVENT_FRAMES = 1     # a note shorter than this after quantization is
                         # still audible as a 1-frame blip, never dropped
                         # to 0 (0 would let it be silently swallowed by
                         # the next event's frame-delta of 0)

# Vectrex Studio's exported "ticksPerBeat" appears to count a finer
# subdivision than a quarter note (an eighth note, most likely), so a
# quarter note is actually 2*ticksPerBeat ticks, not ticksPerBeat ticks
# as secs_per_tick = 60/(tempo*ticksPerBeat) assumes. This constant
# corrects for that by halving secs_per_tick. It is a measured
# correction, confirmed against two source tracks at different tempos;
# re-check it against a third before trusting it unconditionally.
TEMPO_CORRECTION = 2.0


def note_to_period(note):
    freq = 440.0 * (2.0 ** ((note - 69) / 12.0))
    period = round(AY_CLOCK / (16.0 * freq))
    if period < 1:
        period = 1
    if period > 0xFFF:
        period = 0xFFF
    return period


def convert_channel(notes, secs_per_tick):
    """notes: list of {start,duration,note,velocity} ticks for ONE channel,
    already monophonic-by-construction (checked, not assumed - see the
    overlap trim below for the rare exception). Returns a list of
    (frame, period, amp) events, absolute frame numbers, monotonic,
    de-duplicated at the 50Hz grid."""
    notes = sorted(notes, key=lambda n: n["start"])
    events = []
    prev_end_frame = -1

    for i, n in enumerate(notes):
        start_frame = round(n["start"] * secs_per_tick * FRAME_HZ)
        end_frame = round((n["start"] + n["duration"]) * secs_per_tick * FRAME_HZ)

        # Same-channel overlap (rare): the hardware has one oscillator per
        # channel, so a later note cuts the previous one off - clip
        # prev_end_frame to at most this note's start, which the note-off
        # suppression logic below then respects.
        if start_frame < prev_end_frame:
            prev_end_frame = start_frame

        if start_frame < 0:
            start_frame = 0
        if end_frame <= start_frame:
            end_frame = start_frame + MIN_EVENT_FRAMES

        # Collapse consecutive notes that quantized onto the same frame -
        # last one wins (matches "later note cuts off earlier" above).
        if events and events[-1][0] == start_frame:
            events.pop()

        period = note_to_period(n["note"])
        amp = max(0, min(15, n["velocity"]))
        events.append((start_frame, period, amp))

        # Only emit an explicit note-off if the NEXT note doesn't already
        # start there (saves an event; a note-on IS an implicit note-off
        # for whatever was playing before it).
        next_start = None
        if i + 1 < len(notes):
            next_start = round(notes[i + 1]["start"] * secs_per_tick * FRAME_HZ)
        if next_start is None or next_start > end_frame:
            events.append((end_frame, 0, 0))

        prev_end_frame = end_frame

    # Final same-frame collapse pass + monotonic delta check.
    out = []
    for ev in events:
        if out and ev[0] == out[-1][0]:
            out[-1] = ev
            continue
        out.append(ev)
    return out


def to_delta(events):
    """Absolute-frame events -> (frameDelta, period, amp), delta from the
    previous event in the SAME channel (first event's delta is from frame
    0 - i.e. its own absolute frame)."""
    out = []
    prev = 0
    for (frame, period, amp) in events:
        delta = frame - prev
        assert delta >= 0, "non-monotonic event stream - converter bug"
        out.append((delta, period, amp))
        prev = frame
    return out


def emit_channel_array(f, sym, events):
    f.write(f"static const VxtMusicEvent {sym}[] = {{\n")
    for (delta, period, amp) in events:
        f.write(f"    {{ {delta}, {period}, {amp} }},\n")
    f.write("};\n")
    f.write(f"#define {sym.upper()}_COUNT  "
            f"(sizeof({sym}) / sizeof({sym}[0]))\n\n")


def main():
    # Optional 5th positional argument, the symbol/guard prefix.
    # Defaults to "song" if omitted.
    if len(sys.argv) not in (5, 6):
        print(__doc__)
        sys.exit(1)

    vpy_path, c_path, h_path, song_name = sys.argv[1:5]
    prefix = sys.argv[5] if len(sys.argv) == 6 else "song"
    prefix_arg = f" {prefix}" if prefix != "song" else ""

    with open(vpy_path) as fp:
        d = json.load(fp)

    secs_per_tick = 60.0 / (d["tempo"] * d["ticksPerBeat"]) / TEMPO_CORRECTION
    notes = d["notes"]
    channels = sorted(set(n["channel"] for n in notes))
    if channels != [0, 1, 2]:
        print(f"WARNING: expected channels [0,1,2], found {channels} - "
              f"check the source file before trusting this output.",
              file=sys.stderr)

    total_frames = round(d["totalTicks"] * secs_per_tick * FRAME_HZ)

    upper = song_name.upper()
    guard = f"{prefix.upper()}_MUSIC_{upper}_H"
    song_sym = f"{prefix}Music_{song_name}"
    # Derive the #include from the real header filename rather than rebuilding
    # it from the prefix, so the emitted include is correct whatever the caller
    # named the file.
    h_name = h_path.replace("\\", "/").rsplit("/", 1)[-1]

    with open(h_path, "w") as f:
        f.write(f"/* GENERATED by tools/vpy_to_music.py from {vpy_path} - "
                f"DO NOT HAND-EDIT.\n")
        f.write(" * Copyright (C) 2026 Caelotronics.\n")
        f.write(f" * Regenerate: python3 tools/vpy_to_music.py {vpy_path} "
                f"{c_path} {h_path} {song_name}{prefix_arg}\n */\n")
        f.write(f"#ifndef {guard}\n#define {guard}\n\n")
        f.write('#include "../vxt/vxt_music.h"\n\n')
        f.write(f"extern const VxtMusicSong {song_sym};\n\n")
        f.write(f"#endif /* {guard} */\n")

    with open(c_path, "w") as f:
        f.write(f"/* GENERATED by tools/vpy_to_music.py from {vpy_path} - "
                f"DO NOT HAND-EDIT.\n")
        f.write(" * Copyright (C) 2026 Caelotronics.\n")
        f.write(f" * Source: \"{d.get('name', '')}\", tempo={d['tempo']}, "
                f"ticksPerBeat={d['ticksPerBeat']}, "
                f"totalTicks={d['totalTicks']}\n")
        f.write(f" * {len(notes)} source notes across channels {channels}, "
                f"{total_frames} frames (~{total_frames/FRAME_HZ:.1f}s @ 50Hz).\n")
        f.write(f" * Regenerate: python3 tools/vpy_to_music.py {vpy_path} "
                f"{c_path} {h_path} {song_name}{prefix_arg}\n */\n")
        f.write(f'#include "{h_name}"\n\n')

        chan_syms = []
        for c in (0, 1, 2):
            chan_notes = [n for n in notes if n["channel"] == c]
            events_abs = convert_channel(chan_notes, secs_per_tick)
            events_delta = to_delta(events_abs)
            sym = f"{song_sym}_ch{c}"
            chan_syms.append((sym, len(events_delta)))
            emit_channel_array(f, sym, events_delta)

        f.write(f"const VxtMusicSong {song_sym} = {{\n")
        f.write("    {\n")
        for (sym, count) in chan_syms:
            f.write(f"        {{ {sym}, {count} }},\n")
        f.write("    },\n")
        f.write(f"    {total_frames}, 1 /* loop */\n")
        f.write("};\n")

    total_events = sum(c for (_, c) in chan_syms)
    print(f"{song_name}: {total_events} events across 3 channels, "
          f"{total_frames} frames (~{total_frames/FRAME_HZ:.1f}s)")
    for (sym, count) in chan_syms:
        print(f"  {sym}: {count} events "
              f"(~{count * 4} bytes)")


if __name__ == "__main__":
    main()
