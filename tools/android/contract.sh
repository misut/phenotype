#!/usr/bin/env sh
# Device/emulator contract runner for the Android example.
#
# The debug APK writes an artifact bundle from inside the running GameActivity
# when debug.phenotype.contract is enabled. This script builds, installs,
# launches, pulls the bundle with `run-as`, and validates it with the generic
# artifact verifier.
set -eu

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
. "$SCRIPT_DIR/_env.sh"

phenotype_android_require_tool ADB "adb"

step() { printf '\n── %s\n' "$1"; }

CONTRACT_REMOTE_REL="${PHENOTYPE_ANDROID_CONTRACT_REMOTE_REL:-files/phenotype-contract}"
CONTRACT_OUT="${PHENOTYPE_ANDROID_CONTRACT_OUT:-$PHENOTYPE_ANDROID_STATE_DIR/contract-bundle}"
CONTRACT_TIMEOUT="${PHENOTYPE_ANDROID_CONTRACT_TIMEOUT:-30}"

cleanup_props() {
    "$ADB" shell setprop debug.phenotype.contract 0 >/dev/null 2>&1 || true
    "$ADB" shell setprop debug.phenotype.reason "" >/dev/null 2>&1 || true
    "$ADB" shell setprop debug.phenotype.dir "" >/dev/null 2>&1 || true
}
trap cleanup_props EXIT INT TERM

if phenotype_android_device_online; then
    step "device"
    "$ADB" devices | awk 'NR>1 && NF>=2 {print "  "$0}'
else
    step "emu:start"
    "$SCRIPT_DIR/emu_start.sh"
fi

step "build"
(cd "$PHENOTYPE_ROOT" && mise exec -- exon build --target aarch64-linux-android)

step "apk"
"$SCRIPT_DIR/apk.sh"

step "install"
"$SCRIPT_DIR/install.sh"

step "prepare app artifact directory"
"$ADB" shell run-as "$PHENOTYPE_ANDROID_PACKAGE" rm -rf "$CONTRACT_REMOTE_REL" >/dev/null 2>&1 || true
"$ADB" logcat -c || true

step "force-stop"
"$SCRIPT_DIR/stop.sh" || true

step "enable contract capture"
"$ADB" shell setprop debug.phenotype.contract 1
"$ADB" shell setprop debug.phenotype.reason android-contract

step "launch"
"$SCRIPT_DIR/launch.sh"

step "wait for artifact"
deadline=$(( $(date +%s) + CONTRACT_TIMEOUT ))
while :; do
    if "$ADB" shell run-as "$PHENOTYPE_ANDROID_PACKAGE" \
            test -f "$CONTRACT_REMOTE_REL/snapshot.json" >/dev/null 2>&1 \
        && "$ADB" shell run-as "$PHENOTYPE_ANDROID_PACKAGE" \
            test -f "$CONTRACT_REMOTE_REL/frame.bmp" >/dev/null 2>&1; then
        break
    fi
    if [ "$(date +%s)" -ge "$deadline" ]; then
        "$ADB" logcat -d -t 160 -v brief \
            -s phenotype:V AndroidRuntime:E DEBUG:F ActivityManager:I || true
        echo "error: Android artifact did not appear within ${CONTRACT_TIMEOUT}s" >&2
        exit 1
    fi
    sleep 1
done

step "pull artifact"
rm -rf "$CONTRACT_OUT"
mkdir -p "$CONTRACT_OUT/platform"
"$ADB" exec-out run-as "$PHENOTYPE_ANDROID_PACKAGE" \
    cat "$CONTRACT_REMOTE_REL/snapshot.json" > "$CONTRACT_OUT/snapshot.json"
"$ADB" exec-out run-as "$PHENOTYPE_ANDROID_PACKAGE" \
    cat "$CONTRACT_REMOTE_REL/frame.bmp" > "$CONTRACT_OUT/frame.bmp"
"$ADB" exec-out run-as "$PHENOTYPE_ANDROID_PACKAGE" \
    cat "$CONTRACT_REMOTE_REL/platform/android-runtime.json" \
    > "$CONTRACT_OUT/platform/android-runtime.json"

step "verify artifact"
python3 "$PHENOTYPE_ROOT/tools/verify_artifact_bundle.py" "$CONTRACT_OUT" \
    --manifest "$PHENOTYPE_ROOT/examples/android/artifact_manifest.json"

printf '\nandroid contract artifact: %s\n' "$CONTRACT_OUT"
