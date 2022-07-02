#pragma once
#include <memory>
#include "TexelFormat.h"

namespace IMCodec
{
    struct ImageDescriptor
    {
        uint32_t width;
        uint32_t height;
        uint32_t rowPitchInBytes;
        TexelFormat texelFormatDecompressed; 
        TexelFormat texelFormatStorage;
    };
}