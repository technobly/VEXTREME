#!/bin/bash
# pack_vxe.sh - pack already-built payloads into .vxe bundles, one per game
# registered in code/stm32/games/*.mk.
# Each bundle = the game's 6809 cartridge image + its payload_<name>_<layout>.bin files from a
# `make vxe-<name>` build (one per flash layout the game supports; whichever exist are packed).
# For a fresh build of everything, use instead:
#     cd code/stm32 && make vxe-all USE_HW=v0.3        (out/vxe-<layout>/)
#
# Usage (from the repo root): tools/pack_vxe.sh <payload dir> <out dir>      e.g. tools/pack_vxe.sh code/stm32 out/packed
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PAY="${1:?payload dir}"; OUT="${2:?out dir}"
mkdir -p "$OUT"
field() { sed -n -E "s/^$2[[:space:]]*:?=[[:space:]]*([^[:space:]#]*).*/\\1/p" "$1" | head -1; }   # BSD sed: -E, no \\?
for mk in "$ROOT"/code/stm32/games/*.mk; do
    name="$(basename "$mk" .mk)"
    title="$(field "$mk" GAME_TITLE)"; dir="$(field "$mk" GAME_6809_DIR)"; asm="$(field "$mk" GAME_6809_ASM)"
    [ -n "$title" ] && [ -n "$dir" ] && [ -n "$asm" ] || { echo "pack_vxe: $mk is missing GAME_TITLE/GAME_6809_DIR/GAME_6809_ASM" >&2; exit 1; }
    args=()
    for l in uf2 dfu; do [ -f "$PAY/payload_${name}_$l.bin" ] && args+=("--stm-$l" "$PAY/payload_${name}_$l.bin"); done
    [ ${#args[@]} -gt 0 ] || { echo "pack_vxe: skipping $name (no $PAY/payload_${name}_<layout>.bin)"; continue; }
    python3 -I "$ROOT/tools/make_vxe.py" pack "$OUT/$title.vxe" --m6809 "$ROOT/code/app6809/$dir/$asm.bin" \
        "${args[@]}" --name "$title"
    python3 -I "$ROOT/tools/make_vxe.py" info "$OUT/$title.vxe" | tail -2
done
