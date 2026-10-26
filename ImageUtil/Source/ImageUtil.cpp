#include <ImageUtil/ImageUtil.h>
#include <ImageUtil/TexelConvertor.h>
#include <ExoticNumbers/Float24.h>
#include <ExoticNumbers/half.hpp>
#include <algorithm>

namespace IMUtil
{
    TexelConvertor texelConvertor;
    ImageUtil::MapConvertKeyToFunc ImageUtil::sConvertionFunction{
        // Convert to RGBA
        {ConvertKey(IMCodec::TexelFormat::I_B8_G8_R8, IMCodec::TexelFormat::I_R8_G8_B8_A8), PixelUtil::BGR24ToRGBA32},
        {ConvertKey(IMCodec::TexelFormat::I_R8_G8_B8, IMCodec::TexelFormat::I_R8_G8_B8_A8), PixelUtil::RGB24ToRGBA32},
        {ConvertKey(IMCodec::TexelFormat::I_B8_G8_R8_A8, IMCodec::TexelFormat::I_R8_G8_B8_A8),
         PixelUtil::BGRA32ToRGBA32},
        {ConvertKey(IMCodec::TexelFormat::I_A8, IMCodec::TexelFormat::I_R8_G8_B8_A8), PixelUtil::A8ToRGBA32},
        {ConvertKey(IMCodec::TexelFormat::I_R8_G8_B8_A8, IMCodec::TexelFormat::I_B8_G8_R8_A8),
         PixelUtil::RGBA32ToBGRA32},
        {ConvertKey(IMCodec::TexelFormat::I_B8_G8_R8_A8, IMCodec::TexelFormat::I_B8_G8_R8), PixelUtil::BGRA32ToBGR24},
        {ConvertKey(IMCodec::TexelFormat::I_A8_R8_G8_B8, IMCodec::TexelFormat::I_R8_G8_B8_A8),
         PixelUtil::ARGB32ToRGBA32},
        {ConvertKey(IMCodec::TexelFormat::I_A8_B8_G8_R8, IMCodec::TexelFormat::I_R8_G8_B8_A8),
         PixelUtil::ABGR32ToRGBA32},
        {ConvertKey(IMCodec::TexelFormat::I_A8_B8_G8_R8, IMCodec::TexelFormat::I_B8_G8_R8_A8),
         PixelUtil::ABGR32ToBGRA32},
        {ConvertKey(IMCodec::TexelFormat::I_R16_G16_B16, IMCodec::TexelFormat::I_R8_G8_B8_A8),
         PixelUtil::RGB48ToBGRA32},
        {ConvertKey(IMCodec::TexelFormat::I_X1, IMCodec::TexelFormat::I_R8_G8_B8_A8), PixelUtil::A1ToRGBA32},
        {ConvertKey(IMCodec::TexelFormat::I_X1, IMCodec::TexelFormat::I_B8_G8_R8_A8), PixelUtil::A1ToRGBA32},
        {ConvertKey(IMCodec::TexelFormat::I_X16, IMCodec::TexelFormat::I_R8_G8_B8_A8), PixelUtil::A16ToRGBA32},
        {ConvertKey(IMCodec::TexelFormat::I_B5_G5_R5_X1, IMCodec::TexelFormat::I_R8_G8_B8_A8),
         PixelUtil::BGR16ToRGBA32},
        {ConvertKey(IMCodec::TexelFormat::I_B5_G6_R5, IMCodec::TexelFormat::I_R8_G8_B8_A8), PixelUtil::BGR565ToRGBA32},
        {ConvertKey(IMCodec::TexelFormat::F_R32_G32_B32, IMCodec::TexelFormat::I_R8_G8_B8_A8),
         PixelUtil::RGBF32ToRGBA32}

    };

    IMCodec::ImageSharedPtr ImageUtil::Convert(IMCodec::ImageSharedPtr sourceImage,
                                               IMCodec::TexelFormat targetPixelFormat)
    {
        using namespace IMCodec;

        ImageSharedPtr convertedImage;

        if (sourceImage->GetTexelFormat() != targetPixelFormat)
        {
            ImageItemSharedPtr imageItem = std::make_shared<ImageItem>();
            imageItem->descriptor        = sourceImage->GetDescriptor();
            // Convert only repacks or scales channels. Intensity normalization
            // creates its own image and must not inherit this source encoding.
            imageItem->colorInfo = sourceImage->GetImageItem()->colorInfo;
            imageItem->metaData  = sourceImage->GetImageItem()->metaData;
            const auto hasAlpha  = [](const TexelInfo& format)
            {
                return std::ranges::any_of(std::span(format.channles).first(format.numChannles),
                                           [](const ChannelInfo& channel)
                                           { return channel.semantic == ChannelSemantic::Opacity; });
            };
            if (!hasAlpha(GetTexelInfo(targetPixelFormat)) || !hasAlpha(sourceImage->GetTexelInfo()))
                imageItem->colorInfo.alphaMode = ImageItem::ColorInfo::AlphaMode::Opaque;

            // Size the destination through the same checked layout API used by image consumers.
            // A uint32_t pixels * bits intermediate wraps already at 512 MiB for RGBA/CMYK.
            imageItem->descriptor.texelFormatDecompressed = targetPixelFormat;
            const ImageDesc targetLayout(&imageItem->descriptor);
            const uint8_t targetPixelSizeInBits   = targetLayout.GetBitsPerTexel();
            const size_t targetSize               = targetLayout.GetTotalSizeOfImageTexels();
            imageItem->descriptor.rowPitchInBytes = targetLayout.GetBytesPerRowOfPixels();
            imageItem->data.Allocate(targetSize);
            if (imageItem->data.data() == nullptr)
                return nullptr;

            std::byte* dest = imageItem->data.data();
            // TODO: convert without normalization.
            ImageSharedPtr normalizedImage = sourceImage->GetIsRowPitchNormalized() == true
                                                 ? sourceImage
                                                 : NormalizePitch(sourceImage);

            bool succcess = false;
            // Try convert using the meta programmed swizzler
            if ((succcess = texelConvertor.Convert(targetPixelFormat,
                                                   sourceImage->GetDescriptor().texelFormatDecompressed, dest,
                                                   normalizedImage->GetBuffer(), normalizedImage->GetTotalPixels())) ==
                false)
            {
                // if couldn't convert using the above method, try a 'standard' conversion method
                auto converter = sConvertionFunction.find(ConvertKey(sourceImage->GetTexelFormat(), targetPixelFormat));

                if (converter != sConvertionFunction.end())
                {
                    PixelUtil::Convert(converter->second, &dest, normalizedImage->GetBuffer(), targetPixelSizeInBits,
                                       normalizedImage->GetTotalPixels());

                    succcess = true;
                }
            }

            if (succcess == true)
                convertedImage = std::make_shared<Image>(imageItem, sourceImage->GetSubImageGroupType());
        }
        else
        {  // No need to convert, return source image.
            convertedImage = sourceImage;
        }
        return convertedImage;
    }

    IMCodec::ImageSharedPtr ImageUtil::ConvertImageWithNormalization(IMCodec::ImageSharedPtr image,
                                                                     IMCodec::TexelFormat targetTexelFormat,
                                                                     bool isRainbow)
    {
        const NormalizeMode normalizeMode = isRainbow ? NormalizeMode::RainBow : NormalizeMode::GrayScale;

        switch (image->GetTexelFormat())
        {
            case IMCodec::TexelFormat::F_X16:
                image = Normalize<half_float::half>(image, normalizeMode);
                break;

            case IMCodec::TexelFormat::F_X24:
                image = Normalize<Float24>(image, normalizeMode);
                break;

            case IMCodec::TexelFormat::F_X32:
                image = ImageUtil::Normalize<float>(image, normalizeMode);
                break;

            case IMCodec::TexelFormat::I_A8:
            case IMCodec::TexelFormat::I_X8:
                image = ImageUtil::Normalize<uint8_t>(image, normalizeMode);
                break;

            case IMCodec::TexelFormat::S_X16:
                image = ImageUtil::Normalize<int16_t>(image, normalizeMode);
                break;
                /*case IMCodec::TexelFormat::F_R32_G32_B32:
                    image = ImageUtil::Normalize<int16_t>(image, normalizeMode);
                    break;*/

            default:
                image = ImageUtil::Convert(image, targetTexelFormat);
        }

        return image;
    }
}  // namespace IMUtil
