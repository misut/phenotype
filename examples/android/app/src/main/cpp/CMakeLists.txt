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

add_library(phenotype_modules_static STATIC IMPORTED)
set_target_properties(phenotype_modules_static PROPERTIES
    IMPORTED_LOCATION "${PHENOTYPE_LIB_DIR}/libphenotype-modules.a")

add_library(phenotype_android_example SHARED
    phenotype_android_main.cpp
)

target_link_libraries(phenotype_android_example PRIVATE
    phenotype_modules_static
    game-activity::game-activity_static
    vulkan
    android
    log)
