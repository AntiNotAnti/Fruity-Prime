#!/usr/bin/env bash
# Refuse source or release files that do not belong in Fruity Prime packages.
# This intentionally supports the Bash 3.2 that ships with macOS so the same
# guard can run on native Mac build runners without requiring Homebrew Bash.
set -uo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."

ALLOW_FILE="tools/asset-guard-allow.txt"
BANNED_EXT='nds|bin|arc|narc|sdat|sbin|spc|wav|mp3|ogg|brstm|png|jpg|jpeg|gif|bmp|tga|dds'
BANNED_LEVEL='(^|/)pak[0-9]+\.pk3$'
BIG_OK='(^|/)maps/.*\.(pk3|bsp)$'
BANNED_PATH='(^|/)(thumbnails|files|_archives|Savedata|netcheck-shots)/|(^|/)paths\.txt$|(^|/)netlog-[^/]*\.txt$'
MAX_BYTES=$((2 * 1024 * 1024))
CHECK_SIZE=1

allowed() {
  [ -f "$ALLOW_FILE" ] || return 1
  local path="$1" pattern
  while IFS= read -r pattern; do
    case "$pattern" in ''|'#'*) continue ;; esac
    # shellcheck disable=SC2254
    case "$path" in $pattern) return 0 ;; esac
  done < "$ALLOW_FILE"
  return 1
}

fail=0
report() {
  echo "REFUSED: $1 -- $2"
  fail=1
}

check_one() {
  local what="$1" path="$2" size
  [ -n "$path" ] || return 0
  allowed "$path" && return 0

  if echo "$path" | grep -qiE "\.($BANNED_EXT)$"; then
    report "$path" "a protected asset or image ($what)"
    return 0
  fi
  if echo "$path" | grep -qiE "$BANNED_LEVEL"; then
    report "$path" "a reserved upstream package ($what)"
    return 0
  fi
  if echo "$path" | grep -qE "$BANNED_PATH"; then
    report "$path" "is in a generated/extracted-data location ($what)"
    return 0
  fi
  if echo "$path" | grep -qiE "$BIG_OK"; then
    return 0
  fi
  if [ "$CHECK_SIZE" -eq 1 ] && [ -f "$path" ]; then
    size=$(wc -c < "$path")
    if [ "$size" -gt "$MAX_BYTES" ]; then
      report "$path" "$size bytes, too large for source ($what)"
    fi
  fi
}

if [ "$#" -eq 0 ]; then
  echo "== checking what git is tracking =="
  while IFS= read -r path; do
    check_one "tracked by git" "$path"
  done < <(git ls-files)
  if ! grep -q '^thumbnails/$' .gitignore; then
    report ".gitignore" "no longer ignores thumbnails/"
  fi
else
  CHECK_SIZE=0
  for dir in "$@"; do
    echo "== checking $dir =="
    if [ ! -d "$dir" ]; then
      report "$dir" "not a directory"
      continue
    fi
    while IFS= read -r path; do
      check_one "in $dir" "$path"
    done < <(find "$dir" -type f | sed 's|^\./||')
  done
fi

if [ "$fail" -ne 0 ]; then
  echo
  echo "Package guard failed. Add intentional project-owned exceptions to $ALLOW_FILE."
  exit 1
fi

echo "clean: package guard passed"
