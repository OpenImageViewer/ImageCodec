cmake_minimum_required(VERSION 3.8 FATAL_ERROR)

# Add source to this project's executable.
file(GLOB_RECURSE sourceFiles
    "./Source/*.h"
    "./Source/*.cpp"
)

set(TargetName CodecPSD)
add_library (${TargetName} ${sourceFiles} )
set(LibPSDFolder ../../External/libpsd)
add_subdirectory(${LibPSDFolder} ./external/libpsd)
include_directories(${LibPSDFolder}/include)
target_link_libraries(${TargetName} libpsd)