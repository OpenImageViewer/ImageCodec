#include "../Include/CodecBMPFactory.h"
#include "CodecBMP.h"

namespace IMCodec
{
    IImagePlugin* CodecBMPFactory::Create()
    {
        return new CodecBMP();
    }
}
