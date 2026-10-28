#!/bin/bash
# check_vxe_off.sh - proves the VXE switch: with VXE=0 this tree must build the regular VEXTREME firmware, the same file
# as the untouched upstream commit (the `upstream/main` ref).  Run it after every merge from upstream and before
# sending anything to the upstream project.  Needs docker (stm32-build image) and a few minutes.
#
# Usage (from the repo root): tools/check_vxe_off.sh [scratch dir]
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
W="${1:-$(mktemp -d)}"; rm -rf "$W"; mkdir -p "$W/stock" "$W/vxe0"
cd "$ROOT"
git archive upstream/main code | tar -x -C "$W/stock"
git ls-files code | tar -c -T - | tar -x -C "$W/vxe0"
bad=0
for cfg in "v1.0 1" "v0.3 0"; do
  HW=${cfg% *}; UF2=${cfg#* }
  for t in stock vxe0; do
    d="$W/$t/code/stm32"; X=""; [ $t = vxe0 ] && X="VXE=0"
    ( cd "$d" && make clean >/dev/null 2>&1; make all USE_HW=$HW USE_UF2=$UF2 $X >"$W/build_${t}_${HW}_${UF2}.log" 2>&1 ) \
      || { echo "BUILD FAILED: $t hw=$HW uf2=$UF2 (see $W/build_${t}_${HW}_${UF2}.log)"; exit 1; }
    cp "$d/VEXTREME.bin" "$W/${t}_${HW}_${UF2}.bin"
  done
  a=$(shasum -a 256 "$W/stock_${HW}_${UF2}.bin" | cut -c1-16); b=$(shasum -a 256 "$W/vxe0_${HW}_${UF2}.bin" | cut -c1-16)
  if [ "$a" = "$b" ]; then echo "hw=$HW uf2=$UF2: upstream $a == VXE=0 $b  ($(stat -f%z "$W/stock_${HW}_${UF2}.bin") bytes)  IDENTICAL"
  else echo "hw=$HW uf2=$UF2: upstream $a != VXE=0 $b  DIFFERENT"; bad=1; fi
done
[ $bad = 0 ] && echo "VXE=0 is the regular firmware" || echo "VXE=0 DIFFERS from upstream"
exit $bad
