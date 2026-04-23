#!/usr/bin/env sh
# Force-stop the phenotype example process so the next `am start` reloads
# native state from scratch.
set -eu

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
. "$SCRIPT_DIR/_env.sh"

phenotype_android_require_tool ADB "adb"

printf 'am force-stop: %s\n' "$PHENOTYPE_ANDROID_PACKAGE"
"$ADB" shell am force-stop "$PHENOTYPE_ANDROID_PACKAGE"
