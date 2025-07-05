#Codec JPG
cmake_minimum_required(VERSION 3.10)

include(ExternalProject)

set(LibJpegTurboFolder ../../External/libjpeg-turbo)
get_filename_component(LibJpegTurboFolder "${LibJpegTurboFolder}" ABSOLUTE)
set(NASMPATH ${CMAKE_CURRENT_SOURCE_DIR}/../../External/bintools/nasm-2.15.05/nasm.exe)
get_filename_component(NASMPATH "${NASMPATH}" ABSOLUTE)
set(TurboJpegBinDir ${CMAKE_BINARY_DIR}/libjpeg-turbo)

set(LibJpegTurboArgs
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -DENABLE_STATIC=FALSE
        -DWITH_FUZZ=FALSE
        -DCMAKE_INSTALL_PREFIX=${TurboJpegBinDir}/install
        -DWITH_JPEG=FALSE # Disable JPEG codec, we only need TurboJPEG
)

#Enable ASM provided by NASM
if (${CMAKE_SYSTEM_NAME} MATCHES "Windows" AND NOT ${CMAKE_HOST_SYSTEM_NAME} STREQUAL "Linux")
    list(APPEND LibJpegTurboArgs -DCMAKE_ASM_NASM_COMPILER=${NASMPATH})
endif()

ExternalProject_Add(libjpeg-turbo
    SOURCE_DIR ${LibJpegTurboFolder} 
    BINARY_DIR ${TurboJpegBinDir}
    CMAKE_ARGS ${LibJpegTurboArgs}
    INSTALL_COMMAND ""  # <--- disables 'ninja install'
    BUILD_COMMAND ${CMAKE_COMMAND} --build . --target turbojpeg
    BUILD_BYPRODUCTS ${TurboJpegBinDir}/turbojpeg.dll
)

 file(GLOB_RECURSE sourceFiles
     "./Source/*.h"
     "./Source/*.cpp"
 )

set(TargetName CodecJPG)
add_library (${TargetName} ${sourceFiles} )

add_dependencies(${TargetName} libjpeg-turbo)
target_link_libraries(${TargetName} PRIVATE ${TurboJpegBinDir}/turbojpeg.lib)  # adjust to match built target name

add_custom_command(TARGET libjpeg-turbo POST_BUILD
          COMMAND ${CMAKE_COMMAND} -E copy_if_different ${TurboJpegBinDir}/turbojpeg.dll ${CMAKE_BINARY_DIR}/${CMAKE_CFG_INTDIR}/bin/)
target_include_directories(${TargetName} PRIVATE ${LibJpegTurboFolder}/src)

if (${CMAKE_SYSTEM_NAME} MATCHES "Windows" AND  NOT MINGW)
    target_link_options(${TargetName} PUBLIC "/DELAYLOAD:turbojpeg.dll")
else()
endif()