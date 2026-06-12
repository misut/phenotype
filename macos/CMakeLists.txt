cmake_minimum_required(VERSION 3.30)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "451f2fe2-a8a2-47c3-bc32-94786d8fc91b")
set(CMAKE_CXX_MODULE_STD ON)
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

if(NOT TARGET phenotype-core)
  add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../core
    ${CMAKE_BINARY_DIR}/_deps/phenotype-core-build)
endif()

if(TARGET phenotype-macos)
  return()
endif()

add_library(phenotype-macos STATIC)
target_sources(phenotype-macos
  PUBLIC
    FILE_SET CXX_MODULES
    BASE_DIRS ${CMAKE_CURRENT_LIST_DIR}
    FILES ${CMAKE_CURRENT_LIST_DIR}/src/phenotype.macos.cppm
  PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/src/native/phenotype.macos.cpp
)
target_link_libraries(phenotype-macos PUBLIC
  phenotype-core
  webgpu_dawn
)
target_compile_options(phenotype-macos PRIVATE
  -x
  objective-c++
)
target_link_options(phenotype-macos PUBLIC
  LINKER:-framework,AppKit
  LINKER:-framework,QuartzCore
)
