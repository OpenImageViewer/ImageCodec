# Codec JPG
cmake_minimum_required(VERSION 3.10)

include(ExternalProject)

set(LibJpegTurboFolder "${CMAKE_CURRENT_SOURCE_DIR}/../../External/libjpeg-turbo")
get_filename_component(LibJpegTurboFolder "${LibJpegTurboFolder}" ABSOLUTE)
set(NASMPATH "${CMAKE_CURRENT_SOURCE_DIR}/../../External/bintools/nasm-2.15.05/nasm.exe")
get_filename_component(NASMPATH "${NASMPATH}" ABSOLUTE)

# ======== Platform-specific file names ========
set(JPG_OUTPUT_NAME turbojpeg)
set(JPG_RUNTIME_NAME "${CMAKE_SHARED_LIBRARY_PREFIX}${JPG_OUTPUT_NAME}${CMAKE_SHARED_LIBRARY_SUFFIX}")

if(WIN32)
    set(JPG_IMPLIB_NAME "${CMAKE_IMPORT_LIBRARY_PREFIX}${JPG_OUTPUT_NAME}${CMAKE_IMPORT_LIBRARY_SUFFIX}")
endif()

# ======== External build directory ========
set(TurboJpegBinBaseDir "${CMAKE_BINARY_DIR}/libjpeg-turbo")

# ======== Output directories (fall back to external build dir if not set) ========
if(CMAKE_RUNTIME_OUTPUT_DIRECTORY)
    set(JPG_RUNTIME_OUTPUT_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
else()
    set(JPG_RUNTIME_OUTPUT_DIR "${TurboJpegBinBaseDir}")
endif()

if(CMAKE_LIBRARY_OUTPUT_DIRECTORY)
    set(JPG_LIBRARY_OUTPUT_DIR "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
else()
    set(JPG_LIBRARY_OUTPUT_DIR "${TurboJpegBinBaseDir}")
endif()

if(CMAKE_ARCHIVE_OUTPUT_DIRECTORY)
    set(JPG_ARCHIVE_OUTPUT_DIR "${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}")
elseif(WIN32)
    set(JPG_ARCHIVE_OUTPUT_DIR "${JPG_RUNTIME_OUTPUT_DIR}")
else()
    set(JPG_ARCHIVE_OUTPUT_DIR "${TurboJpegBinBaseDir}")
endif()

if(WIN32)
    set(JPG_IMPORTED_LOCATION_DIR "${JPG_RUNTIME_OUTPUT_DIR}")
else()
    set(JPG_IMPORTED_LOCATION_DIR "${JPG_LIBRARY_OUTPUT_DIR}")
endif()

# Resolve a base output directory to its config-specific subdirectory.
# Substitutes known config placeholders; if none are present, appends the config
# name as a subdirectory (guarded to avoid doubling, e.g. .../Debug/Debug).
function(jpg_resolve_config_dir result base_dir config)
    set(path "${base_dir}")
    if("${path}" MATCHES "\\$<CONFIG>|\\$\\(Configuration\\)|\\$\\{CMAKE_CFG_INTDIR\\}")
        string(REPLACE "$<CONFIG>" "${config}" path "${path}")
        string(REPLACE "$(Configuration)" "${config}" path "${path}")
        string(REPLACE "\${CMAKE_CFG_INTDIR}" "${config}" path "${path}")
    elseif(NOT "${path}" MATCHES "(^|[\\\\/])${config}([\\\\/]|$)")
        set(path "${path}/${config}")
    endif()
    set(${result} "${path}" PARENT_SCOPE)
endfunction()

# ======== CMake arguments for libjpeg-turbo ========
set(LibJpegTurboArgs
    "-DENABLE_STATIC=FALSE"
    "-DWITH_FUZZ=FALSE"
    "-DWITH_JPEG=FALSE"
    "-DCMAKE_INSTALL_PREFIX=${TurboJpegBinBaseDir}/install"
)

# ======== Declare Imported Library ========
add_library(libjpeg-turbo::libjpeg-turbo SHARED IMPORTED GLOBAL)
set_target_properties(libjpeg-turbo::libjpeg-turbo PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${LibJpegTurboFolder}/src"
)

set(JPG_BYPRODUCTS "")

get_property(is_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(is_multi_config)
    # Multi-config (e.g. MSVC, Ninja Multi-Config): one pass per config sets both
    # the ExternalProject output dirs and the imported target locations.
    foreach(cfg ${CMAKE_CONFIGURATION_TYPES})
        string(TOUPPER "${cfg}" cfg_upper)

        jpg_resolve_config_dir(cfg_runtime "${JPG_RUNTIME_OUTPUT_DIR}" "${cfg}")
        jpg_resolve_config_dir(cfg_library "${JPG_LIBRARY_OUTPUT_DIR}" "${cfg}")
        jpg_resolve_config_dir(cfg_archive "${JPG_ARCHIVE_OUTPUT_DIR}" "${cfg}")

        list(APPEND LibJpegTurboArgs
            "-DCMAKE_RUNTIME_OUTPUT_DIRECTORY_${cfg_upper}=${cfg_runtime}"
            "-DCMAKE_LIBRARY_OUTPUT_DIRECTORY_${cfg_upper}=${cfg_library}"
            "-DCMAKE_ARCHIVE_OUTPUT_DIRECTORY_${cfg_upper}=${cfg_archive}"
        )

        jpg_resolve_config_dir(cfg_import_loc_dir "${JPG_IMPORTED_LOCATION_DIR}" "${cfg}")
        set(cfg_runtime_lib "${cfg_import_loc_dir}/${JPG_RUNTIME_NAME}")
        list(APPEND JPG_BYPRODUCTS "${cfg_runtime_lib}")
        set_target_properties(libjpeg-turbo::libjpeg-turbo PROPERTIES
            "IMPORTED_LOCATION_${cfg_upper}" "${cfg_runtime_lib}"
        )

        if(WIN32)
            set(cfg_implib "${cfg_archive}/${JPG_IMPLIB_NAME}")
            list(APPEND JPG_BYPRODUCTS "${cfg_implib}")
            set_target_properties(libjpeg-turbo::libjpeg-turbo PROPERTIES
                "IMPORTED_IMPLIB_${cfg_upper}" "${cfg_implib}"
            )
        endif()
    endforeach()
else()
    # Single-config (e.g. Ninja, Make): flat output directory.
    list(APPEND LibJpegTurboArgs
        "-DCMAKE_RUNTIME_OUTPUT_DIRECTORY=${JPG_RUNTIME_OUTPUT_DIR}"
        "-DCMAKE_LIBRARY_OUTPUT_DIRECTORY=${JPG_LIBRARY_OUTPUT_DIR}"
        "-DCMAKE_ARCHIVE_OUTPUT_DIRECTORY=${JPG_ARCHIVE_OUTPUT_DIR}"
    )
    if(CMAKE_BUILD_TYPE)
        list(APPEND LibJpegTurboArgs "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}")
    endif()

    set(imported_location "${JPG_IMPORTED_LOCATION_DIR}/${JPG_RUNTIME_NAME}")
    list(APPEND JPG_BYPRODUCTS "${imported_location}")
    set_target_properties(libjpeg-turbo::libjpeg-turbo PROPERTIES
        IMPORTED_LOCATION "${imported_location}"
    )

    if(WIN32)
        set(imported_implib "${JPG_ARCHIVE_OUTPUT_DIR}/${JPG_IMPLIB_NAME}")
        list(APPEND JPG_BYPRODUCTS "${imported_implib}")
        set_target_properties(libjpeg-turbo::libjpeg-turbo PROPERTIES
            IMPORTED_IMPLIB "${imported_implib}"
        )
    endif()
endif()

foreach(cmake_arg CMAKE_MT CMAKE_C_COMPILER CMAKE_CXX_COMPILER CMAKE_MAKE_PROGRAM CMAKE_RC_COMPILER)
    if(DEFINED ${cmake_arg} AND NOT "${${cmake_arg}}" STREQUAL "")
        list(APPEND LibJpegTurboArgs "-D${cmake_arg}=${${cmake_arg}}")
    endif()
endforeach()

if(CMAKE_TOOLCHAIN_FILE)
    get_filename_component(CMAKE_TOOLCHAIN_FILE_ABSOLUTE "${CMAKE_TOOLCHAIN_FILE}" ABSOLUTE)
    list(APPEND LibJpegTurboArgs "-DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE_ABSOLUTE}")
endif()

# Enable NASM on Windows when building on Windows.
if(WIN32 AND NOT CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux" AND EXISTS "${NASMPATH}")
    list(APPEND LibJpegTurboArgs "-DCMAKE_ASM_NASM_COMPILER=${NASMPATH}")
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

add_dependencies(libjpeg-turbo::libjpeg-turbo libjpeg-turbo)

file(GLOB_RECURSE sourceFiles
    "./Source/*.h"
    "./Source/*.cpp"
)

set(TargetName CodecJPG)
add_library(${TargetName} ${sourceFiles})

target_link_libraries(${TargetName} PRIVATE libjpeg-turbo::libjpeg-turbo)
