package io.github.misut.phenotype.example

import com.google.androidgamesdk.GameActivity

// The JVM side is intentionally empty: all app logic runs in
// phenotype_android_main.cpp via GameActivity's native_app_glue bridge.
class MainActivity : GameActivity()
