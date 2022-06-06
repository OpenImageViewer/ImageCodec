#CodecPNG

cmake_minimum_required(VERSION 3.8)

file(GLOB_RECURSE sourceFiles
    "./Source/*.h"
    "./Source/*.cpp"
)

set(TargetName CodecPNG)
add_library (${TargetName} ${sourceFiles} )


set(LibPNGFolder ../../External/libpng)
set(zlibFolder ../../External/zlib)
add_subdirectory(${zlibFolder} ./external/zlib)
option(PNG_BUILD_ZLIB ON)
set(ZLIB_INCLUDE_DIR ${zlibFolder})
option(PNG_SHARED OFF)
option(PNG_TESTS OFF)

add_subdirectory(${LibPNGFolder} ./external/libpng)

target_include_directories(${TargetName} PRIVATE ${LibPNGFolder})
target_include_directories(${TargetName} PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/external/libpng)
target_include_directories(png_static PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/external/zlib)


target_link_libraries(${TargetName} png_static)
target_link_libraries(${TargetName} zlib)
