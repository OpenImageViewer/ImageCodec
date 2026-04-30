 file(GLOB_RECURSE sourceFiles
     "./Source/*.h"
     "./Source/*.cpp"
 )

set(TargetName ImageUtil)
add_library (${TargetName} STATIC ${sourceFiles})

target_include_directories(${TargetName} PUBLIC ./Include)
target_include_directories(${TargetName} PRIVATE ./Source)
target_include_directories(${TargetName} PRIVATE ../External/ExoticNumbers/include)