#!/bin/bash
# test_vxe_install.sh - host test of the kernel's .vxe bundle loader (code/stm32/vxe/test_vxe_install.c)
# against real bundles: VXCOOP.vxe and CAL.vxe as shipped, plus BIG.vxe, which is VXCOOP's own
# payloads padded past 64 KB so that the two-sector (4+5) install path is exercised too, and
# bundles that carry only one layout's payload (the "game for the other kernel" case).
#
# Usage (from the repo root): tools/test_vxe_install.sh <fat32 image> [vxe dir] [uf2|dfu]
#   <fat32 image>  a raw FAT32 volume, e.g.
#                  hdiutil create -size 64m -fs "MS-DOS FAT32" -layout NONE -type UDIF card.dmg
#   [vxe dir]      where VXCOOP.vxe and CAL.vxe are (dual-layout bundles, format 2) (default out/vxe-<layout>, i.e. the output of
#                  `cd code/stm32 && make vxe-all USE_HW=v0.3 VXE_LAYOUT=<layout>`). Pass an ABSOLUTE path.
#   [uf2|dfu]      the flash layout those bundles were built for (default uf2): which sectors the
#                  simulated flash allows, and how many erases a big payload needs
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
IMG="$1"; LAYOUT="${3:-uf2}"; VXE="${2:-$ROOT/out/vxe-$LAYOUT}"
case "$LAYOUT" in uf2) UF2=1; SLOT=0x08020000;; dfu) UF2=0; SLOT=0x08010000;; *) echo "layout must be uf2 or dfu"; exit 2;; esac
[ -f "$IMG" ] || { echo "usage: $0 <fat32 image> [vxe dir]"; exit 2; }
for b in VXCOOP CAL; do [ -f "$VXE/$b.vxe" ] || { echo "missing $VXE/$b.vxe - run make vxe-all first"; exit 2; }; done
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

# The bundles from `make vxe-all` carry one payload per layout. BIG = VXCOOP's payloads, each padded with
# pseudo-random words to 110,000 bytes (header image_end updated; the packer re-stamps the identity CRC), so the
# two-sector install is exercised too. ONLYOTHER / ONLYMINE carry just one layout's payload.
python3 -I "$ROOT/tools/make_vxe.py" unpack "$VXE/VXCOOP.vxe" "$WORK/u" > /dev/null
python3 -I - "$WORK/u" "$WORK" <<'PY'
import glob, struct, sys, random
d, out = sys.argv[1], sys.argv[2]
SLOTS = {"uf2": 0x08020000, "dfu": 0x08010000}
random.seed(1)
for lay, slot in SLOTS.items():
    p = open(glob.glob(d + "/*STM_" + lay + ".bin")[0], "rb").read()
    pad = bytes(random.getrandbits(8) for _ in range(110000 - len(p)))
    q = bytearray(p + pad)
    struct.pack_into("<I", q, 8, slot + len(q))
    open(out + "/big_" + lay + ".bin", "wb").write(q)
PY
M6809="$(echo "$WORK"/u/*6809.bin)"
python3 -I "$ROOT/tools/make_vxe.py" pack "$WORK/BIG.vxe" --m6809 "$M6809" --stm-uf2 "$WORK/big_uf2.bin" --stm-dfu "$WORK/big_dfu.bin" --name BIG > /dev/null
if [ "$LAYOUT" = uf2 ]; then MINE=uf2; OTHER=dfu; else MINE=dfu; OTHER=uf2; fi
python3 -I "$ROOT/tools/make_vxe.py" pack "$WORK/ONLYOTHER.vxe" --m6809 "$M6809" --stm-$OTHER "$WORK/big_$OTHER.bin" --name ONLYOTHER > /dev/null
python3 -I "$ROOT/tools/make_vxe.py" pack "$WORK/ONLYMINE.vxe" --m6809 "$M6809" --stm-$MINE "$WORK/big_$MINE.bin" --name ONLYMINE > /dev/null

# libopencm3 lives only in the Docker image: stand-in headers for the host.
mkdir -p "$WORK/inc/libopencm3/cm3"
printf '#include <stdint.h>\n#include <stdbool.h>\n' > "$WORK/inc/libopencm3/cm3/common.h"
printf '#include <stdint.h>\nuint32_t dwt_read_cycle_counter(void);\n' > "$WORK/inc/libopencm3/cm3/dwt.h"
cp "$IMG" "$WORK/card.img"
cd "$ROOT/code/stm32"
cc -w -I. -I"$WORK/inc" -DVXE_ENABLED=1 -DVXE_LAYOUT_UF2=$UF2 -DVXE_SPLIT=1 -DVXE_HOST_TEST -o "$WORK/t" vxe/test_vxe_install.c fatfs/ff.c fatfs/option/unicode.c
"$WORK/t" "$WORK/card.img" "$WORK/BIG.vxe" "$VXE/VXCOOP.vxe" "$VXE/CAL.vxe" "$WORK/ONLYOTHER.vxe" "$WORK/ONLYMINE.vxe"
