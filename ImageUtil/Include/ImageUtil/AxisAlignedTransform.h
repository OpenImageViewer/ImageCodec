#pragma once
#include <LLUtils/EnumClassBitwise.h>

namespace IMUtil
{
    enum AxisAlignedRotation
    {
          None          = 0
        , Rotate90CW    = 1
        , Rotate180     = 2
        , Rotate90CCW   = 3
    };

    enum class AxisAlignedFlip
    {
          None          = 0 << 0
        , Horizontal    = 1 << 0
        , Vertical      = 1 << 1
    };


    struct AxisAlignedTransform
    {
        AxisAlignedRotation rotation;
        AxisAlignedFlip flip;
    };
}

LLUTILS_DEFINE_ENUM_CLASS_FLAG_OPERATIONS(IMUtil::AxisAlignedFlip)
