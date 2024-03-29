cmake_minimum_required (VERSION 3.8)

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
    target_link_libraries(${TargetName} PRIVATE delayimp)
endif()


#Copy oiv library resources to the output dir 
     add_custom_command(TARGET ${TargetName}
     COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/cat.jpg ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/)

#Copy needed dependencies to output dir 
    add_custom_command(TARGET ${TargetName} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:png_shared> ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/)

    add_custom_command(TARGET ${TargetName} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:zlib> ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/)
    
    add_custom_command(TARGET ${TargetName} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:turbojpeg> ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/)