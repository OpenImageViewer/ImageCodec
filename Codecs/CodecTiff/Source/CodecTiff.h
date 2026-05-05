#pragma once

#include <Image.h>

#include <tiffio.h>
#include <tiffio.hxx>
#include "TiffClientFunctions.h"
#include "TiffFile.h"
#include <LLUtils/Exception.h>
#include <IImagePlugin.h>

#include <utility>

namespace IMCodec
{
    class CodecTiff : public IImagePlugin
    {
    private:
        PluginProperties mPluginProperties;
    public:

        CodecTiff() : mPluginProperties(
            {
                // {3AF4C3A1-FA72-4394-8B7C-68412A3433E3}
                { 0x3af4c3a1, 0xfa72, 0x4394, { 0x8b, 0x7c, 0x68, 0x41, 0x2a, 0x34, 0x33, 0xe3 } }
                , CodecCapabilities::Decode
                , LLUTILS_TEXT("LibTiff image codec")
                ,
                {
                    {
                        { LLUTILS_TEXT("Tag Image File Format")}
                        ,{ LLUTILS_TEXT("tif"),LLUTILS_TEXT("tiff")}
                    }
                }

            }
        )
        {

        }

        const PluginProperties& GetPluginProperties() override
        {
            return mPluginProperties;
        }

        TexelFormat GetTexelFormat(uint16_t sampleFormat, uint16_t bitsPerSample, uint16_t samplesPerPixel, uint16_t photoMetric) const
        {
            TexelFormat texelFormat = TexelFormat::UNKNOWN;

            switch (photoMetric)
            {
            case PHOTOMETRIC_RGB:
                switch (sampleFormat)
                {
                case SAMPLEFORMAT_IEEEFP:
                    switch (samplesPerPixel)
                    {
                    case 3:
                        switch (bitsPerSample)
                        {
                        case 32:
                            texelFormat = TexelFormat::F_R32_G32_B32;
                            break;
                        }
                    }
                    break;
                case SAMPLEFORMAT_UINT:
                    switch (samplesPerPixel)
                    {
                    case 3:
                        switch (bitsPerSample)
                        {
                        case 8:
                            texelFormat = TexelFormat::I_R8_G8_B8;
                            break;
                        case 16:
                            texelFormat = TexelFormat::I_R16_G16_B16;
                            break;
                        }
                        break;
                    case 4:
                        switch (bitsPerSample)
                        {
                        case 8:
                            texelFormat = TexelFormat::I_R8_G8_B8_A8;
                            break;
                        case 16:
                            texelFormat = TexelFormat::I_R16_G16_B16_A16;
                            break;
                        }
                        break;
                    }
                }
                break;
            case PHOTOMETRIC_MINISWHITE:
            case PHOTOMETRIC_MINISBLACK:
                switch (sampleFormat)
                {
                case SAMPLEFORMAT_IEEEFP:
                    switch (samplesPerPixel)
                    {
                    case 1:
                        switch (bitsPerSample)
                        {
                        case 16:
                            texelFormat = TexelFormat::F_X16;
                            break;
                        case 24:
                            texelFormat = TexelFormat::F_X24;
                            break;
                        case 32:
                            texelFormat = TexelFormat::F_X32;
                            break;
                        case 64:
                            texelFormat = TexelFormat::F_X64;
                            break;
                        default:
                            break;
                        }
                        break;
                    }
                    break;
                case SAMPLEFORMAT_UINT:
                    switch (samplesPerPixel)
                    {
                    case 1:
                        switch (bitsPerSample)
                        {
                        case 1:
                            texelFormat = TexelFormat::I_X1;
                            break;
                        case 4:
                            texelFormat = TexelFormat::I_X4;
                            break;
                        case 8:
                            texelFormat = TexelFormat::I_X8;
                            break;
                        case 16:
                            texelFormat = TexelFormat::I_X16;
                            break;
                        }
                        break;
                    }
                    break;
                case SAMPLEFORMAT_INT:
                    switch (samplesPerPixel)
                    {
                    case 1:
                        switch (bitsPerSample)
                        {
                        case 8:
                            texelFormat = TexelFormat::S_X8;
                            break;
                        case 16:
                            texelFormat = TexelFormat::S_X16;
                            break;
                        default:
                            break;
                        }
                        break;
                    }
                    break;
                }
            }

            return texelFormat;
        }

        std::vector<ImageItemSharedPtr> GetImage(TIFF* tiff)
        {
            std::vector< ImageItemSharedPtr> imageItems;
            imageItems.push_back(std::make_shared<ImageItem>());
            auto& imageItem = imageItems.back();
            imageItem->itemType = ImageItemType::Image;

            uint16_t sampleFormat{};

            uint32_t width, height{};
            uint16_t bitsPerSample{};
            //uint16 compression{};
            uint16_t photoMetric{};
            uint16_t samplesPerPixel{};
            uint16_t orientation{};
            uint32_t rowPitch{};
            //uint32 rowsPerStrip{};
            //uint16_t stripRowCount{};
            //uint32 stripoffsets{};
            uint16_t planarConfig{};
            /*uint16_t minSampleValue{};
            uint16_t maxSampleValue{};*/
            uint16_t extraSamples{};
            uint16_t* sampleTypes{};
            //field width specification can be found here: http://www.libtiff.org/man/TIFFGetField.3t.html

            TIFFGetField(tiff, TIFFTAG_BITSPERSAMPLE, &bitsPerSample);
            TIFFGetField(tiff, TIFFTAG_IMAGEWIDTH, &width);
            TIFFGetField(tiff, TIFFTAG_IMAGELENGTH, &height);
            //TIFFGetField(tiff, TIFFTAG_COMPRESSION, &compression);
            TIFFGetField(tiff, TIFFTAG_PHOTOMETRIC, &photoMetric);
            TIFFGetField(tiff, TIFFTAG_SAMPLESPERPIXEL, &samplesPerPixel);
            TIFFGetFieldDefaulted(tiff, TIFFTAG_ORIENTATION, &orientation);
            TIFFGetFieldDefaulted(tiff, TIFFTAG_SAMPLEFORMAT, &sampleFormat);

            //TIFFGetField(tiff, TIFFTAG_ROWSPERSTRIP, &rowsPerStrip);
            //TIFFGetField(tiff, TIFFTAG_STRIPROWCOUNTS, &stripRowCount);
            TIFFGetField(tiff, TIFFTAG_PLANARCONFIG, &planarConfig);
            /*TIFFGetField(tiff, TIFFTAG_MINSAMPLEVALUE, &minSampleValue);
            TIFFGetField(tiff, TIFFTAG_MAXSAMPLEVALUE, &maxSampleValue);*/
            TIFFGetField(tiff, TIFFTAG_EXTRASAMPLES, &extraSamples, &sampleTypes);

            if (samplesPerPixel <= extraSamples)
                return {};

            const uint32_t bytesPerSample = bitsPerSample / CHAR_BIT;

            uint32_t numberOfStripts = TIFFNumberOfStrips(tiff);
            tmsize_t stripSize = TIFFStripSize(tiff);
            rowPitch = static_cast<uint32_t>(TIFFScanlineSize(tiff));
            //auto rasterScanLineSize = TIFFRasterScanlineSize(tiff);
            
            const auto mainChannelTexelFormat = GetTexelFormat(sampleFormat, bitsPerSample, samplesPerPixel - extraSamples, photoMetric);

            imageItem->descriptor.width = width;
            imageItem->descriptor.height = height;
            imageItem->descriptor.rowPitchInBytes = rowPitch;

            if (mainChannelTexelFormat != TexelFormat::UNKNOWN)
            {
                imageItem->data.Allocate(numberOfStripts * stripSize);

                std::byte* currensPos = imageItem->data.data();

                for (uint32_t i = 0; i < numberOfStripts; i++)
                {
                    TIFFReadEncodedStrip(tiff, i, currensPos, stripSize);
                    currensPos += stripSize;
                }

                //If there's an alpha channel and it's interleaved in the image data, return as single image with alpha channel
                imageItem->descriptor.texelFormatDecompressed = imageItem->descriptor.texelFormatStorage = mainChannelTexelFormat;

                auto extractExtraSampleImages = [&]()
                {
                    return ExtactExtraSamples(width, height, rowPitch, samplesPerPixel, extraSamples, planarConfig
                        , bytesPerSample, imageItem->data, photoMetric, bitsPerSample, sampleFormat);
                };

                if (planarConfig == PLANARCONFIG_CONTIG && extraSamples > 0)
                {
                    const auto interleavedTexelFormat = GetTexelFormat(sampleFormat, bitsPerSample, samplesPerPixel, photoMetric);

                    if (interleavedTexelFormat != TexelFormat::UNKNOWN)
                    {
                        imageItem->descriptor.texelFormatStorage = imageItem->descriptor.texelFormatDecompressed = interleavedTexelFormat;
                    }
                    else
                    {
                        auto extraChannelImages = extractExtraSampleImages();

                        if (extraChannelImages.empty() == false)
                            imageItems = extraChannelImages;
                        else
                            return {};
                    }
                }
                else if (extraSamples > 0)
                {
                    auto extraChannelImages = extractExtraSampleImages();

                    if (extraChannelImages.empty() == false)
                        imageItems = extraChannelImages;
                    else
                        return {};
                }
            }
            else
            {
                imageItem->descriptor.texelFormatDecompressed = TexelFormat::I_R8_G8_B8_A8;
                //override target row pitch - always 32bpp
                imageItem->descriptor.rowPitchInBytes = width * 4;
                imageItem->data.Allocate(height * imageItem->descriptor.rowPitchInBytes);
                TIFFReadRGBAImageOriented(tiff, width, height, reinterpret_cast<uint32_t*>(imageItem->data.data()), orientation);
            }
            

            return imageItems;
        }
        
        std::vector<ImageItemSharedPtr> ExtactExtraSamples(
            uint32_t width, uint32_t height, uint32_t sourceRowPitch, 
            uint16_t samplesPerPixel, uint16_t extraSmaples
            , uint16_t planarConfig, uint16_t bytesPerSample
            , const LLUtils::Buffer& sourceBuffer
            , uint16_t photometric
            , uint16_t bitsPerSample
            , uint16_t sampleFormat
        )
        {

            std::vector< ImageItemSharedPtr> subChannels;


            auto extractSamples = [&](uint16_t sampleOffset, uint16_t sampleCount)->
                ImageItemSharedPtr
            {

                const auto sampleSize = sampleCount * bytesPerSample;
                const auto samplePhotometric = sampleOffset == 0 ? photometric : PHOTOMETRIC_MINISBLACK;

                auto imageItem = std::make_shared<ImageItem>();
                imageItem->descriptor.texelFormatStorage = GetTexelFormat(sampleFormat, bitsPerSample , sampleCount, samplePhotometric);
                if (imageItem->descriptor.texelFormatStorage == TexelFormat::UNKNOWN)
                    return {};

                imageItem->descriptor.texelFormatDecompressed = imageItem->descriptor.texelFormatStorage;
                imageItem->descriptor.height = height;
                imageItem->descriptor.width = width;

                LLUtils::Buffer image;

                if (planarConfig == PLANARCONFIG_CONTIG) //interleaved
                {
                    const auto singleSampleRowPitch = sampleSize * width;
                    image.Allocate(singleSampleRowPitch * height);
                    size_t sourceOffset = 0;
                    size_t destOffset = 0;
                    for (size_t y = 0; y < height; y++)
                    {
                        for (size_t x = 0; x < width; x++)
                        {
                            const size_t sourceRowIndex = sampleOffset * bytesPerSample + x * samplesPerPixel * bytesPerSample;
                            const size_t destRowIndex = x * sampleSize;
                            memcpy(image.data() + destOffset + destRowIndex, sourceBuffer.data() + sourceOffset + sourceRowIndex, sampleSize);
                        }

                        sourceOffset += sourceRowPitch;
                        destOffset += singleSampleRowPitch;
                    }
                    imageItem->descriptor.rowPitchInBytes = singleSampleRowPitch;;
                }
                else if (planarConfig == PLANARCONFIG_SEPARATE)
                {
                    const auto singleSampleRowPitch = sourceRowPitch * sampleCount;
                    image.Allocate(sourceRowPitch * height);
                    memcpy(image.data(), sourceBuffer.data() + height * singleSampleRowPitch * sampleOffset, height * singleSampleRowPitch);
                    imageItem->descriptor.rowPitchInBytes = singleSampleRowPitch;;
                }

                imageItem->data = std::move(image);

                return imageItem;
            };

            if (extraSmaples > 0)
            {
                if (bytesPerSample == 0 || bitsPerSample % CHAR_BIT != 0 || samplesPerPixel <= extraSmaples)
                    return {};

                const auto mainImageSampleCount = samplesPerPixel - extraSmaples;

                auto mainImage = extractSamples(0, mainImageSampleCount);
                if (mainImage == nullptr)
                    return {};

                subChannels.push_back(std::move(mainImage));

                for (int i = 0; i < extraSmaples; i++)
                {
                    auto extraImage = extractSamples(mainImageSampleCount + i, 1);
                    if (extraImage == nullptr)
                        return {};

                    subChannels.push_back(std::move(extraImage));
                }
            }


            return subChannels;
        }

        ImageResult Decode(const std::byte* buffer, std::size_t size,[[maybe_unused]] ImageLoadFlags loadFlags, const Parameters& params ,[[maybe_unused]] ImageSharedPtr& out_image) override
        {
            using namespace std;

            ImageResult result = ImageResult::UnknownError;
            TiffFile tifFile(reinterpret_cast<const uint8_t*>(buffer), size);
            TIFF* tiff = tifFile.GetTiff();
            ImageItemSharedPtr firstImageItem;
            std::vector<ImageSharedPtr> subImages;
            if (tiff != nullptr)
            {
                int currentSubImage = 0;

                auto firstImageItemChannles = GetImage(tiff);
                if (firstImageItemChannles.empty())
                    return ImageResult::FormatNotSupported;

                firstImageItem = firstImageItemChannles.at(0);

                if (firstImageItemChannles.size() > 1)
                {
                    for (size_t i = 0; i < firstImageItemChannles.size(); i++)
                    {
                        subImages.push_back(std::make_shared<Image>(firstImageItemChannles.at(i), ImageItemType::Pages));
                        currentSubImage++;
                    }
                }


                while (TIFFReadDirectory(tiff) > 0)
                {
                    if (currentSubImage == 0)
                    {
                        subImages.push_back(std::make_shared<Image>(firstImageItem, ImageItemType::Pages));
                        currentSubImage++;
                    }

                    auto nextImages = GetImage(tiff);
                    if (nextImages.empty())
                        return ImageResult::FormatNotSupported;

                    for (size_t i = 0; i < nextImages.size(); i++)
                    {
                        subImages.push_back(std::make_shared<Image>(nextImages.at(i), ImageItemType::Pages));
                        currentSubImage++;
                    }
                }

                if (currentSubImage > 0)
                {
                    ImageItemSharedPtr containerImageItem = std::make_shared<ImageItem>();
                    containerImageItem->itemType = ImageItemType::Container;
                    out_image = std::make_shared<Image>(containerImageItem, ImageItemType::Pages);
                    out_image->SetNumSubImages(currentSubImage);

                    for (auto subImage = 0; subImage < currentSubImage; subImage++)
                        out_image->SetSubImage(subImage, subImages.at(subImage));
                }
                else
                {
                    out_image = std::make_shared<Image>(firstImageItem, ImageItemType::Image);
                }
       
                result = ImageResult::Success;
                
            }
            return result;
        }
    };
}
