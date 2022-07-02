#pragma once

#include <IImagePlugin.h>
#include <IImageCodec.h>

namespace IMCodec
{
    class CodecIconFactory
    {
    public:
        static IImagePlugin* Create(IImageCodec* imageLoader);
    };
}

