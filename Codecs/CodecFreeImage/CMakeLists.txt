#CodecFreeImage

cmake_minimum_required(VERSION 3.10)

# Add source to this project's executable.
file(GLOB_RECURSE sourceFiles
    "./Source/*.h"
    "./Source/*.cpp"
)

set(TargetName CodecFreeImage)
add_library (${TargetName} STATIC ${sourceFiles})

set(FreeImageReFolder ${CMAKE_CURRENT_LIST_DIR}/../../External/FreeImageRe)
set(EXTERNALPROJECT_SOURCE_ROOT ${CMAKE_CURRENT_BINARY_DIR}/External/FreeImageRe/dependencies/source CACHE PATH "FreeImageRe dependency source directory")
set(EXTERNALPROJECT_BINARY_ROOT ${CMAKE_CURRENT_BINARY_DIR}/External/FreeImageRe/dependencies/build CACHE PATH "FreeImageRe dependency build directory")
foreach(FreeImageReBackend LIBJPEG LIBOPENJPEG LIBOPENEXR LIBPNG LIBTIFF LIBWEBP LIBHEIF LIBJPEGXL)
    set(FREEIMAGE_WITH_${FreeImageReBackend} OFF CACHE BOOL "Compile FreeImageRe with the ${FreeImageReBackend} backend" FORCE)
endforeach()

if(NOT TARGET FreeImage)
    add_subdirectory(${FreeImageReFolder} ${CMAKE_CURRENT_BINARY_DIR}/External/FreeImageRe)
endif()

target_include_directories(${TargetName} PRIVATE ${FreeImageReFolder}/Source)
target_link_libraries(${TargetName} PRIVATE FreeImage)
