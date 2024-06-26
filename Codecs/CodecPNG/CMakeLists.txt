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
cmake_path(ABSOLUTE_PATH zlibFolder NORMALIZE OUTPUT_VARIABLE zlibFolder)

#Remove -Wdeprecated-non-prototype for zblib
if (IMCODEC_DISABLE_WARNINGS_EXTERNAL_LIBS)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        add_compile_options(-Wno-deprecated-non-prototype)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        add_compile_options(/wd4244)
    endif()
endif()


add_subdirectory(${zlibFolder} ./external/zlib)

list(APPEND ZLIB_INCLUDE_DIRS "${zlibFolder}")
list(APPEND ZLIB_INCLUDE_DIRS "${CMAKE_CURRENT_BINARY_DIR}/external/zlib")
set(ZLIB_LIBRARIES zlib)

#LibPNG
option(PNG_SHARED       "Build shared lib" ON)
option(PNG_STATIC       "Build static lib" OFF)
option(PNG_TESTS        "Build libpng tests" OFF)
option(PNG_EXECUTABLES  "Build libpng executables" OFF)
option(PNG_BUILD_ZLIB   "Custom zlib Location, else find_package is used" ON)


set(SKIP_INSTALL_LIBRARIES TRUE)
set(SKIP_INSTALL_ALL TRUE)

add_subdirectory(${LibPNGFolder} ./external/libpng)

if (IMCODEC_DISABLE_WARNINGS_EXTERNAL_LIBS)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(png_shared PRIVATE -Wno-unused-variable -Wno-return-type -Wno-tautological-constant-out-of-range-compare)
    endif()
endif()

target_include_directories(${TargetName} PRIVATE ${LibPNGFolder})
target_include_directories(${TargetName} PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/external/libpng)
target_link_libraries(${TargetName} png_shared )
target_include_directories(png_shared PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/external/zlib)

if (${CMAKE_SYSTEM_NAME} MATCHES "Windows" AND (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" OR CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC"))
  target_link_options(${TargetName} PUBLIC "/DELAYLOAD:$<TARGET_FILE_BASE_NAME:png_shared>.dll")
  target_link_options(${TargetName} PUBLIC "/DELAYLOAD:$<TARGET_FILE_BASE_NAME:zlib>.dll")
endif()