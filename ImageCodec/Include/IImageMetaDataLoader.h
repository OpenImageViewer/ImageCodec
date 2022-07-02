#pragma once
#include <cstddef>
#include "IImagePlugin.h"

namespace IMCodec
{
    class IImageMetaDataLoader
    {
    public:
        // Decode memory buffer into ImageSharedPtr
        virtual ImageResult LoadMetaData(const std::byte* buffer
            , size_t size
            , ItemMetaDataSharedPtr out_metaData) = 0;

    };
}