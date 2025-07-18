cmake_minimum_required (VERSION 3.10)

#Image Codec
set(CMAKE_CXX_STANDARD 20)

file(GLOB_RECURSE sourceFiles
    "*.h"
    "*.cpp"
)

set (TargetName ImageLoaderExample)

add_executable(${TargetName} ${sourceFiles})


target_include_directories(${TargetName} PRIVATE ./Imageloader/Include)
target_link_libraries(${TargetName} PRIVATE ImageCodec)


if (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
    target_link_libraries(${TargetName} PRIVATE delayimp dbghelp)
endif()


set(OUTPUT_PATH $<TARGET_FILE_DIR:${TargetName}>)

#Copy oiv library resources to the output dir 
add_custom_command(TARGET ${TargetName} POST_BUILD
COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/cat.jpg  ${OUTPUT_PATH}/)

#copy zlib, libpng and libjpeg-turbo dlls to the output dir
add_custom_command(TARGET ${TargetName} POST_BUILD
COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:zlib> ${OUTPUT_PATH}/)

add_custom_command(TARGET ${TargetName} POST_BUILD
COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:png_shared> ${OUTPUT_PATH}/)

add_custom_command(TARGET ${TargetName} POST_BUILD
COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:libjpeg-turbo::libjpeg-turbo> ${OUTPUT_PATH}/)


