#!/bin/bash
# leak_check.sh - run before every push of the PUBLIC repo, and before it is ever made public.
# Fails if something that must stay private is tracked, or appears in text we added, or (with --history) was ever added in
# any commit that is not upstream's.
#
# What counts as private is NOT written in this file (so this file can be public): it is read from
# tools/leak_patterns.local, a shell file that is not tracked here, defining
#     LEAK_NAME_REGEX   extended regex matched against tracked file paths
#     LEAK_TEXT_REGEX   extended regex matched, case-insensitively, against text we added or changed
# Without that file only the generic check (build artefacts) runs, and the script says so.
#
# Usage (from the repo root): tools/leak_check.sh [--history]
cd "$(dirname "$0")/.." || exit 2
bad=0
PATS=tools/leak_patterns.local
if [ -f "$PATS" ]; then . "$PATS"; else echo "NOTE: $PATS not found - only the generic check runs"; fi
if [ -n "$LEAK_NAME_REGEX" ]; then
  hits=$(git ls-files | grep -E "$LEAK_NAME_REGEX" | grep -v '^hall-of-fame/' || true)
  [ -n "$hits" ] && { echo "PRIVATE-LOOKING FILES TRACKED:"; echo "$hits"; bad=1; }
fi
if [ -n "$LEAK_TEXT_REGEX" ]; then
  # only files WE added or changed relative to upstream (upstream's own docs may say what they like)
  ours=$(git diff --name-only --diff-filter=AMR upstream/main -- . ':!tools/leak_patterns.local' | grep -v '^hall-of-fame/' || true)
  hits=$(echo "$ours" | tr '\n' '\0' | xargs -0 grep -I -l -i -E "$LEAK_TEXT_REGEX" 2>/dev/null || true)
  [ -n "$hits" ] && { echo "FORBIDDEN TEXT IN:"; echo "$hits"; bad=1; }
fi
hits=$(git ls-files | grep -E '\.(o|d|elf|map|hex|srec|list|uf2|vxe)$|^out/' || true)
[ -n "$hits" ] && { echo "BUILD ARTEFACTS TRACKED:"; echo "$hits" | head; bad=1; }
if [ "$1" = "--history" ]; then
  # every line ADDED by a commit that is not upstream's, plus the commit messages and the file names
  for c in $(git rev-list upstream/main..HEAD); do
    if [ -n "$LEAK_TEXT_REGEX" ]; then
      h=$( { git show --format=%B -U0 "$c" | grep -E '^\+[^+]|^[^+d@ -]'; git show --format= -U0 "$c" | grep -E '^\+[^+]'; } | grep -i -E "$LEAK_TEXT_REGEX" | head -1 )
      [ -n "$h" ] && { echo "HISTORY: commit $(git log -1 --format=%h "$c") adds or says: $(echo "$h" | cut -c1-90)"; bad=1; }
    fi
    if [ -n "$LEAK_NAME_REGEX" ]; then
      h=$(git show --format= --name-only "$c" | grep -E "$LEAK_NAME_REGEX" | grep -v '^hall-of-fame/' | head -1)
      [ -n "$h" ] && { echo "HISTORY: commit $(git log -1 --format=%h "$c") touches a private-looking file: $h"; bad=1; }
    fi
  done
fi
[ $bad = 0 ] && echo "leak check: clean${1:+ ($1)}"
exit $bad
