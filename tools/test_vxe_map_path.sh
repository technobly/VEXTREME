#!/bin/bash
# test_vxe_map_path.sh - host test of which folder a game's files land in (vxe_kernel.c's path mapping).
# Usage (from the repo root): tools/test_vxe_map_path.sh
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
WORK="$(mktemp -d)"; trap 'rm -rf "$WORK"' EXIT
mkdir -p "$WORK/inc/libopencm3/cm3"
printf '#include <stdint.h>\n#include <stdbool.h>\n' > "$WORK/inc/libopencm3/cm3/common.h"
printf '#include <stdint.h>\nuint32_t dwt_read_cycle_counter(void);\n' > "$WORK/inc/libopencm3/cm3/dwt.h"
cd "$ROOT/code/stm32"
cc -w -I. -I"$WORK/inc" -DVXE_ENABLED=1 -DVXE_LAYOUT_UF2=${LAYOUT_UF2:-1} -DVXE_SPLIT=1 -o "$WORK/t" vxe/test_vxe_map_path.c
"$WORK/t"
