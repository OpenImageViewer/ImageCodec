#CodecGif

cmake_minimum_required(VERSION 3.10)


file(GLOB_RECURSE sourceFiles
    "./Source/*.h"
    "./Source/*.cpp"
)

set(TargetName CodecGif)
add_library (${TargetName} ${sourceFiles})


set(giflibFolder ../../External/libgif)
add_subdirectory(${giflibFolder} ./external/libgif)

if (IMCODEC_DISABLE_WARNINGS_EXTERNAL_LIBS)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(libgif PRIVATE -Wno-deprecated-declarations)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        target_compile_options(libgif PRIVATE /wd4018 /wd4996 /wd4267 /wd4244)
    endif()
endif()

include_directories(${giflibFolder}/libgif/include)
target_link_libraries(${TargetName} libgif)