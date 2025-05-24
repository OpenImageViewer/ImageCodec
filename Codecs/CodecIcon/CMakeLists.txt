#Codec Icon
cmake_minimum_required(VERSION 3.10)

project(CodecIcon)

file(GLOB_RECURSE sourceFiles
    "./Source/*.h"
    "./Source/*.cpp"
)

set(TargetName CodecIcon)
add_library (${TargetName} ${sourceFiles} )

