#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DESKTOP_DIR="$ROOT/examples/file_explorer_desktop"
MOBILE_DIR="$ROOT/examples/file_explorer_mobile"
DESKTOP_BUNDLE="${PHENOTYPE_FILE_EXPLORER_DESKTOP_ARTIFACT_DIR:-}"
MOBILE_BUNDLE="${PHENOTYPE_FILE_EXPLORER_MOBILE_ARTIFACT_DIR:-}"

if [[ -z "$DESKTOP_BUNDLE" ]]; then
  DESKTOP_BUNDLE="$(mktemp -d "${TMPDIR:-/tmp}/phenotype-file-explorer-desktop.XXXXXX")"
fi
if [[ -z "$MOBILE_BUNDLE" ]]; then
  MOBILE_BUNDLE="$(mktemp -d "${TMPDIR:-/tmp}/phenotype-file-explorer-mobile.XXXXXX")"
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

run_uv_python() {
  if [[ -n "${UV:-}" ]]; then
    "$UV" run --frozen python "$@"
  elif command -v mise >/dev/null 2>&1; then
    mise exec -- uv run --frozen python "$@"
  elif command -v uv >/dev/null 2>&1; then
    uv run --frozen python "$@"
  else
    echo "error: uv is required; run through 'mise exec -- uv run ...'" >&2
    exit 1
  fi
}

cd "$DESKTOP_DIR"
run_exon build
PHENOTYPE_ARTIFACT_DIR="$DESKTOP_BUNDLE" \
PHENOTYPE_ARTIFACT_REASON=file-explorer-desktop-gate \
PHENOTYPE_ACCESSIBILITY_DISPLAY="${PHENOTYPE_ACCESSIBILITY_DISPLAY:-standard}" \
PHENOTYPE_ARTIFACT_EXIT=1 \
  .exon/debug/file_explorer_desktop

cd "$MOBILE_DIR"
run_exon build
PHENOTYPE_ARTIFACT_DIR="$MOBILE_BUNDLE" \
PHENOTYPE_ARTIFACT_REASON=file-explorer-mobile-gate \
PHENOTYPE_ACCESSIBILITY_DISPLAY="${PHENOTYPE_ACCESSIBILITY_DISPLAY:-standard}" \
PHENOTYPE_ARTIFACT_EXIT=1 \
  .exon/debug/file_explorer_mobile

cd "$ROOT"
run_uv_python tools/verify_artifact_bundle.py "$DESKTOP_BUNDLE" \
  --manifest examples/file_explorer_desktop/artifact_manifest.json
run_uv_python tools/verify_artifact_bundle.py "$MOBILE_BUNDLE" \
  --manifest examples/file_explorer_mobile/artifact_manifest.json
