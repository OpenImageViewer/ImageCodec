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
            )

            target_compile_options(port PRIVATE 
            -Wno-missing-prototypes
            -Wno-strict-prototypes
            -Wno-reserved-macro-identifier
            -Wno-reserved-identifier
            -Wno-missing-variable-declarations
            )
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        target_compile_options(tiff PRIVATE /wd4996)
    endif()
    target_compile_definitions(tiff PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()




target_include_directories(${TargetName} PRIVATE ${LibTiffFolder}/libtiff)
target_include_directories(${TargetName} PRIVATE ${CMAKE_CURRENT_BINARY_DIR}./external/libtiff/libtiff)



if(MSVC)
   #set_property(TARGET tiff PROPERTY WINDOWS_EXPORT_ALL_SYMBOLS true)
   target_link_libraries(${TargetName} tiff)
   target_link_options(${TargetName} PUBLIC "/DELAYLOAD:$<TARGET_FILE_BASE_NAME:tiff>.dll")
endif()
