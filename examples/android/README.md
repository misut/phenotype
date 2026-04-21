# phenotype · android example

Minimal Android application that boots phenotype's Vulkan backend inside a
[GameActivity](https://developer.android.com/games/agdk/game-activity). As
of Stage 2 it clears the surface to the active theme background on every
frame and survives window-create / window-destroy / rotation lifecycles.
Touch routing lands in Stage 6; text + image pipelines land in Stages 3–5.

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

## Extending

- Swap the example `View` / `Update` / `AppState` once Stages 3–5 land
  real color + text + image pipelines.
- Replace the Kotlin `MainActivity` with your own subclass of
  `GameActivity` if you need to hook Java-side lifecycle events.
- Gradle Prefab surfaces are declared in `app/build.gradle.kts`
  (`buildFeatures { prefab = true }`); add more AARs there if you need
  additional AGDK modules.
