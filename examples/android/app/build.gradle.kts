plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

android {
    namespace = "io.github.misut.phenotype.example"
    compileSdk = 35
    // Aligns with the NDK exon uses to produce libphenotype-modules.a.
    // Override with -PphenotypeNdkVersion=... if your environment pins a
    // different revision.
    ndkVersion = (project.findProperty("phenotypeNdkVersion") as String?)
        ?: "30.0.14904198-beta1"

    defaultConfig {
        applicationId = "io.github.misut.phenotype.example"
        minSdk = 33
        targetSdk = 35
        versionCode = 1
        versionName = "0.1.0"

        ndk {
            // Stage 2 ships aarch64 only. Revisit once exon grows
            // multi-ABI support for Android.
            abiFilters += "arm64-v8a"
        }

        externalNativeBuild {
            cmake {
                cppFlags("-std=c++23")
                // Point the example CMake at the static library exon
                // produces for aarch64-linux-android. Override from the
                // command line if your output lives elsewhere:
                //   -PphenotypeLibDir=/custom/path
                val phenotypeLibDir = (project.findProperty("phenotypeLibDir") as String?)
                    ?: "${rootDir}/../../.exon/aarch64-linux-android/debug"
                arguments(
                    "-DANDROID_STL=c++_shared",
                    "-DPHENOTYPE_LIB_DIR=$phenotypeLibDir"
                )
            }
        }
    }

    buildFeatures {
        prefab = true
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.28.0+"
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    kotlinOptions {
        jvmTarget = "17"
    }

    buildTypes {
        release {
            isMinifyEnabled = false
        }
    }
}

dependencies {
    implementation("androidx.games:games-activity:3.0.5")
    implementation("androidx.appcompat:appcompat:1.7.1")
    implementation("androidx.core:core:1.16.0")
}
