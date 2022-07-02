#include "../Include/CodecIconFactory.h"
#include "CodecIcon.h"

namespace IMCodec
{
    IImagePlugin* CodecIconFactory::Create(IImageCodec* imageLoader)
    {
        return new CodecIcon(imageLoader);
    }
}