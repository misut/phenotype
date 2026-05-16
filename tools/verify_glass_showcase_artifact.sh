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
  exec "$CLI" artifact verify-glass-showcase "$@"
fi

EXAMPLE_DIR="$ROOT/examples/glass_showcase"
MANIFEST="${PHENOTYPE_ARTIFACT_MANIFEST:-examples/glass_showcase/artifact_manifest.json}"
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

run_uv_python() {
  local uv_env="${PHENOTYPE_UV_PROJECT_ENVIRONMENT:-${TMPDIR:-/tmp}/phenotype-uv-tools}"
  if [[ -n "${UV:-}" ]]; then
    UV_PROJECT_ENVIRONMENT="$uv_env" "$UV" run --frozen python "$@"
  elif command -v mise >/dev/null 2>&1; then
    UV_PROJECT_ENVIRONMENT="$uv_env" mise exec -- uv run --frozen python "$@"
  elif command -v uv >/dev/null 2>&1; then
    UV_PROJECT_ENVIRONMENT="$uv_env" uv run --frozen python "$@"
  else
    echo "error: uv is required; run through 'mise exec -- uv run ...'" >&2
    exit 1
  fi
}

cd "$EXAMPLE_DIR"
run_exon build

PHENOTYPE_ARTIFACT_DIR="$BUNDLE_DIR" \
PHENOTYPE_ARTIFACT_REASON="${PHENOTYPE_ARTIFACT_REASON:-glass-showcase-gate}" \
PHENOTYPE_ACCESSIBILITY_DISPLAY="${PHENOTYPE_ACCESSIBILITY_DISPLAY:-standard}" \
PHENOTYPE_ARTIFACT_EXIT=1 \
  .exon/debug/glass_showcase

cd "$ROOT"
run_uv_python tools/verify_artifact_bundle.py "$BUNDLE_DIR" \
  --expect-platform "${PHENOTYPE_EXPECT_PLATFORM:-macos}" \
  --manifest "$MANIFEST"
