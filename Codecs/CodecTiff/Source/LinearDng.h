#pragma once
#include <Image.h>
#include <tiffio.h>

namespace IMCodec
{
    // Decode the supported RGB LinearRaw subset without developing its stored sample values.
    // Unsupported calibration or storage returns FormatNotSupported for the existing raw fallback.
    ImageResult DecodeLinearDng(TIFF* tiff, ImageSharedPtr& output);
}  // namespace IMCodec
