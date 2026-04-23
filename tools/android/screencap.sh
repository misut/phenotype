#!/usr/bin/env sh
# Pull a PNG screenshot via `adb exec-out screencap -p`. Writes to
# $PHENOTYPE_ANDROID_STATE_DIR/phenotype-<epoch>.png and opens it
# automatically on macOS.
set -eu

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
. "$SCRIPT_DIR/_env.sh"

phenotype_android_require_tool ADB "adb"

ts="$(date +%s)"
out="$PHENOTYPE_ANDROID_STATE_DIR/phenotype-$ts.png"
"$ADB" exec-out screencap -p > "$out"

printf 'screenshot: %s\n' "$out"

if [ "$(_phenotype_android_host)" = "darwin" ] && command -v open >/dev/null 2>&1; then
    open "$out"
fi
