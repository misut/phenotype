#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DESKTOP_DIR="$ROOT/examples/file_explorer_desktop"
MOBILE_DIR="$ROOT/examples/file_explorer_mobile"
DESKTOP_BUNDLE_ROOT="${PHENOTYPE_FILE_EXPLORER_DESKTOP_ARTIFACT_DIR:-}"
MOBILE_BUNDLE_ROOT="${PHENOTYPE_FILE_EXPLORER_MOBILE_ARTIFACT_DIR:-}"

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

reset_demo_profile() {
  local profile="$1"
  local base="${TMPDIR:-/tmp}"
  rm -rf "${base%/}/phenotype-file-explorer-$profile"
}

desktop_bundle_for_case() {
  local case_id="$1"
  if [[ -n "$DESKTOP_BUNDLE_ROOT" ]]; then
    if [[ "$case_id" == "icon" ]]; then
      mkdir -p "$DESKTOP_BUNDLE_ROOT"
      printf '%s\n' "$DESKTOP_BUNDLE_ROOT"
    else
      local case_bundle="${DESKTOP_BUNDLE_ROOT%/}-$case_id"
      mkdir -p "$case_bundle"
      printf '%s\n' "$case_bundle"
    fi
  else
    mktemp -d "${TMPDIR:-/tmp}/phenotype-file-explorer-desktop-$case_id.XXXXXX"
  fi
}

mobile_bundle_for_case() {
  local case_id="$1"
  if [[ -n "$MOBILE_BUNDLE_ROOT" ]]; then
    if [[ "$case_id" == "default" ]]; then
      mkdir -p "$MOBILE_BUNDLE_ROOT"
      printf '%s\n' "$MOBILE_BUNDLE_ROOT"
    else
      local case_bundle="${MOBILE_BUNDLE_ROOT%/}-$case_id"
      mkdir -p "$case_bundle"
      printf '%s\n' "$case_bundle"
    fi
  else
    mktemp -d "${TMPDIR:-/tmp}/phenotype-file-explorer-mobile-$case_id.XXXXXX"
  fi
}

append_scenario_requirements() {
  local scenario="$1"
  case "$scenario" in
    ""|"default")
      ;;
    "created-preview")
      extra_args+=(--require-label-contains "Action Note.txt")
      extra_args+=(--require-label-contains "Created Action Note.txt")
      extra_args+=(--require-label-contains "Created from artifact scenario")
      ;;
    "deleted-file")
      extra_args+=(--require-label-contains "Deleted Delete Me.txt")
      ;;
    "documents-preview")
      extra_args+=(--require-label-contains "Project Notes.txt")
      extra_args+=(--require-label-contains "Finder-like desktop layout")
      ;;
    *)
      echo "error: unknown file explorer startup scenario: $scenario" >&2
      exit 1
      ;;
  esac
}

verify_desktop_capture() {
  local mode="$1"
  local scenario="$2"
  local bundle="$3"
  local reason="file-explorer-desktop-$mode-gate"
  local -a extra_args=()

  if [[ -n "$scenario" && "$scenario" != "default" ]]; then
    reason="file-explorer-desktop-$mode-$scenario-gate"
  fi

  case "$mode" in
    icon|list|column|gallery)
      ;;
    *)
      echo "error: unknown desktop file explorer view mode: $mode" >&2
      exit 1
      ;;
  esac

  if [[ -z "$scenario" || "$scenario" == "default" ]]; then
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
    esac
  fi

  append_scenario_requirements "$scenario"
  local -a verifier_args=()
  if [[ -n "$scenario" && "$scenario" != "default" ]]; then
    verifier_args+=(
      --expect-platform macos
      --require-frame
      --require-role button
      --require-role material
      --require-role text_field
      --require-material-plan
      --require-material-semantic-runtime-match
    )
  else
    verifier_args+=(--manifest examples/file_explorer_desktop/artifact_manifest.json)
  fi

  local -a env_args=(
    "PHENOTYPE_FILE_EXPLORER_VIEW=$mode"
    "PHENOTYPE_ARTIFACT_DIR=$bundle"
    "PHENOTYPE_ARTIFACT_REASON=$reason"
    "PHENOTYPE_ACCESSIBILITY_DISPLAY=${PHENOTYPE_ACCESSIBILITY_DISPLAY:-standard}"
    "PHENOTYPE_ARTIFACT_EXIT=1"
  )
  if [[ -n "$scenario" && "$scenario" != "default" ]]; then
    env_args+=("PHENOTYPE_FILE_EXPLORER_SCENARIO=$scenario")
  fi

  reset_demo_profile "desktop"
  cd "$DESKTOP_DIR"
  env "${env_args[@]}" .exon/debug/file_explorer_desktop

  cd "$ROOT"
  local -a verify_args=(
    tools/verify_artifact_bundle.py
    "$bundle"
    "${verifier_args[@]}"
  )
  if ((${#extra_args[@]} > 0)); then
    verify_args+=("${extra_args[@]}")
  fi
  run_uv_python "${verify_args[@]}"
}

verify_mobile_capture() {
  local scenario="$1"
  local bundle="$2"
  local reason="file-explorer-mobile-gate"
  local -a extra_args=()

  if [[ -n "$scenario" && "$scenario" != "default" ]]; then
    reason="file-explorer-mobile-$scenario-gate"
  fi
  append_scenario_requirements "$scenario"
  local -a verifier_args=()
  if [[ -n "$scenario" && "$scenario" != "default" ]]; then
    verifier_args+=(
      --expect-platform macos
      --require-frame
      --require-role button
      --require-role material
      --require-role text_field
      --require-material-plan
      --require-material-semantic-runtime-match
    )
  else
    verifier_args+=(--manifest examples/file_explorer_mobile/artifact_manifest.json)
  fi

  local -a env_args=(
    "PHENOTYPE_ARTIFACT_DIR=$bundle"
    "PHENOTYPE_ARTIFACT_REASON=$reason"
    "PHENOTYPE_ACCESSIBILITY_DISPLAY=${PHENOTYPE_ACCESSIBILITY_DISPLAY:-standard}"
    "PHENOTYPE_ARTIFACT_EXIT=1"
  )
  if [[ -n "$scenario" && "$scenario" != "default" ]]; then
    env_args+=("PHENOTYPE_FILE_EXPLORER_SCENARIO=$scenario")
  fi

  reset_demo_profile "mobile"
  cd "$MOBILE_DIR"
  env "${env_args[@]}" .exon/debug/file_explorer_mobile

  cd "$ROOT"
  local -a verify_args=(
    tools/verify_artifact_bundle.py
    "$bundle"
    "${verifier_args[@]}"
  )
  if ((${#extra_args[@]} > 0)); then
    verify_args+=("${extra_args[@]}")
  fi
  run_uv_python "${verify_args[@]}"
}

cd "$DESKTOP_DIR"
run_exon build
for mode in icon list column gallery; do
  verify_desktop_capture "$mode" "default" "$(desktop_bundle_for_case "$mode")"
done
for scenario in created-preview deleted-file documents-preview; do
  verify_desktop_capture \
    "icon" \
    "$scenario" \
    "$(desktop_bundle_for_case "icon-$scenario")"
done

cd "$MOBILE_DIR"
run_exon build
verify_mobile_capture "default" "$(mobile_bundle_for_case "default")"
for scenario in created-preview deleted-file documents-preview; do
  verify_mobile_capture "$scenario" "$(mobile_bundle_for_case "$scenario")"
done
