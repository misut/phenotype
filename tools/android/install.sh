#!/usr/bin/env sh
# Install the phenotype Android example APK onto the attached device.
set -eu

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
. "$SCRIPT_DIR/_env.sh"

phenotype_android_require_tool ADB "adb"

if [ ! -f "$PHENOTYPE_ANDROID_APK" ]; then
    echo "error: apk not found at $PHENOTYPE_ANDROID_APK" >&2
    echo "       run \`mise run android:apk\` first" >&2
    exit 1
fi

if ! phenotype_android_emu_running && ! "$ADB" devices | awk 'NR>1 && $2=="device"' | grep -q .; then
    echo "error: no online adb device — run \`mise run android:emu:start\` or attach a device" >&2
    exit 1
fi

printf 'installing %s\n' "$PHENOTYPE_ANDROID_APK"
"$ADB" install -r "$PHENOTYPE_ANDROID_APK"
