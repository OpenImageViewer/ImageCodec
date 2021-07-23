#include "../Include/CodecIconFactory.h"
#include "CodecIcon.h"

namespace IMCodec
{
    IImagePlugin* CodecIconFactory::Create(IImageLoader* imageLoader)
    {
        return new CodecIcon(imageLoader);
    }
}
