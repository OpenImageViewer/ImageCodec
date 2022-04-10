#pragma once
#include <memory>
#include "TexelFormat.h"

namespace IMCodec
{
    struct ImageDescriptor
    {
        uint32_t width = std::numeric_limits<decltype(width)>::max();
        uint32_t height = std::numeric_limits<decltype(height)>::max();
        uint32_t rowPitchInBytes = std::numeric_limits<decltype(rowPitchInBytes)>::max();
        TexelFormat texelFormatDecompressed = TexelFormat::UNKNOWN;
        TexelFormat texelFormatStorage = TexelFormat::UNKNOWN;

        bool IsValid() const
        {
            return !(false
                || texelFormatDecompressed == TexelFormat::UNKNOWN
                || texelFormatDecompressed == TexelFormat::UNKNOWN
                || width == std::numeric_limits<decltype(width)>::max()
                || height == std::numeric_limits<decltype(height)>::max()
                || rowPitchInBytes == std::numeric_limits<decltype(rowPitchInBytes)>::max()
                );
        }
    };
}