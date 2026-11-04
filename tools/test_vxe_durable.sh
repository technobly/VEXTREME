#!/bin/bash
# test_vxe_durable.sh - host test of the save-durability fix (the kernel's file table), against a model
# of the cart's SPI-flash write-back cache. See code/stm32/vxe/test_vxe_durable.c.
#
# Usage (from the repo root): tools/test_vxe_durable.sh <fat32 image of >= 64 MB>
#   image: hdiutil create -size 64m -fs "MS-DOS FAT32" -layout NONE -type UDIF card.dmg
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
IMG="$1"; [ -f "$IMG" ] || { echo "usage: $0 <fat32 image>"; exit 2; }
WORK="$(mktemp -d)"; trap 'rm -rf "$WORK"' EXIT
mkdir -p "$WORK/inc/libopencm3/cm3"
printf '#include <stdint.h>\n#include <stdbool.h>\n' > "$WORK/inc/libopencm3/cm3/common.h"
printf '#include <stdint.h>\nuint32_t dwt_read_cycle_counter(void);\n' > "$WORK/inc/libopencm3/cm3/dwt.h"
cd "$ROOT/code/stm32"
cc -w -I. -I"$WORK/inc" -DVXE_ENABLED=1 -DVXE_LAYOUT_UF2=${LAYOUT_UF2:-1} -o "$WORK/td" vxe/test_vxe_durable.c fatfs/ff.c fatfs/option/unicode.c
VXE_TEST_IMG="$IMG" "$WORK/td"
