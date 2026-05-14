#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
EXAMPLE_DIR="$ROOT/examples/glass_showcase"
BUNDLE_DIR="${PHENOTYPE_ARTIFACT_DIR:-}"
if [[ -z "$BUNDLE_DIR" ]]; then
  BUNDLE_DIR="$(mktemp -d "${TMPDIR:-/tmp}/phenotype-glass-showcase.XXXXXX")"
fi

run_exon() {
  if [[ -n "${EXON:-}" ]]; then
    "$EXON" "$@"
  elif command -v mise >/dev/null 2>&1; then
    mise exec -- exon "$@"
  else
    exon "$@"
  fi
}

cd "$EXAMPLE_DIR"
run_exon build

PHENOTYPE_ARTIFACT_DIR="$BUNDLE_DIR" \
PHENOTYPE_ARTIFACT_REASON="${PHENOTYPE_ARTIFACT_REASON:-glass-showcase-gate}" \
PHENOTYPE_ARTIFACT_EXIT=1 \
  .exon/debug/glass_showcase

cd "$ROOT"
tools/verify_artifact_bundle.py "$BUNDLE_DIR" \
  --expect-platform "${PHENOTYPE_EXPECT_PLATFORM:-macos}" \
  --manifest examples/glass_showcase/artifact_manifest.json
