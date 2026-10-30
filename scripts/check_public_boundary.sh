#!/usr/bin/env bash
# Fail if the public packaging tree looks like a monorepo leak.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DENY_PUBLIC="$ROOT/scripts/public_denylist.txt"
DENY_LOCAL="$ROOT/scripts/local/extra_denylist.txt"
STRICT=0
[[ "${1:-}" == "--strict" ]] && STRICT=1

fail=0
warn=0

while IFS= read -r -d '' entry; do
  name="$(basename "$entry")"
  case "$name" in
    .git|.gitignore|README.md|VERSION|LICENSE|LICENSE.txt|DISCLAIMER.md|scripts|src|glyphs|tests|Makefile|build.sh|setup-native.sh|bin|debug|dep|obj|.build|.DS_Store)
      continue ;;
  esac
  echo "error: unexpected top-level entry: $name" >&2
  fail=1
done < <(find "$ROOT" -mindepth 1 -maxdepth 1 \( -type d -o -type f \) -print0)

hit_paths() {
  local pattern="$1"
  local search_roots=()
  for d in src glyphs tests; do
    [[ -d "$ROOT/$d" ]] && search_roots+=("$ROOT/$d")
  done
  for f in Makefile build.sh setup-native.sh; do
    [[ -f "$ROOT/$f" ]] && search_roots+=("$ROOT/$f")
  done
  ((${#search_roots[@]} == 0)) && return 0
  find "${search_roots[@]}" \( -type f -o -type d \) -path "*${pattern}*" 2>/dev/null | head -20
}

scan_denylist_file() {
  local file="$1"
  [[ -f "$file" ]] || return 0
  local section="" line pat matches
  while IFS= read -r line || [[ -n "${line:-}" ]]; do
    [[ -z "$line" || "$line" =~ ^[[:space:]]*# ]] && continue
    if [[ "$line" =~ ^\[(paths|cargo|sources)\]$ ]]; then
      section="${BASH_REMATCH[1]}"
      continue
    fi
    pat="${line#"${line%%[![:space:]]*}"}"
    pat="${pat%"${pat##*[![:space:]]}"}"
    [[ -z "$pat" || -z "$section" ]] && continue
    case "$section" in
      paths)
        matches="$(hit_paths "$pat" || true)"
        if [[ -n "$matches" ]]; then
          echo "error: denylist path hit ($file): $pat" >&2
          echo "$matches" | sed 's/^/  /' >&2
          fail=1
        fi
        ;;
      sources)
        if [[ -d "$ROOT/src" ]]; then
          matches="$(grep -R -n -F -- "$pat" "$ROOT/src" "$ROOT/tests" 2>/dev/null | head -20 || true)"
          if [[ -n "$matches" ]]; then
            if (( STRICT )); then
              echo "error: denylist source hit ($file): $pat" >&2
              fail=1
            else
              echo "warning: denylist source hit ($file): $pat" >&2
              warn=1
            fi
            echo "$matches" | sed 's/^/  /' >&2
          fi
        fi
        ;;
    esac
  done < "$file"
}

scan_denylist_file "$DENY_PUBLIC"
scan_denylist_file "$DENY_LOCAL"

if (( fail != 0 )); then
  echo "public boundary check FAILED" >&2
  exit 1
fi
if (( warn != 0 )); then
  echo "public boundary check OK (with warnings; pass --strict to fail)"
else
  echo "public boundary check OK"
fi
