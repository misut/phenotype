// Root Gradle script. All plugin versions come from libs/version catalogues
// or the alias block below; subprojects only apply the plugins.
plugins {
    id("com.android.application")        version "8.7.3" apply false
    id("org.jetbrains.kotlin.android")    version "2.0.21" apply false
}
