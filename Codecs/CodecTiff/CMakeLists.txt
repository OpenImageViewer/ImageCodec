cmake_minimum_required(VERSION 3.10)

# Add source to this project's executable.
file(GLOB_RECURSE sourceFiles
    "./Source/*.h"
    "./Source/*.cpp"
)

set(TargetName CodecTiff)
add_library (${TargetName} ${sourceFiles} )

option(tiff-static "build TIFF static libraries" OFF)
option(tiff-tools "build TIFF tools" OFF)
option(tiff-tests "build TIFF tests" OFF)
option(tiff-contrib "build TIFF contrib" OFF)
option(tiff-docs "build TIFF documentation" OFF)
option(tiff-deprecated "build TIFF deprecated features" OFF)

set(TIFF_BUILD_CXX FALSE)
set(LibTiffFolder ../../External/libtiff)
add_subdirectory(${LibTiffFolder} ./external/libtiff)

if (IMCODEC_DISABLE_WARNINGS_EXTERNAL_LIBS)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
       target_compile_options(tiff PRIVATE 
            -Wno-nonportable-system-include-path 
            -Wno-cast-align 
            -Wno-strict-prototypes 
            -Wno-double-promotion
            -Wno-deprecated-declarations 
            -Wno-reserved-identifier 
            -Wno-sign-conversion
            -Wno-format-nonliteral
            -Wno-unused-macros
            -Wno-language-extension-token
            -Wno-undef
            -Wno-extra-semi-stmt
            -Wno-format-non-iso
            -Wno-missing-variable-declarations
            -Wno-missing-prototypes
            -Wno-cast-qual
            -Wno-implicit-fallthrough
            -Wno-switch-enum
            -Wno-covered-switch-default
            -Wno-implicit-int-float-conversion
            -Wno-float-equal
            -Wno-bad-function-cast
            -Wno-implicit-int-conversion
            -Wno-unused-but-set-variable
            -Wno-unused-variable
            -Wno-reserved-macro-identifier
            -Wno-unused-parameter
            -Wno-unsafe-buffer-usage
            -Wno-switch-default
            )

    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        target_compile_options(tiff PRIVATE -Wno-array-parameter -Wno-misleading-indentation)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        target_compile_options(tiff PRIVATE /wd4996)
    endif()
    target_compile_definitions(tiff PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()


target_include_directories(${TargetName} PRIVATE ${LibTiffFolder}/libtiff)
target_include_directories(${TargetName} PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/external/libtiff/libtiff)


target_link_libraries(${TargetName} tiff)
if (${CMAKE_SYSTEM_NAME} MATCHES "Windows" AND (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" OR CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC") AND NOT MINGW)
   target_link_options(${TargetName} PUBLIC "/DELAYLOAD:$<TARGET_FILE_BASE_NAME:tiff>.dll")
endif()
