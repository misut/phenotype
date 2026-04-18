cmake_minimum_required(VERSION 3.30)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "451f2fe2-a8a2-47c3-bc32-94786d8fc91b")
set(CMAKE_CXX_MODULE_STD ON)
project(native LANGUAGES CXX)

if(MSVC)
    add_compile_options(/W4 /wd4996 /utf-8)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif()

# GLFW
include(FetchContent)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
    GIT_SHALLOW ON
    EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(glfw)

if(APPLE)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../libs/metal-cpp
                     ${CMAKE_BINARY_DIR}/_deps/metal-cpp-build)
endif()

# phenotype git dependencies
FetchContent_Declare(jsoncpp
    GIT_REPOSITORY https://github.com/misut/jsoncpp.git
    GIT_TAG v0.1.4
    GIT_SHALLOW ON
)
FetchContent_Declare(cppx
    GIT_REPOSITORY https://github.com/misut/cppx.git
    GIT_TAG v1.6.0
    GIT_SHALLOW ON
)
FetchContent_Declare(txn
    GIT_REPOSITORY https://github.com/misut/txn.git
    GIT_TAG v0.6.2
    GIT_SHALLOW ON
)
FetchContent_MakeAvailable(jsoncpp cppx txn)

# phenotype library
set(PHENOTYPE_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)
add_library(phenotype)
target_sources(phenotype
    PUBLIC FILE_SET CXX_MODULES
    BASE_DIRS ${PHENOTYPE_ROOT}
    FILES
    ${PHENOTYPE_ROOT}/src/phenotype.types.cppm
    ${PHENOTYPE_ROOT}/src/phenotype.host.cppm
    ${PHENOTYPE_ROOT}/src/phenotype.commands.cppm
    ${PHENOTYPE_ROOT}/src/phenotype.diag.cppm
    ${PHENOTYPE_ROOT}/src/phenotype.layout.cppm
    ${PHENOTYPE_ROOT}/src/phenotype.state.cppm
    ${PHENOTYPE_ROOT}/src/phenotype.paint.cppm
    ${PHENOTYPE_ROOT}/src/phenotype.theme_json.cppm
    ${PHENOTYPE_ROOT}/src/phenotype.wasm.cppm
    ${PHENOTYPE_ROOT}/src/phenotype.native.platform.cppm
    ${PHENOTYPE_ROOT}/src/phenotype.native.stub.cppm
    ${PHENOTYPE_ROOT}/src/phenotype.native.shell.cppm
    ${PHENOTYPE_ROOT}/src/phenotype.native.macos.cppm
    ${PHENOTYPE_ROOT}/src/phenotype.native.windows.cppm
    ${PHENOTYPE_ROOT}/src/phenotype.cppm
    ${PHENOTYPE_ROOT}/src/phenotype.native.cppm
)
target_link_libraries(phenotype PUBLIC
    jsoncpp txn glfw
)

if(APPLE)
    target_link_libraries(phenotype PUBLIC metal-cpp)
endif()

# example
add_executable(native src/main.cpp)
target_link_libraries(native PRIVATE phenotype)
target_compile_definitions(native PRIVATE
    PHENOTYPE_EXAMPLE_ASSET_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/assets"
)
add_custom_command(TARGET native POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/assets
            $<TARGET_FILE_DIR:native>/assets
)
