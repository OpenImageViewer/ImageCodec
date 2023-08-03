cmake_minimum_required(VERSION 3.8 FATAL_ERROR)

# Add source to this project's executable.
file(GLOB_RECURSE sourceFiles
    "./Source/*.h"
    "./Source/*.cpp"
)

set(TargetName CodecTiff)
add_library (${TargetName} ${sourceFiles} )
set(TIFF_BUILD_SHARED_LIBS TRUE)
set(TIFF_BUILD_CXX FALSE)
set(LibTiffFolder ../../External/libtiff)
add_subdirectory(${LibTiffFolder} ./external/libtiff)
target_include_directories(${TargetName} PRIVATE ${LibTiffFolder}/libtiff)
target_include_directories(${TargetName} PRIVATE ${CMAKE_CURRENT_BINARY_DIR}./external/libtiff/libtiff)



if(MSVC)
   #set_property(TARGET tiff PROPERTY WINDOWS_EXPORT_ALL_SYMBOLS true)
   target_link_libraries(${TargetName} tiff)
   target_link_options(${TargetName} PUBLIC "/DELAYLOAD:$<TARGET_FILE_BASE_NAME:tiff>.dll")
endif()

