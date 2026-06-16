cmake_minimum_required(VERSION 3.30)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "451f2fe2-a8a2-47c3-bc32-94786d8fc91b")
set(CMAKE_CXX_MODULE_STD ON)
project(phenotype-windows LANGUAGES CXX)

if(NOT WIN32)
  message(FATAL_ERROR "phenotype-windows is only supported on Windows")
endif()

if(NOT TARGET phenotype-core)
  add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../core
    ${CMAKE_BINARY_DIR}/_deps/phenotype-core-build)
endif()

if(TARGET phenotype-windows)
  return()
endif()

add_library(phenotype-windows STATIC)
target_sources(phenotype-windows
  PUBLIC
    FILE_SET CXX_MODULES
    BASE_DIRS ${CMAKE_CURRENT_LIST_DIR}
    FILES ${CMAKE_CURRENT_LIST_DIR}/src/phenotype.windows.cppm
  PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/src/native/phenotype.windows.cpp
)
target_include_directories(phenotype-windows PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/include
)
target_compile_definitions(phenotype-windows PRIVATE
  UNICODE
  WIN32_LEAN_AND_MEAN
  NOMINMAX
)
target_link_libraries(phenotype-windows PUBLIC
  phenotype-core
  user32
  dwmapi
)
