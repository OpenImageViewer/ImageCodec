#CodecFreeImage

cmake_minimum_required(VERSION 3.8)

# Add source to this project's executable.
file(GLOB_RECURSE sourceFiles
    "./Source/*.h"
    "./Source/*.cpp"
)

set(TargetName CodecFreeImage)
add_library (${TargetName} STATIC ${sourceFiles})
target_link_libraries(${TargetName} d:/DevelopmentExternal/FreeImage3180Win32Win64/FreeImage/Dist/x64/FreeImage.lib)
include_directories(D:/DevelopmentExternal/FreeImage3180Win32Win64/FreeImage/Dist/x64)

#FreeImage from source
#set(FreeImageFolder ../../External/FreeImage)
#add_subdirectory(${FreeImageFolder} ./External/FreeImage)
#target_include_directories(${TargetName} PRIVATE ${FreeImageFolder}/Source)
#target_link_libraries(${TargetName} PRIVATE FreeImage)