#include "../Include/CodecPNGFactory.h"
#include "CodecPng.h"

namespace IMCodec
{
    IImagePlugin* CodecPNGFactory::Create()
    {
        return new CodecPNG();
    }
}
