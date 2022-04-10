#pragma once
#include <LLUtils/EnumClassBitwise.h>
namespace IMCodec
{
    enum class ImageLoadFlags
    {
          None = 0 << 0   
        , Preview = 1 << 0
        , LoadOnlyFirstImage = 1 << 1
    };

    LLUTILS_DEFINE_ENUM_CLASS_FLAG_OPERATIONS(ImageLoadFlags)
}