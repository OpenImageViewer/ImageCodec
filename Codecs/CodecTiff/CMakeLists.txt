cmake_minimum_required(VERSION 3.8 FATAL_ERROR)

# Add source to this project's executable.
file(GLOB_RECURSE sourceFiles
    "./Source/*.h"
    "./Source/*.cpp"
)

set(TargetName CodecTiff)
add_library (${TargetName} ${sourceFiles} )
set(TIFF_BUILD_SHARED_LIBS TRUE)
set(LibTiffFolder ../../External/libtiff)
add_subdirectory(${LibTiffFolder} ./external/libtiff)
target_include_directories(${TargetName} PRIVATE ${LibTiffFolder}/libtiff)
target_include_directories(${TargetName} PRIVATE ${CMAKE_CURRENT_BINARY_DIR}./external/libtiff/libtiff)
target_link_libraries(${TargetName} tiff)