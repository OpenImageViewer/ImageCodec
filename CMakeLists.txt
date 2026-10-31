cmake_minimum_required (VERSION 3.10)
if(POLICY CMP0079)
	cmake_policy(SET CMP0079 NEW)
endif()
project(ImageCodec)

#Image Codec
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(IMCODEC_BUILD_TESTS_DEFAULT OFF)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
	set(IMCODEC_BUILD_TESTS_DEFAULT ON)
endif()
option(IMCODEC_BUILD_TESTS "Build ImageCodec tests" ${IMCODEC_BUILD_TESTS_DEFAULT})

if (${CMAKE_SYSTEM_NAME} STREQUAL "Windows" AND ${CMAKE_HOST_SYSTEM_NAME} STREQUAL "Linux")
	message("Cross compiling: Host: linux, Target: Windows")
	set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32)
	set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
	set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
	set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM BOTH)
	set(CMAKE_SYSTEM_PROCESSOR x86_64)
	set(CMAKE_SYSTEM_VERSION 10)
endif()


include_directories(./ImageCodec/Include)
add_subdirectory(./External/LLUtils)

option(IMCODEC_DISABLE_WARNINGS_EXTERNAL_LIBS "Disable warnings for external libraries" TRUE)


option(IMCODEC_BUILD_IMAGE_UTIL "Build Image util" TRUE)
option(IMCODEC_BUILD_CODEC_PSD "Build Codec PSD" TRUE)
option(IMCODEC_BUILD_CODEC_JPG "Build Codec JPG" TRUE)
option(IMCODEC_BUILD_CODEC_PNG "Build Codec PNG" TRUE)
option(IMCODEC_BUILD_CODEC_DDS "Build Codec DDS" TRUE)
option(IMCODEC_BUILD_CODEC_BMP "Build Codec BMP" TRUE)
option(IMCODEC_BUILD_CODEC_GIF "Build Codec GIF" TRUE)
option(IMCODEC_BUILD_CODEC_ICON "Build Codec Icon" TRUE)
option(IMCODEC_BUILD_CODEC_TIFF "Build Codec TIFF" TRUE)
option(IMCODEC_BUILD_CODEC_WEBP "Build Codec WEBP" TRUE)
option(IMCODEC_BUILD_CODEC_FREEIMAGE "Build Codec FREEIMAGE" TRUE)

option(IMCODEC_BUILD_EXAMPLES "Build Examples" TRUE)

if (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
	add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif()


if ( ${IMCODEC_BUILD_CODEC_PSD})
	add_definitions(-DIMCODEC_BUILD_CODEC_PSD=1)
endif()
if ( ${IMCODEC_BUILD_CODEC_JPG})
	add_definitions(-DIMCODEC_BUILD_CODEC_JPG=1)
endif()
if ( ${IMCODEC_BUILD_CODEC_PNG})
	add_definitions(-DIMCODEC_BUILD_CODEC_PNG=1)
endif()
if ( ${IMCODEC_BUILD_CODEC_DDS})
	add_definitions(-DIMCODEC_BUILD_CODEC_DDS=1)
endif()
if ( ${IMCODEC_BUILD_CODEC_ICON})
	add_definitions(-DIMCODEC_BUILD_CODEC_ICON=1)
endif()
if ( ${IMCODEC_BUILD_CODEC_GIF})
	add_definitions(-DIMCODEC_BUILD_CODEC_GIF=1)
endif()
if ( ${IMCODEC_BUILD_CODEC_TIFF})
	add_definitions(-DIMCODEC_BUILD_CODEC_TIFF=1)
endif()
if ( ${IMCODEC_BUILD_CODEC_WEBP})
	add_definitions(-DIMCODEC_BUILD_CODEC_WEBP=1)
endif()
if ( ${IMCODEC_BUILD_CODEC_BMP})
	add_definitions(-DIMCODEC_BUILD_CODEC_BMP=1)
endif()
if ( ${IMCODEC_BUILD_CODEC_FREEIMAGE})
	add_definitions(-DIMCODEC_BUILD_CODEC_FREEIMAGE=1)
endif()

if (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
	add_compile_definitions(NOMINMAX)
endif()


add_subdirectory(Codecs)
option(tinyxml2_BUILD_TESTING "Build tests for tinyxml2" OFF)
add_subdirectory(./External/tinyxml2)

option(TINY_EXIF_BUILD_DEMO "build demo binary" OFF)
add_subdirectory(./External/TinyEXIF)

add_subdirectory(ImageCodec)

if (IMCODEC_BUILD_IMAGE_UTIL)
	add_subdirectory(ImageUtil)
endif()


if (IMCODEC_BUILD_EXAMPLES AND IMCODEC_BUILD_CODEC_JPG AND IMCODEC_BUILD_CODEC_PNG)
  add_subdirectory(Example)
endif()

if(IMCODEC_BUILD_TESTS)
	enable_testing()
	add_subdirectory(Tests)
endif()

foreach(codecTarget IN ITEMS
	CodecBMP CodecDDS CodecFreeImage CodecGif CodecIcon CodecJPG CodecPNG CodecPSD CodecTiff CodecWebP)
	if(TARGET ${codecTarget})
		target_link_libraries(${codecTarget} PRIVATE LLUtils::LLUtils)
	endif()
endforeach()

target_link_libraries(ImageCodec PUBLIC LLUtils::LLUtils)
if(TARGET ImageUtil)
	target_link_libraries(ImageUtil PUBLIC LLUtils::LLUtils)
endif()
