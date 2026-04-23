#!/usr/bin/env sh
# `am start` the phenotype example on the attached device.
set -eu

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
. "$SCRIPT_DIR/_env.sh"

phenotype_android_require_tool ADB "adb"

target="$PHENOTYPE_ANDROID_PACKAGE/$PHENOTYPE_ANDROID_ACTIVITY"
printf 'am start: %s\n' "$target"
"$ADB" shell am start -n "$target"
