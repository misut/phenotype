cmake_minimum_required(VERSION 3.30)

project(phenotype-macos LANGUAGES CXX)

if(NOT TARGET webgpu_dawn)
  include(FetchContent)

  set(DAWN_FETCH_DEPENDENCIES ON)
  set(DAWN_BUILD_BENCHMARKS OFF)
  set(DAWN_BUILD_NODE_BINDINGS OFF)
  set(DAWN_BUILD_PROTOBUF OFF)
  set(DAWN_BUILD_SAMPLES OFF)
  set(DAWN_BUILD_TESTS OFF)
  set(DAWN_ENABLE_DESKTOP_GL OFF)
  set(DAWN_ENABLE_NULL OFF)
  set(DAWN_ENABLE_OPENGLES OFF)
  set(DAWN_ENABLE_SPIRV_VALIDATION OFF)
  set(DAWN_ENABLE_VULKAN OFF)
  set(DAWN_USE_GLFW OFF)
  set(TINT_BUILD_CMD_TOOLS OFF)
  set(TINT_BUILD_GLSL_VALIDATOR OFF)
  set(TINT_BUILD_IR_BINARY OFF)
  set(TINT_BUILD_TESTS OFF)

  FetchContent_Declare(dawn
    GIT_REPOSITORY https://github.com/google/dawn.git
    GIT_TAG v20260423.175430
    GIT_SHALLOW ON
    GIT_SUBMODULES ""
    EXCLUDE_FROM_ALL
  )
  FetchContent_MakeAvailable(dawn)
endif()

add_library(phenotype-macos STATIC
  src/phenotype.macos.cpp
)

set_source_files_properties(src/phenotype.macos.cpp PROPERTIES
  COMPILE_OPTIONS "-x;objective-c++"
)

target_compile_features(phenotype-macos PUBLIC cxx_std_23)
target_include_directories(phenotype-macos PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../include
)
target_link_libraries(phenotype-macos PUBLIC
  webgpu_dawn
)
target_link_options(phenotype-macos PUBLIC
  LINKER:-framework,AppKit
  LINKER:-framework,QuartzCore
)
