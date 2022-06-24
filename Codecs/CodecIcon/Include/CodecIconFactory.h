#pragma once

#include <IImagePlugin.h>

namespace IMCodec
{
    class CodecIconFactory
    {
    public:
        static IImagePlugin* Create(IImageCodec* imageLoader);
    };
}

