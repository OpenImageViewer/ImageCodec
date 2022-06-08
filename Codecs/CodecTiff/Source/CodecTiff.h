#pragma once

#include <Image.h>

#include <tiffio.h>
#include <tiffio.hxx>
#include "TiffClientFunctions.h"
#include "TiffFile.h"
#include <LLUtils/Exception.h>

namespace IMCodec
{
    class CodecTiff : public IImagePlugin
    {
    private:
        PluginProperties mPluginProperties;
    public:

        CodecTiff() : mPluginProperties({ L"LibTiff image codec","tif;tiff" })
        {

        }

        PluginProperties& GetPluginProperties() override
        {
            return mPluginProperties;
        }

        TexelFormat GetTexelFormat(uint16_t sampleFormat, uint16 bitsPerSample) const
        {
            TexelFormat texelFormat = TexelFormat::UNKNOWN;
            switch (sampleFormat)
            {
            case SAMPLEFORMAT_IEEEFP:

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
                default:
                    LL_ERROR(LLUtils::Exception::ErrorCode::NotImplemented, "CodecTiff: unsupported floating point format.");
                }
                break;


            case SAMPLEFORMAT_UINT:
                switch (bitsPerSample)
                {
                case 1:
                    texelFormat = TexelFormat::I_X1;
                    break;
                case 8:
                    texelFormat = TexelFormat::I_X8;
                    break;
                case 16:
                    texelFormat = TexelFormat::I_X16;
                    break;
                case 24:
                    texelFormat = TexelFormat::I_R8_G8_B8;
                    break;
                case 32:
                    texelFormat = TexelFormat::I_R8_G8_B8_A8;
                    break;
                default:
                    LL_ERROR(LLUtils::Exception::ErrorCode::NotImplemented, "CodecTiff: unsupported integer format.");
                }
                break;
            case SAMPLEFORMAT_INT:

                switch (bitsPerSample)
                {
                case 8:
                    texelFormat = TexelFormat::S_X8;
                    break;
                case 16:
                    texelFormat = TexelFormat::S_X16;
                    break;
                default:
                    LL_ERROR(LLUtils::Exception::ErrorCode::NotImplemented, "CodecTiff: unsupported integer format.");
                }
                break;

            default:
                LL_ERROR(LLUtils::Exception::ErrorCode::NotImplemented, "CodecTiff: unsupported type format.");
            }

            return texelFormat;
        }
        //Base abstract methods

        ImageItemSharedPtr GetImage(TIFF* tiff)
        {
            auto imageItem = std::make_shared<ImageItem>();
            imageItem->itemType = ImageItemType::Image;

            uint16_t sampleFormat = 0;

            uint32 width, height;
            uint16 bitsPerSample;
            uint16 compression;
            uint16 photoMetric;
            uint16 samplesPerPixel;
            uint16_t orientation;
            TexelFormat texelFormatDecompressed = TexelFormat::UNKNOWN;
            uint32_t rowPitch;
            uint32 rowsPerStrip;
            uint16_t stripRowCount = 0;
            uint32 stripoffsets;
            uint16_t planarConfig;
            //field width specification can be found here: http://www.libtiff.org/man/TIFFGetField.3t.html

            TIFFGetField(tiff, TIFFTAG_BITSPERSAMPLE, &bitsPerSample);
            TIFFGetField(tiff, TIFFTAG_IMAGEWIDTH, &width);
            TIFFGetField(tiff, TIFFTAG_IMAGELENGTH, &height);
            TIFFGetField(tiff, TIFFTAG_COMPRESSION, &compression);
            TIFFGetField(tiff, TIFFTAG_PHOTOMETRIC, &photoMetric);
            TIFFGetField(tiff, TIFFTAG_SAMPLESPERPIXEL, &samplesPerPixel);
            TIFFGetFieldDefaulted(tiff, TIFFTAG_ORIENTATION, &orientation);

            uint32_t bytesPerSample = bitsPerSample / CHAR_BIT;

            //TODO: use this field to read extra sample info e.g. transparency channel
            //TIFFGetField(tiff, TIFFTAG_EXTRASAMPLES, &extra);

            /* Default sample format is unsigned integer.
               From tiff specification:
               Section 19: Data Sample Format
                This section describes a scheme for specifying data sample type information.
                TIFF implicitly types all data samples as unsigned integer values.Certain applications,
                however, require the ability to store image - related data in other formats
                such as floating point.This section presents a scheme for describing a variety of
                data sample formats.*/

            if (TIFFGetField(tiff, TIFFTAG_SAMPLEFORMAT, &sampleFormat) == 0)
                sampleFormat = SAMPLEFORMAT_UINT;

            TIFFGetField(tiff, TIFFTAG_ROWSPERSTRIP, &rowsPerStrip);
            TIFFGetField(tiff, TIFFTAG_STRIPROWCOUNTS, &stripRowCount);
            TIFFGetField(tiff, TIFFTAG_STRIPOFFSETS, &stripoffsets);
            TIFFGetField(tiff, TIFFTAG_PLANARCONFIG, &planarConfig);
            
         
            uint32_t numberOfStripts = TIFFNumberOfStrips(tiff);
            tmsize_t stripSize = TIFFStripSize(tiff);
            rowPitch = static_cast<uint32_t>(TIFFScanlineSize(tiff));

            imageItem->descriptor.texelFormatStorage = GetTexelFormat(sampleFormat, bitsPerSample);
            if (imageItem->descriptor.texelFormatStorage != TexelFormat::UNKNOWN)
            {
                switch (photoMetric)
                {
                case PHOTOMETRIC_RGB:
                    texelFormatDecompressed = TexelFormat::I_R8_G8_B8_A8;
                    //override target row pitch - always 32bpp
                    rowPitch = width * 4;
                    imageItem->data.Allocate(height * rowPitch);
                    TIFFReadRGBAImageOriented(tiff, width, height, reinterpret_cast<uint32*>(imageItem->data.data()), orientation);
                    break;
                case PHOTOMETRIC_YCBCR:
                case PHOTOMETRIC_SEPARATED: //CMYK.

                    LL_ERROR(LLUtils::Exception::ErrorCode::NotImplemented, "YCBCR and CYMK are not implemented, using RGBA as fallback");
                    texelFormatDecompressed = TexelFormat::I_R8_G8_B8_A8;
                    //Read RGBA as a fallback
                    //override target row pitch - always 32bpp
                    rowPitch = width * 4;
                    imageItem->data.Allocate(height * rowPitch);
                    TIFFReadRGBAImageOriented(tiff, width, height, reinterpret_cast<uint32*>(imageItem->data.data()), orientation);
                    imageItem->descriptor.texelFormatStorage = GetTexelFormat(sampleFormat, bitsPerSample);
                    break;
                case PHOTOMETRIC_MINISWHITE:
                case PHOTOMETRIC_MINISBLACK:
                {
                    texelFormatDecompressed = imageItem->descriptor.texelFormatStorage;
                    imageItem->data.Allocate(height * rowPitch);

                    uint8_t* currensPos = reinterpret_cast<uint8_t*>(imageItem->data.data());

                    if (stripSize != rowPitch * rowsPerStrip)
                        LL_EXCEPTION(LLUtils::Exception::ErrorCode::NotImplemented, "CodecTiff: unsupported strip size.");

                    for (uint32_t i = 0; i < numberOfStripts; i++)
                    {
                        TIFFReadEncodedStrip(tiff, i, currensPos, stripSize);
                        currensPos += rowPitch * rowsPerStrip;
                    }


                }
                break;


                default:
                    LL_ERROR(LLUtils::Exception::ErrorCode::NotImplemented, "CodecTiff: unsupported type format.");

                }


                imageItem->descriptor.width = width;
                imageItem->descriptor.height = height;
                imageItem->descriptor.texelFormatDecompressed = texelFormatDecompressed;
                imageItem->descriptor.rowPitchInBytes = rowPitch;



                // Two or more image channels are interleaved
                // Extract the first one, currently discard the others.
                // TODO: return extra channel as subimage.				
                if (samplesPerPixel > 1
                    && (photoMetric == PHOTOMETRIC_MINISWHITE || photoMetric == PHOTOMETRIC_MINISBLACK))
                {

                    if (photoMetric != PHOTOMETRIC_MINISWHITE && photoMetric != PHOTOMETRIC_MINISBLACK)
                        LL_ERROR(LLUtils::Exception::ErrorCode::NotImplemented, "Extracing channel for non grayscale image is yet to be implemented");


                    LLUtils::Buffer image(rowPitch / samplesPerPixel * height);
                    size_t destIndex = 0;

                    const size_t samplesPerSingleCHannel = rowPitch * height / bytesPerSample / samplesPerPixel;
                    for (size_t i = 0; i < samplesPerSingleCHannel; i++)
                    {
                        const size_t sourceIndex = i * samplesPerPixel * bytesPerSample;
                        memcpy(reinterpret_cast<uint8_t*>(image.data()) + destIndex, reinterpret_cast<uint8_t*>(imageItem->data.data()) + sourceIndex, bytesPerSample);
                        destIndex += bytesPerSample;
                    }
                    imageItem->data = std::move(image);
                    imageItem->descriptor.rowPitchInBytes /= samplesPerPixel;
                }
            }
            else
            {
                imageItem = nullptr;
            }
            return imageItem;
        }
        
        ImageResult LoadMemoryImageFile(const std::byte* buffer, std::size_t size,[[maybe_unused]] ImageLoadFlags loadFlags, [[maybe_unused]] ImageSharedPtr& out_image) override
        {
            using namespace std;

            ImageResult result = ImageResult::Fail;
            TiffFile tifFile(reinterpret_cast<const uint8_t*>(buffer), size);
            TIFF* tiff = tifFile.GetTiff();
            ImageItemSharedPtr firstImageItem;
            std::vector<ImageSharedPtr> subImages;
            if (tiff != nullptr)
            {
                firstImageItem = GetImage(tiff);
                int currentSubImage = 0;
                while (TIFFReadDirectory(tiff) > 0)
                {
                    if (currentSubImage == 0)
                    {
                        subImages.push_back(std::make_shared<Image>(firstImageItem, ImageItemType::Image));
                        currentSubImage++;
                    }

                    auto subImage = GetImage(tiff);
                    subImages.push_back(std::make_shared<Image>(subImage, ImageItemType::Image));
                    currentSubImage++;
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
