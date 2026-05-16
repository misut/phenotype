#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

if [[ "${PHENOTYPE_CLI_SCRIPT_COMPAT:-}" != "1" ]]; then
  CLI="$ROOT/tools/phenotype_cli/.exon/debug/phenotype_cli"
  if [[ ! -x "$CLI" ]]; then
    LOCK="$ROOT/tools/phenotype_cli/.exon/phenotype_cli_build.lock"
    mkdir -p "$(dirname "$LOCK")"
    while ! mkdir "$LOCK" 2>/dev/null; do
      sleep 0.2
      [[ -x "$CLI" ]] && break
    done
    if [[ ! -x "$CLI" ]]; then
      trap 'rmdir "$LOCK" 2>/dev/null || true' EXIT
      (cd "$ROOT/tools/phenotype_cli" && mise exec -- exon build)
      rmdir "$LOCK"
      trap - EXIT
    fi
  fi
  exec "$CLI" artifact verify-glass-showcase --accessibility "$@"
fi

PHENOTYPE_ACCESSIBILITY_DISPLAY="${PHENOTYPE_ACCESSIBILITY_DISPLAY:-reduce-transparency,increase-contrast,reduce-motion}" \
PHENOTYPE_ARTIFACT_REASON="${PHENOTYPE_ARTIFACT_REASON:-glass-showcase-accessibility-gate}" \
PHENOTYPE_ARTIFACT_MANIFEST="${PHENOTYPE_ARTIFACT_MANIFEST:-examples/glass_showcase/artifact_manifest.accessibility.json}" \
  "$ROOT/tools/verify_glass_showcase_artifact.sh"
