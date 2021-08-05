#Codec WebP
cmake_minimum_required(VERSION 3.8 FATAL_ERROR)

# Add source to this project's executable.
file(GLOB_RECURSE sourceFiles
    "./Source/*.h"
    "./Source/*.cpp"
)

#override webp default settings.
option(WEBP_BUILD_ANIM_UTILS "Build animation utilities." OFF)
option(WEBP_BUILD_CWEBP "Build the cwebp command line tool." OFF)
option(WEBP_BUILD_DWEBP "Build the dwebp command line tool." OFF)
option(WEBP_BUILD_GIF2WEBP "Build the gif2webp conversion tool." OFF)
option(WEBP_BUILD_IMG2WEBP "Build the img2webp animation tool." OFF)
option(WEBP_BUILD_VWEBP "Build the vwebp viewer tool." OFF)
option(WEBP_BUILD_WEBPINFO "Build the webpinfo command line tool." OFF)
option(WEBP_BUILD_LIBWEBPMUX "Build the libwebpmux library." OFF)
option(WEBP_BUILD_WEBPMUX "Build the webpmux command line tool." OFF)
option(WEBP_BUILD_EXTRAS "Build extras." OFF)
option(WEBP_BUILD_WEBP_JS "Emscripten build of webp.js." OFF)
option(WEBP_USE_THREAD "Enable threading support" ON)
option(WEBP_NEAR_LOSSLESS "Enable near-lossless encoding" ON)
option(WEBP_ENABLE_SWAP_16BIT_CSP "Enable byte swap for 16 bit colorspaces." OFF)

set(TargetName CodecWebP)
add_library (${TargetName} ${sourceFiles} )
add_subdirectory(D:/DevelopmentExternal/libwebp  ./external/libwebp)
target_include_directories(${TargetName} PRIVATE D:/DevelopmentExternal/libwebp)
target_include_directories(${TargetName} PRIVATE D:/DevelopmentExternal/libwebp/src)

target_link_libraries(${TargetName} 
webp
webpdecoder
webpdemux
)
