#Codec DDS
cmake_minimum_required(VERSION 3.10)

file(GLOB_RECURSE sourceFiles
    "./Source/*.h"
    "./Source/*.cpp"
)

set(TargetName ImageCodec)
add_library (${TargetName} ${sourceFiles} )

target_include_directories(${TargetName} PRIVATE ./Include)
target_include_directories(${TargetName} PRIVATE ../External/TinyExif)

if ( ${IMCODEC_BUILD_CODEC_BMP})
    target_link_libraries(${TargetName} CodecBMP)
endif()

if ( ${IMCODEC_BUILD_CODEC_DDS})
    target_link_libraries(${TargetName} CodecDDS)
endif()
if ( ${IMCODEC_BUILD_CODEC_ICON})
    target_link_libraries(${TargetName} CodecIcon)
endif()
if ( ${IMCODEC_BUILD_CODEC_PSD})
    target_link_libraries(${TargetName} CodecPSD)
endif()
if ( ${IMCODEC_BUILD_CODEC_JPG})
    target_link_libraries(${TargetName} CodecJPG)
endif()
if ( ${IMCODEC_BUILD_CODEC_PNG})
    target_link_libraries(${TargetName} CodecPNG)
endif()
if ( ${IMCODEC_BUILD_CODEC_GIF})
    target_link_libraries(${TargetName} CodecGif)
endif()
if ( ${IMCODEC_BUILD_CODEC_TIFF})
    target_link_libraries(${TargetName} CodecTiff)
endif()
if ( ${IMCODEC_BUILD_CODEC_WEBP})
    target_link_libraries(${TargetName} CodecWebP)
endif()

if ( ${IMCODEC_BUILD_CODEC_FREEIMAGE})
    target_link_libraries(${TargetName} CodecFreeImage)
endif()

target_link_libraries(${TargetName} TinyEXIFstatic)
