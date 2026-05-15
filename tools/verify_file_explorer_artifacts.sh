#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DESKTOP_DIR="$ROOT/examples/file_explorer_desktop"
MOBILE_DIR="$ROOT/examples/file_explorer_mobile"
DESKTOP_BUNDLE_ROOT="${PHENOTYPE_FILE_EXPLORER_DESKTOP_ARTIFACT_DIR:-}"
MOBILE_BUNDLE="${PHENOTYPE_FILE_EXPLORER_MOBILE_ARTIFACT_DIR:-}"

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

desktop_bundle_for_mode() {
  local mode="$1"
  if [[ -n "$DESKTOP_BUNDLE_ROOT" ]]; then
    if [[ "$mode" == "icon" ]]; then
      mkdir -p "$DESKTOP_BUNDLE_ROOT"
      printf '%s\n' "$DESKTOP_BUNDLE_ROOT"
    else
      local mode_bundle="${DESKTOP_BUNDLE_ROOT%/}-$mode"
      mkdir -p "$mode_bundle"
      printf '%s\n' "$mode_bundle"
    fi
  else
    mktemp -d "${TMPDIR:-/tmp}/phenotype-file-explorer-desktop-$mode.XXXXXX"
  fi
}

verify_desktop_mode() {
  local mode="$1"
  local bundle="$2"
  local reason="file-explorer-desktop-$mode-gate"
  local extra_args=()

  case "$mode" in
    icon)
      extra_args+=(--require-label "README.txt")
      ;;
    list)
      extra_args+=(--require-label "Name")
      extra_args+=(--require-label "Kind")
      extra_args+=(--require-label "Size")
      ;;
    column)
      extra_args+=(--require-label "Preview")
      extra_args+=(--require-label "Demo Root")
      ;;
    gallery)
      extra_args+=(--require-label "TXT File")
      extra_args+=(--require-label-contains "Phenotype File Explorer")
      ;;
    *)
      echo "error: unknown desktop file explorer view mode: $mode" >&2
      exit 1
      ;;
  esac

  cd "$DESKTOP_DIR"
  PHENOTYPE_FILE_EXPLORER_VIEW="$mode" \
  PHENOTYPE_ARTIFACT_DIR="$bundle" \
  PHENOTYPE_ARTIFACT_REASON="$reason" \
  PHENOTYPE_ACCESSIBILITY_DISPLAY="${PHENOTYPE_ACCESSIBILITY_DISPLAY:-standard}" \
  PHENOTYPE_ARTIFACT_EXIT=1 \
    .exon/debug/file_explorer_desktop

  cd "$ROOT"
  run_uv_python tools/verify_artifact_bundle.py "$bundle" \
    --manifest examples/file_explorer_desktop/artifact_manifest.json \
    "${extra_args[@]}"
}

cd "$DESKTOP_DIR"
run_exon build
for mode in icon list column gallery; do
  verify_desktop_mode "$mode" "$(desktop_bundle_for_mode "$mode")"
done

cd "$MOBILE_DIR"
run_exon build
PHENOTYPE_ARTIFACT_DIR="$MOBILE_BUNDLE" \
PHENOTYPE_ARTIFACT_REASON=file-explorer-mobile-gate \
PHENOTYPE_ACCESSIBILITY_DISPLAY="${PHENOTYPE_ACCESSIBILITY_DISPLAY:-standard}" \
PHENOTYPE_ARTIFACT_EXIT=1 \
  .exon/debug/file_explorer_mobile

cd "$ROOT"
run_uv_python tools/verify_artifact_bundle.py "$MOBILE_BUNDLE" \
  --manifest examples/file_explorer_mobile/artifact_manifest.json
