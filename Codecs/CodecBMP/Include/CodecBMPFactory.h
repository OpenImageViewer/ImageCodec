#pragma once

#include <IImagePlugin.h>

namespace IMCodec
{
    class CodecBMPFactory
    {
    public:
        static IImagePlugin* Create();
    };
}

