#Codec Icon
cmake_minimum_required(VERSION 3.10)

project(CodecBMP)

file(GLOB_RECURSE sourceFiles
    "./Source/*.h"
    "./Source/*.cpp"
)

set(TargetName CodecBMP)
add_library (${TargetName} ${sourceFiles} )

