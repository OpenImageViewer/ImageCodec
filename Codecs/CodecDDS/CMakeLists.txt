# Codec DDS
cmake_minimum_required(VERSION 3.20)
project(CodecDDS)

add_subdirectory(../../External/libdds libdds)
add_library(CodecDDS Source/CodecDDSFactory.cpp Source/CodecDDS.h Include/CodecDDSFactory.h)
target_link_libraries(CodecDDS PRIVATE libdds::libdds)
