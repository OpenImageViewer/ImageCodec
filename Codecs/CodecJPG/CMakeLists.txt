#Codec JPG
cmake_minimum_required(VERSION 3.10)

include(ExternalProject)

set(LibJpegTurboFolder ${CMAKE_CURRENT_SOURCE_DIR}/../../External/libjpeg-turbo)
get_filename_component(LibJpegTurboFolder "${LibJpegTurboFolder}" ABSOLUTE)
set(NASMPATH ${CMAKE_CURRENT_SOURCE_DIR}/../../External/bintools/nasm-2.15.05/nasm.exe)
get_filename_component(NASMPATH "${NASMPATH}" ABSOLUTE)


# =========================
# =========================

# ======== Detect Multi-config vs Single-config ========
if(CMAKE_CONFIGURATION_TYPES)
    set(IS_MULTI_CONFIG TRUE)
else()
    set(IS_MULTI_CONFIG FALSE)
endif()

# ======== Platform-specific file extensions ========
if(WIN32)
    if (MINGW)
        set(JPG_RUNTIME_NAME "libturbojpeg.dll")
        set(JPG_IMPLIB_NAME "libturbojpeg.dll.a")
    else()
        set(JPG_RUNTIME_NAME "turbojpeg.dll")
        set(JPG_IMPLIB_NAME "turbojpeg.lib")
    endif()
elseif(APPLE)
    set(JPG_RUNTIME_NAME  "libturbojpeg.dylib")
    set(JPG_IMPLIB_NAME "") # No import libs on macOS
else()
    set(JPG_RUNTIME_NAME "libturbojpeg.so")
    set(JPG_IMPLIB_NAME "") # No import libs on Linux
endif()

# ======== External build directory ========
set(TurboJpegBinBaseDir "${CMAKE_BINARY_DIR}/libjpeg-turbo")

# ======== CMake arguments for libjpeg-turbo ========
set(LibJpegTurboArgs
    -DCMAKE_RUNTIME_OUTPUT_DIRECTORY=${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    -DCMAKE_LIBRARY_OUTPUT_DIRECTORY=${CMAKE_LIBRARY_OUTPUT_DIRECTORY}
    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
    -DCMAKE_MT=${CMAKE_MT}
    -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
    -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
    -DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}
    -DCMAKE_RC_COMPILER=${CMAKE_RC_COMPILER}
    -DENABLE_STATIC=FALSE
    -DWITH_FUZZ=FALSE
    -DWITH_JPEG=FALSE
    -DCMAKE_INSTALL_PREFIX=${TurboJpegBinBaseDir}/install
)

if (CMAKE_TOOLCHAIN_FILE)
    get_filename_component(CMAKE_TOOLCHAIN_FILE_ABSOLUTE "${CMAKE_TOOLCHAIN_FILE}" ABSOLUTE)
    list(APPEND LibJpegTurboArgs -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE_ABSOLUTE})
endif()
# Enable NASM on Windows if needed
if(WIN32 AND NOT ${CMAKE_HOST_SYSTEM_NAME} STREQUAL "Linux")
    list(APPEND LibJpegTurboArgs -DCMAKE_ASM_NASM_COMPILER=${NASMPATH})
endif()

# Support Custom runtime and library output directories
if (CMAKE_RUNTIME_OUTPUT_DIRECTORY)
    set(JPG_RUNTIME_OUTPUT_DIR ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
else()
    set(JPG_RUNTIME_OUTPUT_DIR ${TurboJpegBinBaseDir})
endif()


if (CMAKE_LIBRARY_OUTPUT_DIRECTORY)
    set(JPG_LIB_OUTPUT_DIR ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})
else()
    set(JPG_LIB_OUTPUT_DIR ${TurboJpegBinBaseDir})
endif()

# ======== Declare Imported Library ========
add_library(libjpeg-turbo::libjpeg-turbo SHARED IMPORTED GLOBAL)

set(JPG_IMPORTED_LOCATION "")
set(JPG_IMPORTED_IMPLIB "")


if(IS_MULTI_CONFIG)
    # Multi-config: set per-config imported locations
    foreach(cfg ${CMAKE_CONFIGURATION_TYPES})
        string(TOUPPER "${cfg}" cfg_upper)
        list(APPEND JPG_IMPORTED_LOCATION "IMPORTED_LOCATION_${cfg_upper}" "${JPG_RUNTIME_OUTPUT_DIR}/${cfg}/${JPG_RUNTIME_NAME}")
        set_target_properties(libjpeg-turbo::libjpeg-turbo PROPERTIES 
        "IMPORTED_LOCATION_${cfg_upper}" "${JPG_RUNTIME_OUTPUT_DIR}/${cfg}/${JPG_RUNTIME_NAME}")
        # Set import library on Windows
        if(WIN32)
        list(APPEND JPG_IMPORTED_IMPLIB "IMPORTED_IMPLIB_${cfg_upper}" "${JPG_LIB_OUTPUT_DIR}/${cfg}/${JPG_IMPLIB_NAME}")
            set_target_properties(libjpeg-turbo::libjpeg-turbo PROPERTIES 
            "IMPORTED_IMPLIB_${cfg_upper}" "${JPG_LIB_OUTPUT_DIR}/${cfg}/${JPG_IMPLIB_NAME}")
        endif()
    endforeach()
else()

    set(JPG_IMPORTED_LOCATION "${JPG_RUNTIME_OUTPUT_DIR}/${JPG_RUNTIME_NAME}")
    # Single-config: flat directory
    set_target_properties(libjpeg-turbo::libjpeg-turbo PROPERTIES IMPORTED_LOCATION 
    "${JPG_RUNTIME_OUTPUT_DIR}/${JPG_RUNTIME_NAME}")

    # Set import library on Windows
    if(WIN32)
    set(JPG_IMPORTED_IMPLIB "${JPG_LIB_OUTPUT_DIR}/${JPG_IMPLIB_NAME}")
        set_target_properties(libjpeg-turbo::libjpeg-turbo PROPERTIES IMPORTED_IMPLIB
         "${JPG_LIB_OUTPUT_DIR}/${JPG_IMPLIB_NAME}")
    endif()
endif()

# ======== Include directories ========
set_target_properties(libjpeg-turbo::libjpeg-turbo PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${LibJpegTurboFolder}/src"
)

list ( APPEND JPG_BYPRODUCTS  ${JPG_IMPORTED_LOCATION} )
if (WIN32)
    list (APPEND JPG_BYPRODUCTS ${JPG_IMPORTED_IMPLIB} )
endif()

# ======== Add ExternalProject ========
ExternalProject_Add(libjpeg-turbo
    SOURCE_DIR ${LibJpegTurboFolder}
    BINARY_DIR ${TurboJpegBinBaseDir}
    CMAKE_ARGS ${LibJpegTurboArgs}
    INSTALL_COMMAND ""
    BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> --config $<CONFIG> --target turbojpeg
    BYPRODUCTS ${JPG_BYPRODUCTS} 
)

# ======== Depend on ExternalProject ========
add_dependencies(libjpeg-turbo::libjpeg-turbo libjpeg-turbo)


# =========================

 file(GLOB_RECURSE sourceFiles
     "./Source/*.h"
     "./Source/*.cpp"
 )

set(TargetName CodecJPG)
add_library (${TargetName} ${sourceFiles} )

target_link_libraries(${TargetName} PRIVATE libjpeg-turbo::libjpeg-turbo)  # adjust to match built target name

if (${CMAKE_SYSTEM_NAME} MATCHES "Windows" AND (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" OR CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC") AND NOT MINGW)
    target_link_options(${TargetName} PUBLIC "/DELAYLOAD:turbojpeg.dll")
endif()