#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CLI="$ROOT/tools/phenotype_cli/.exon/debug/phenotype_cli"
LOCK="$ROOT/tools/phenotype_cli/.exon/phenotype_cli_build.lock"

if [[ ! -x "$CLI" ]]; then
  mkdir -p "$(dirname "$LOCK")"
  lock_acquired=0
  while [[ ! -x "$CLI" ]]; do
    if mkdir "$LOCK" 2>/dev/null; then
      lock_acquired=1
      break
    fi
    sleep 0.2
  done
  if [[ "$lock_acquired" == "1" ]]; then
    trap 'rmdir "$LOCK" 2>/dev/null || true' EXIT
    if [[ ! -x "$CLI" ]]; then
      (cd "$ROOT/tools/phenotype_cli" && mise exec -- exon build)
    fi
    rmdir "$LOCK"
    trap - EXIT
  fi
fi

exec "$CLI" artifact verify-file-explorer "$@"
