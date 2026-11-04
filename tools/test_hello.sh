#!/bin/bash
# test_hello.sh - host test of the SDK template game (code/stm32/vxe/test_hello.c).
# Usage (from the repo root): tools/test_hello.sh <fat32 image of >= 64 MB>
#   image: hdiutil create -size 64m -fs "MS-DOS FAT32" -layout NONE -type UDIF card.dmg
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
IMG="$1"; [ -f "$IMG" ] || { echo "usage: $0 <fat32 image>"; exit 2; }
WORK="$(mktemp -d)"; trap 'rm -rf "$WORK"' EXIT
mkdir -p "$WORK/inc/libopencm3/cm3"
printf '#include <stdint.h>\n#include <stdbool.h>\n' > "$WORK/inc/libopencm3/cm3/common.h"
printf '#include <stdint.h>\nuint32_t dwt_read_cycle_counter(void);\n' > "$WORK/inc/libopencm3/cm3/dwt.h"
cp "$IMG" "$WORK/card.img"
cd "$ROOT/code/stm32"
PAY="-w -I. -I$WORK/inc -DVXE_ENABLED=1 -DVXE_LAYOUT_UF2=${LAYOUT_UF2:-1} -include vxe/vxe_redirect.h"
for f in game/hello.c vxt/vxt_smart.c vxt/vxt_frame.c vxt/vxt_sound.c vxt/vxt_cal_load.c gamelib/gamelib_beam.c vxe/vxe_shim.c; do
    if [ "$f" = vxe/vxe_shim.c ]; then cc -w -I. -I"$WORK/inc" -DVXE_ENABLED=1 -DVXE_LAYOUT_UF2=${LAYOUT_UF2:-1} -c "$f" -o "$WORK/$(basename "$f").o"
    else cc $PAY -c "$f" -o "$WORK/$(basename "$f").o"; fi
done
cc -w -I. -I"$WORK/inc" -DVXE_ENABLED=1 -DVXE_LAYOUT_UF2=${LAYOUT_UF2:-1} -o "$WORK/t" vxe/test_hello.c "$WORK"/*.o fatfs/ff.c fatfs/option/unicode.c -lm
"$WORK/t" "$WORK/card.img"
