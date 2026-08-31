#pragma once

#include <cstdint>

void BlockDecompressImageDXT1(std::uint32_t width, std::uint32_t height, const unsigned char* blockStorage,
                              std::uint32_t* image);
void BlockDecompressImageDXT5(std::uint32_t width, std::uint32_t height, const unsigned char* blockStorage,
                              std::uint32_t* image);
