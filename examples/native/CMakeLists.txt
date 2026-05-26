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

include(FetchContent)
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
    GIT_TAG v1.6.1
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
    ${PHENOTYPE_ROOT}/src/phenotype.native.shell.macos.cppm
    ${PHENOTYPE_ROOT}/src/phenotype.native.shell.windows.cppm
    ${PHENOTYPE_ROOT}/src/phenotype.native.macos.objc.cppm
    ${PHENOTYPE_ROOT}/src/phenotype.native.macos.text.cppm
    ${PHENOTYPE_ROOT}/src/phenotype.native.macos.image.cppm
    ${PHENOTYPE_ROOT}/src/phenotype.native.macos.render.cppm
    ${PHENOTYPE_ROOT}/src/phenotype.native.macos.material.cppm
    ${PHENOTYPE_ROOT}/src/phenotype.native.macos.commands.cppm
    ${PHENOTYPE_ROOT}/src/phenotype.native.macos.cppm
    ${PHENOTYPE_ROOT}/src/phenotype.native.windows.cppm
    ${PHENOTYPE_ROOT}/src/phenotype.cppm
    ${PHENOTYPE_ROOT}/src/phenotype.native.cppm
)
target_link_libraries(phenotype PUBLIC
    jsoncpp cppx txn
)

if(APPLE)
    target_link_libraries(phenotype PUBLIC
        metal-cpp
        "-framework AppKit"
        "-framework CoreGraphics"
        "-framework CoreText"
        "-framework Foundation"
        "-framework Metal"
        "-framework QuartzCore"
        "-framework Security"
        "-framework CoreFoundation"
    )
elseif(WIN32)
    target_link_libraries(phenotype PUBLIC
        d3d12
        d3dcompiler
        dwmapi
        dxgi
        dwrite
        gdi32
        imm32
        ole32
        user32
        windowscodecs
    )
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
