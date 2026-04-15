cmake_minimum_required(VERSION 3.28)
project(metal-cpp LANGUAGES CXX)
add_library(metal-cpp INTERFACE)
target_include_directories(metal-cpp INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(metal-cpp INTERFACE
    "-framework Metal"
    "-framework QuartzCore"
    "-framework CoreText"
    "-framework CoreGraphics"
    "-framework CoreFoundation"
    "-framework Foundation"
)
