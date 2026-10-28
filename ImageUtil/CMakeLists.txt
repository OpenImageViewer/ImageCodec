 file(GLOB_RECURSE sourceFiles
     "./Source/*.h"
     "./Source/*.cpp"
 )

set(TargetName ImageUtil)
add_library (${TargetName} STATIC ${sourceFiles})

target_include_directories(${TargetName} PUBLIC ./Include)
target_include_directories(${TargetName} PRIVATE ./Source)
target_include_directories(${TargetName} PRIVATE ../External/ExoticNumbers/include)
# Transfer scales are external metadata. Preserve NaN and infinity checks despite global fast-math flags.
if(CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    set_source_files_properties(Source/TransferFunction.cpp PROPERTIES COMPILE_OPTIONS "/fp:precise")
else()
    set_source_files_properties(Source/TransferFunction.cpp PROPERTIES COMPILE_OPTIONS "-fno-fast-math")
endif()
