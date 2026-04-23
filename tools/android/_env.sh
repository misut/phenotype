# Shared helpers for tools/android/*.sh. Source, don't exec:
#   . "$(dirname "$0")/_env.sh"
#
# Resolves SDK / NDK / JDK locations, derives `adb` / `emulator`
# absolute paths, and exports a small set of PHENOTYPE_ANDROID_*
# variables that the individual task scripts consume.

set -eu

_phenotype_android_host() {
    case "$(uname -s)" in
        Darwin) printf 'darwin' ;;
        Linux)  printf 'linux'  ;;
        *) echo "unsupported host: $(uname -s)" >&2; return 1 ;;
    esac
}

_phenotype_android_default_sdk() {
    case "$(_phenotype_android_host)" in
        darwin) printf '%s/Library/Android/sdk' "$HOME" ;;
        linux)  printf '%s/Android/Sdk'         "$HOME" ;;
    esac
}

_phenotype_android_default_ndk() {
    # Prefer the SDK-managed revision (symlink-friendly), fall back to
    # the hand-extracted zip path the repo README documents.
    rev="30.0.14904198-beta1"
    if [ -n "${ANDROID_HOME:-}" ] && [ -d "$ANDROID_HOME/ndk/$rev" ]; then
        printf '%s/ndk/%s' "$ANDROID_HOME" "$rev"
        return 0
    fi
    if [ -d "/tmp/ndk-r30/android-ndk-r30-beta1" ]; then
        printf '/tmp/ndk-r30/android-ndk-r30-beta1'
        return 0
    fi
    # Emit nothing — callers treat empty as "missing".
}

_phenotype_android_default_jdk() {
    if [ "$(_phenotype_android_host)" = "darwin" ] \
        && command -v /usr/libexec/java_home >/dev/null 2>&1; then
        /usr/libexec/java_home -v 17 2>/dev/null || true
    fi
}

_phenotype_android_first_avd() {
    [ -x "${EMULATOR:-}" ] || return 0
    "$EMULATOR" -list-avds 2>/dev/null | head -n1
}

# Resolve ANDROID_HOME / ANDROID_NDK_HOME / JAVA_HOME with sensible
# defaults. Do NOT overwrite values the user already exported.
: "${ANDROID_HOME:=$(_phenotype_android_default_sdk)}"
export ANDROID_HOME

_ndk_default="$(_phenotype_android_default_ndk)"
if [ -z "${ANDROID_NDK_HOME:-}" ] && [ -n "$_ndk_default" ]; then
    ANDROID_NDK_HOME="$_ndk_default"
fi
[ -n "${ANDROID_NDK_HOME:-}" ] && export ANDROID_NDK_HOME

_jdk_default="$(_phenotype_android_default_jdk)"
if [ -z "${JAVA_HOME:-}" ] && [ -n "$_jdk_default" ]; then
    JAVA_HOME="$_jdk_default"
fi
[ -n "${JAVA_HOME:-}" ] && export JAVA_HOME

# Tool resolution. Kept as plain variables so scripts can check emptiness.
ADB=""
EMULATOR=""
if [ -n "${ANDROID_HOME:-}" ] && [ -d "$ANDROID_HOME" ]; then
    [ -x "$ANDROID_HOME/platform-tools/adb" ] && ADB="$ANDROID_HOME/platform-tools/adb"
    [ -x "$ANDROID_HOME/emulator/emulator"  ] && EMULATOR="$ANDROID_HOME/emulator/emulator"
fi
# Fall back to PATH lookups if the SDK layout is unusual.
[ -z "$ADB"      ] && ADB="$(command -v adb      2>/dev/null || true)"
[ -z "$EMULATOR" ] && EMULATOR="$(command -v emulator 2>/dev/null || true)"
export ADB EMULATOR

# Repo roots. _env.sh lives at tools/android/_env.sh; repo root is two
# levels up from this file.
PHENOTYPE_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
ANDROID_EXAMPLE_DIR="$PHENOTYPE_ROOT/examples/android"
export PHENOTYPE_ROOT ANDROID_EXAMPLE_DIR

# Caller-tunable knobs.
: "${PHENOTYPE_ANDROID_PACKAGE:=io.github.misut.phenotype.example}"
: "${PHENOTYPE_ANDROID_ACTIVITY:=io.github.misut.phenotype.example.MainActivity}"
: "${PHENOTYPE_ANDROID_APK:=$ANDROID_EXAMPLE_DIR/app/build/outputs/apk/debug/app-debug.apk}"
export PHENOTYPE_ANDROID_PACKAGE PHENOTYPE_ANDROID_ACTIVITY PHENOTYPE_ANDROID_APK

# AVD: default to the first one `emulator -list-avds` reports, unless
# the user already picked one.
if [ -z "${PHENOTYPE_ANDROID_AVD:-}" ] && [ -x "${EMULATOR:-}" ]; then
    PHENOTYPE_ANDROID_AVD="$(_phenotype_android_first_avd)"
fi
export PHENOTYPE_ANDROID_AVD

# Runtime state — emu_start.sh writes here, emu_stop.sh reads.
: "${PHENOTYPE_ANDROID_STATE_DIR:=/tmp/phenotype-android}"
mkdir -p "$PHENOTYPE_ANDROID_STATE_DIR"
export PHENOTYPE_ANDROID_STATE_DIR

phenotype_android_require() {
    # phenotype_android_require VAR "friendly name" [hint]
    # Fails if $VAR is empty or its value is not a directory.
    name=$1; label=$2; hint=${3:-}
    eval "val=\${$name:-}"
    if [ -z "$val" ] || [ ! -d "$val" ]; then
        {
            printf 'error: %s not found (env %s="%s")\n' "$label" "$name" "$val"
            [ -n "$hint" ] && printf '  hint: %s\n' "$hint"
        } >&2
        return 1
    fi
}

phenotype_android_require_tool() {
    # phenotype_android_require_tool VAR "friendly name"
    name=$1; label=$2
    eval "val=\${$name:-}"
    if [ -z "$val" ] || [ ! -x "$val" ]; then
        printf 'error: %s not found (looked for %s)\n' "$label" "$name" >&2
        return 1
    fi
}

phenotype_android_emu_running() {
    [ -x "$ADB" ] || return 1
    "$ADB" devices 2>/dev/null | awk 'NR>1 && $1 ~ /^emulator-/ && $2=="device"' | grep -q .
}
