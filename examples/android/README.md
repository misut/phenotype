# phenotype · android example

Minimal Android application that boots phenotype's Vulkan backend inside a
[GameActivity](https://developer.android.com/games/agdk/game-activity). As
of Stage 6 it runs a real `view(State) / update(State, Msg)` loop through
phenotype's shell. The built-in counter demo exercises the full three-
pipeline stack (color + text + image) plus touch + hardware-key input:
tap the `−` / `+` buttons to mutate `State.count`, Tab to cycle focus,
Enter / Space to trigger the focused button. Motion and key events drain
from GameActivity's `android_input_buffer` every tick and forward to
phenotype's shell via `phenotype_android_dispatch_{pointer,key,char}`;
the Android renderer caches each flushed command buffer so
`renderer.hit_test(x, y, scroll_y)` can replay the `HitRegionCmd` list
without another view pass.

The example survives window-create / window-destroy / rotation
lifecycles. The first post-resume frame may flash blank briefly while
Vulkan rebuilds the swapchain — the first new input or state change
repaints the current view. Touch scroll and soft keyboard show/hide
land in Stage 7.

The driver calls three bootstrap hooks at `android_main` startup:
`phenotype_android_bind_jvm(app->activity->vm)` (text pipeline's JNI
attach), `phenotype_android_bind_assets(app->activity->assetManager)`
(`asset://` URL resolver), and `phenotype_android_start_app()` on the
first `APP_CMD_INIT_WINDOW` (instantiates the library-side
`run_host<demo6::State, demo6::Msg>` so view + update wire into the
shell).

## URL schemes supported by `DrawImage`

| Scheme | Resolved via | Status |
|---|---|---|
| `asset://path.png` | `AAssetManager_open` + `AImageDecoder_createFromBuffer` | **supported** |
| `/absolute/path.png` (or `file:///absolute/path.png`) | `open()` + `AImageDecoder_createFromFd` | **supported** |
| `http://…` / `https://…` | JNI `HttpsURLConnection` worker | Stage 7 |
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

## Build

1. Build phenotype for Android:

   ```sh
   cd ../../..  # phenotype repo root
   ANDROID_NDK_HOME=/tmp/ndk-r30/android-ndk-r30-beta1 \
     mise exec -- exon build --target aarch64-linux-android
   ```

   This produces the `.a` archives under
   `.exon/aarch64-linux-android/debug/`.

2. Build + install the APK:

   ```sh
   cd examples/android
   export ANDROID_HOME=~/Library/Android/sdk
   export JAVA_HOME=...JDK17...
   ./gradlew assembleDebug
   adb install -r app/build/outputs/apk/debug/app-debug.apk
   ```

3. Launch:

   ```sh
   adb shell am start -n io.github.misut.phenotype.example/.MainActivity
   adb logcat | grep phenotype
   ```

   The screen should clear to the phenotype theme background
   (`{250,250,250,255}` off-white by default). Home → pull recents →
   reopen should not crash.

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
