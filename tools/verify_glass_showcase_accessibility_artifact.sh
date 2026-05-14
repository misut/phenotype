#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

PHENOTYPE_ACCESSIBILITY_DISPLAY="${PHENOTYPE_ACCESSIBILITY_DISPLAY:-reduce-transparency,increase-contrast,reduce-motion}" \
PHENOTYPE_ARTIFACT_REASON="${PHENOTYPE_ARTIFACT_REASON:-glass-showcase-accessibility-gate}" \
PHENOTYPE_ARTIFACT_MANIFEST="${PHENOTYPE_ARTIFACT_MANIFEST:-examples/glass_showcase/artifact_manifest.accessibility.json}" \
  "$ROOT/tools/verify_glass_showcase_artifact.sh"
