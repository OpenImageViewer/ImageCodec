#Codec JPG
cmake_minimum_required(VERSION 3.8)

file(GLOB_RECURSE sourceFiles
    "./Source/*.h"
    "./Source/*.cpp"
)

set(TargetName CodecJPG)
add_library (${TargetName} ${sourceFiles} )


set(libjpeg-turboFolder ../../External/libjpeg-turbo)
#Enable ASM provided by NASM

if (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
    set(CMAKE_ASM_NASM_COMPILER ${CMAKE_CURRENT_SOURCE_DIR}/../../External/bintools/nasm-2.15.05/nasm.exe)
endif()
#set(ENV{ASM_NASM} ${CMAKE_CURRENT_SOURCE_DIR}/../../External/bintools/nasm-2.15.05/nasm.exe)

#set(CMAKE_ASM_COMPILE_OBJECT "-I<CMAKE_CURRENT_SOURCE_DIR>/../../External/libjpeg-turbo/simd/nasm 
# -I<CMAKE_CURRENT_SOURCE_DIR>/../../External/libjpeg-turbo/simd/x86_64 <SOURCE>")



option(ENABLE_STATIC "Build static libraries" FALSE)
set(WITH_FUZZ FALSE)


add_subdirectory(${libjpeg-turboFolder} ./external/libjpeg-turbo)

target_include_directories(${TargetName} PRIVATE ${libjpeg-turboFolder})

target_link_libraries(${TargetName} turbojpeg)
if (${CMAKE_SYSTEM_NAME} MATCHES "Windows" AND  NOT MINGW)
    target_link_options(${TargetName} PUBLIC "/DELAYLOAD:$<TARGET_FILE_BASE_NAME:turbojpeg>.dll")
else()
endif()