#pragma once
#include "Image.h"
#include "IImageCodec.h"
#include "IImageMetaDataLoader.h"

namespace IMCodec
{
    class ImageMetaDataLoader //: public IImageMetaDataLoader
    {
    public:
        ImageResult LoadMetaData(const std::byte* buffer, size_t size,ItemMetaDataSharedPtr& out_metaData) ;
    };
}