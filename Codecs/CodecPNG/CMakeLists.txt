#CodecPNG

cmake_minimum_required(VERSION 3.8)

file(GLOB_RECURSE sourceFiles
    "./Source/*.h"
    "./Source/*.cpp"
)

set(TargetName CodecPNG)
add_library (${TargetName} ${sourceFiles} )

#ZLIB
option(ZLIB_BUILD_BINARIES "Build zlib test sample and example" OFF)
set(LibPNGFolder ../../External/libpng)
set(zlibFolder ../../External/zlib)

add_subdirectory(${zlibFolder} ./external/zlib)
set(ZLIB_INCLUDE_DIRS ${zlibFolder})
set(ZLIB_LIBRARIES zlib)

#LibPNG
option(PNG_SHARED       "Build shared lib" ON)
option(PNG_STATIC       "Build static lib" OFF)
option(PNG_TESTS        "Build libpng tests" OFF)
option(PNG_FIX          "Build pngfix" OFF)
option(PNG_FIX_ITXT     "Build pngfix itext" OFF)
option(PNG_BUILD_ZLIB   "Custom zlib Location, else find_package is used" ON)


set(SKIP_INSTALL_LIBRARIES TRUE)
set(SKIP_INSTALL_ALL TRUE)

add_subdirectory(${LibPNGFolder} ./external/libpng)

target_include_directories(${TargetName} PRIVATE ${LibPNGFolder})
target_include_directories(${TargetName} PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/external/libpng)
target_link_libraries(${TargetName} png_shared )
target_include_directories(png_shared PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/external/zlib)

if(MSVC)
  target_link_options(${TargetName} PUBLIC "/DELAYLOAD:$<TARGET_FILE_BASE_NAME:png_shared>.dll")
  target_link_options(${TargetName} PUBLIC "/DELAYLOAD:$<TARGET_FILE_BASE_NAME:zlib>.dll")
endif()