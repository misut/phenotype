#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CLI="$ROOT/tools/phenotype_cli/.exon/debug/phenotype_cli"
LOCK="$ROOT/tools/phenotype_cli/.exon/phenotype_cli_build.lock"

needs_build=0
if [[ ! -x "$CLI" ]]; then
  needs_build=1
elif [[ -n "$(find \
    "$ROOT/tools/phenotype_cli/exon.toml" \
    "$ROOT/tools/phenotype_cli/src" \
    "$ROOT/examples/file_explorer_shared/exon.toml" \
    "$ROOT/examples/file_explorer_shared/src" \
    -type f -newer "$CLI" -print -quit)" ]]; then
  needs_build=1
fi

if [[ "$needs_build" == "1" ]]; then
  mkdir -p "$(dirname "$LOCK")"
  lock_acquired=0
  while [[ "$needs_build" == "1" ]]; do
    if mkdir "$LOCK" 2>/dev/null; then
      lock_acquired=1
      break
    fi
    if [[ -x "$CLI" && -z "$(find \
        "$ROOT/tools/phenotype_cli/exon.toml" \
        "$ROOT/tools/phenotype_cli/src" \
        "$ROOT/examples/file_explorer_shared/exon.toml" \
        "$ROOT/examples/file_explorer_shared/src" \
        -type f -newer "$CLI" -print -quit)" ]]; then
      needs_build=0
      break
    fi
    sleep 0.2
  done
  if [[ "$lock_acquired" == "1" ]]; then
    trap 'rmdir "$LOCK" 2>/dev/null || true' EXIT
    (cd "$ROOT/tools/phenotype_cli" && mise exec -- exon build)
    rmdir "$LOCK"
    trap - EXIT
  fi
fi

exec "$CLI" artifact verify-file-explorer "$@"
