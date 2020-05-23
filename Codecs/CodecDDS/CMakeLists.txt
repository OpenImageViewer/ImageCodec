#Codec DDS
cmake_minimum_required(VERSION 3.8)

file(GLOB_RECURSE sourceFiles
    "./Source/*.h"
    "./Source/*.cpp"
)

set(TargetName CodecDDS)

add_library (${TargetName} ${sourceFiles})
target_include_directories(${TargetName} PRIVATE ../../ImageLoader/Include)
target_compile_definitions(${TargetName} PRIVATE NV_DDS_NO_GL_SUPPORT)
