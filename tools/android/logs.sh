#!/usr/bin/env sh
# Live-tail the logcat stream filtered to phenotype and the crash
# categories we normally want when debugging the example app.
# Passes any extra args straight through to `adb logcat`.
set -eu

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
. "$SCRIPT_DIR/_env.sh"

phenotype_android_require_tool ADB "adb"

exec "$ADB" logcat -v brief \
    -s phenotype:V AndroidRuntime:E DEBUG:F ActivityManager:I "$@"
