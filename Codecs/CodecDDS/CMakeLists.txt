#Codec DDS
cmake_minimum_required(VERSION 3.10)

project(CodecDDS)

file(GLOB_RECURSE sourceFiles
    "./Source/*.h"
    "./Source/*.cpp"
)

set(TargetName CodecDDS)
add_library (${TargetName} ${sourceFiles} )

add_compile_definitions(NV_DDS_NO_GL_SUPPORT)
