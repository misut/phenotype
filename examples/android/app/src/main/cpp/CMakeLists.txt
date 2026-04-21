cmake_minimum_required(VERSION 3.28)

project(phenotype_android_example LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# GameActivity ships as a Prefab-backed AAR; AGP extracts the headers
# and static library into the build tree and exposes this CONFIG.
find_package(game-activity REQUIRED CONFIG)

# PHENOTYPE_LIB_DIR points at exon's aarch64-linux-android output. See
# app/build.gradle.kts for the default.
if(NOT DEFINED PHENOTYPE_LIB_DIR)
    message(FATAL_ERROR
        "PHENOTYPE_LIB_DIR not set. Build phenotype first:\n"
        "  cd <repo>; mise exec -- exon build --target aarch64-linux-android\n"
        "then rerun ./gradlew assembleDebug.")
endif()

function(phenotype_import_static name path)
    add_library(${name} STATIC IMPORTED)
    set_target_properties(${name} PROPERTIES IMPORTED_LOCATION "${path}")
endfunction()

# Keep this list in sync with the set of targets exon emits for
# aarch64-linux-android. libphenotype-modules depends transitively on
# cppx / txn / jsoncpp / the generated std module object.
phenotype_import_static(phenotype_modules_static
    "${PHENOTYPE_LIB_DIR}/libphenotype-modules.a")
phenotype_import_static(phenotype_cppx_static
    "${PHENOTYPE_LIB_DIR}/libcppx.a")
phenotype_import_static(phenotype_txn_static
    "${PHENOTYPE_LIB_DIR}/libtxn.a")
phenotype_import_static(phenotype_jsoncpp_static
    "${PHENOTYPE_LIB_DIR}/libjsoncpp.a")
phenotype_import_static(phenotype_cxx_std_static
    "${PHENOTYPE_LIB_DIR}/lib__cmake_cxx_std_23.a")

add_library(phenotype_android_example SHARED
    phenotype_android_main.cpp
)

target_link_libraries(phenotype_android_example PRIVATE
    # --start-group lets the static archives resolve each other's
    # symbols regardless of command-line order.
    -Wl,--start-group
    phenotype_modules_static
    phenotype_cppx_static
    phenotype_txn_static
    phenotype_jsoncpp_static
    phenotype_cxx_std_static
    -Wl,--end-group
    # GameActivity's JNI entry points (Java_com_google_androidgamesdk_*)
    # are only referenced from Java, so without --whole-archive the
    # linker strips them and the JVM throws UnsatisfiedLinkError at
    # onCreate time.
    -Wl,--whole-archive
    game-activity::game-activity_static
    -Wl,--no-whole-archive
    vulkan
    android
    log)
