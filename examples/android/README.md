# phenotype · android example

Minimal Android application that boots phenotype's Vulkan backend inside a
[GameActivity](https://developer.android.com/games/agdk/game-activity). As
of Stage 7 it runs a real `view(State) / update(State, Msg)` loop through
phenotype's shell and exposes the full `debug_api` (capabilities,
`snapshot_json`, `capture_frame_rgba`, `write_artifact_bundle`). The
built-in counter demo exercises the full three-pipeline stack (color +
text + image) plus touch + hardware-key + vertical-scroll input: tap the
`−` / `+` buttons to mutate `State.count`, Tab to cycle focus, Enter /
Space to trigger the focused button, mouse-wheel / trackpad / `adb shell
input roll` to dispatch scroll deltas (touchscreen two-finger scroll
stays Stage 8). Motion, key, and scroll events drain from GameActivity's
`android_input_buffer` every tick and forward to phenotype's shell via
`phenotype_android_dispatch_{pointer,key,char,scroll}`; the Android
renderer caches each flushed command buffer so `renderer.hit_test(x, y,
scroll_y)` can replay the `HitRegionCmd` list without another view
pass. `renderer_flush` also snapshots the presented swapchain image
into a persistent `debug_capture_image` so `capture_frame_rgba()`
returns fresh pixels without re-rendering.

The example survives window-create / window-destroy / rotation
lifecycles. After Stage 7, the first post-resume frame repaints
immediately — `phenotype_android_attach_surface` clears
`last_paint_hash` and calls `trigger_rebuild()` so the shell re-flushes
the current view against the fresh Vulkan state instead of short-
circuiting via the unchanged-buffer fast path. Soft keyboard show/hide
and IME composition remain follow-up work.

The driver calls four bootstrap hooks at `android_main` startup:
`phenotype_android_bind_jvm(app->activity->vm)` (text pipeline's JNI
attach), `phenotype_android_bind_activity(app->activity->javaGameActivity)`
(OS resource/configuration preferences such as Android `fontScale`),
`phenotype_android_bind_assets(app->activity->assetManager)` (`asset://`
URL resolver), and `phenotype_android_start_app()` on the first
`APP_CMD_INIT_WINDOW` (instantiates the library-side
`run_host<demo6::State, demo6::Msg>` so view + update wire into the
shell).

## URL schemes supported by `DrawImage`

| Scheme | Resolved via | Status |
|---|---|---|
| `asset://path.png` | `AAssetManager_open` + `AImageDecoder_createFromBuffer` | **supported** |
| `/absolute/path.png` (or `file:///absolute/path.png`) | `open()` + `AImageDecoder_createFromFd` | **supported** |
| `http://…` / `https://…` | — | **rejected** (`remote images not implemented (stage 7)`) |
| relative path | — | **rejected** (no CWD on Android) |

Rejected and failed URLs render a light-gray placeholder with a darker
2-pixel stroke so layout stays stable.

```
┌──────────────────┐   prefab    ┌──────────────────────────┐
│ androidx.games   │─────────────│ libgame-activity_static.a │
│ games-activity   │             └──────────────────────────┘
│ AAR              │
└──────────────────┘                  gradle build
                                            │
                                  ┌─────────▼─────────┐
                                  │ libphenotype_     │
                                  │ android_example.so│
                                  └────────┬──────────┘
                                           │ link
                                  ┌────────▼──────────┐
                                  │ libphenotype-     │
  exon build --target aarch64-    │ modules.a         │
  linux-android                   │ + libcppx.a       │
────────────────────────────────► │ + libtxn.a        │
                                  │ + libjsoncpp.a    │
                                  │ + lib__cmake_...  │
                                  └───────────────────┘
```

## Prerequisites

- Android SDK with platforms-android-35 + build-tools-35.x.
- JDK 17 (AGP 8.7 requirement).
- NDK r30-beta1. Either install it via Android Studio's SDK Manager
  (appears as revision `30.0.14904198-beta1`) or extract the official zip
  and symlink it into the SDK's NDK dir, e.g.:

  ```sh
  ln -s /tmp/ndk-r30/android-ndk-r30-beta1 \
        ~/Library/Android/sdk/ndk/30.0.14904198-beta1
  ```

- A phenotype toolchain (`mise install && eval "$(intron env)"`) that can
  produce the `aarch64-linux-android` static libraries.

## Run

From the phenotype repo root:

```sh
cd tools/phenotype_cli
mise exec -- exon build
.exon/debug/phenotype_cli android doctor --json   # SDK / NDK / JDK / AVD
.exon/debug/phenotype_cli android run --json      # boot emu -> build -> install -> launch
.exon/debug/phenotype_cli android logs --json     # bounded recent logcat sample
.exon/debug/phenotype_cli android contract --json # pull and verify a debug artifact bundle
```

The legacy `mise run android:*` tasks and scripts under
[`tools/android/`](../../tools/android/) remain available for compatibility.
New automation should prefer `phenotype android ...` so command names, JSON
output, and device-state overrides are stable. Override defaults via these
environment variables or the CLI options `--serial`, `--avd`, `--state-dir`,
and `--apk`:

| env | default | purpose |
|---|---|---|
| `ANDROID_HOME` | `~/Library/Android/sdk` (macOS), `~/Android/Sdk` (Linux) | SDK root |
| `ANDROID_NDK_HOME` | `$ANDROID_HOME/ndk/30.0.14904198-beta1`, fallback `/tmp/ndk-r30/android-ndk-r30-beta1` | NDK r30+ |
| `JAVA_HOME` | `/usr/libexec/java_home -v 17` on macOS | JDK 17 for AGP 8.7 |
| `PHENOTYPE_ANDROID_AVD` | first entry of `emulator -list-avds` | emulator target |
| `PHENOTYPE_ANDROID_PACKAGE` | `io.github.misut.phenotype.example` | `am` target |
| `PHENOTYPE_ANDROID_APK` | `examples/android/app/build/outputs/apk/debug/app-debug.apk` | install target |
| `PHENOTYPE_ANDROID_STATE_DIR` | `/tmp/phenotype-android` | emulator pid/log, screenshots, contract bundle |

The screen clears to the phenotype theme background
(`{250,250,250,255}` off-white by default). Home → pull recents →
reopen should not crash.

### Manual fallback

If you need to bypass the mise tasks (e.g. CI), the underlying commands are:

```sh
# 1. Static libs for aarch64-linux-android.
mise exec -- exon build --target aarch64-linux-android
# 2. APK.
(cd examples/android && ./gradlew assembleDebug)
# 3. Install + launch.
adb install -r examples/android/app/build/outputs/apk/debug/app-debug.apk
adb shell am start -n io.github.misut.phenotype.example/.MainActivity
adb logcat | grep phenotype
```

For automated debug-plane validation on an attached device or emulator:

```sh
cd tools/phenotype_cli
mise exec -- exon build
.exon/debug/phenotype_cli android contract --json
```

The contract runner enables the example's opt-in artifact hook with
`debug.phenotype.contract`, waits for `snapshot.json` and `frame.bmp`, pulls the
bundle from app-private storage with `adb run-as`, and validates it through the
CLI-owned Android contract command using
`examples/android/artifact_manifest.json`.
The pulled bundle lands in `$PHENOTYPE_ANDROID_STATE_DIR/contract-bundle` by
default.

## Overrides

Both the NDK version and the static-library path are overridable without
editing `app/build.gradle.kts`:

```sh
./gradlew assembleDebug \
    -PphenotypeNdkVersion=29.0.14033849 \
    -PphenotypeLibDir=/custom/exon/output/aarch64-linux-android/debug
```

## Shaders

The color pipeline's GLSL sources live in
`src/phenotype.native.android.shaders/` (`color.vert` + `color.frag`) and
are precompiled to SPIR-V ahead of time. The embedded `uint32_t` blobs
that the Vulkan backend uploads at runtime live in
`src/phenotype.native.android.shaders.inl`, which is checked in. Rerun
the regen script after editing any `.vert` / `.frag`:

```sh
ANDROID_NDK_HOME=/path/to/android-ndk-r30+ \
    ./tools/compile_android_shaders.sh
```

The script uses the host `glslc` bundled under
`${ANDROID_NDK_HOME}/shader-tools/<host>/`, targets `--target-env=vulkan1.1`,
and emits inline `uint32_t SPIRV_COLOR_VS[] / _FS[]` arrays.

## Extending

- Swap the example `View` / `Update` / `AppState` once Stages 4–5 land
  real text + image pipelines.
- Replace the Kotlin `MainActivity` with your own subclass of
  `GameActivity` if you need to hook Java-side lifecycle events.
- Gradle Prefab surfaces are declared in `app/build.gradle.kts`
  (`buildFeatures { prefab = true }`); add more AARs there if you need
  additional AGDK modules.
