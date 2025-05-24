cmake_minimum_required(VERSION 3.10)

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

if (IMCODEC_DISABLE_WARNINGS_EXTERNAL_LIBS)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        target_compile_options(libpsd PRIVATE -Wno-switch -Wno-unused-but-set-variable -Wno-implicit-function-declaration -Wno-pointer-sign -Wno-deprecated-declarations
        -Wno-pointer-to-int-cast -Wno-unused-variable -Wno-incompatible-function-pointer-types -Wno-int-to-pointer-cast -Wno-sometimes-uninitialized
        -Wno-unused-function -Wno-enum-conversion -Wno-empty-body)
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
            target_compile_options(libpsd PRIVATE -Wno-builtin-declaration-mismatch -Wno-multichar -Wno-incompatible-pointer-types)
        endif()
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        target_compile_options(libpsd PRIVATE /wd4668 /wd4013 /wd4311 /wd4312 /wd4267 /wd4101 /wd4996)
    endif()
endif()
