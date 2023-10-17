#pragma once

#include <Image.h>
#include <ImageLoader.h>

namespace IMCodec
{
#pragma pack (push,1)
    struct IcoDir
    {
        uint16_t reserved;
        uint16_t type;
        uint16_t numImages;
    };

    struct IcoDirEntry
    {
        uint8_t width;    // 0	1B	Specifies image width in pixels.Can be any number between 0 and 255. Value 0 means image width is 256 pixels.
        uint8_t height;    // 1	1B	Specifies image height in pixels.Can be any number between 0 and 255. Value 0 means image height is 256 pixels.
        uint8_t numColors;    // 2	1B	Specifies number of colors in the color palette.Should be 0 if the image does not use a color palette.
        uint8_t reserved;    // 3	1B	Reserved.Should be 0.[Notes 2]
        uint16_t colorPlanesOrHorizontalCoordinatesl;   // 4	2B	In ICO format : Specifies color planes.Should be 0 or 1.[Notes 3]
                                                             // In CUR format : Specifies the horizontal coordinates of the hotspot in number of pixels from the left.
        uint16_t bppOrVerticalCoordinates;              // 6	2B	In ICO format : Specifies bits per pixel.[Notes 4]
                                                        // In CUR format : Specifies the vertical coordinates of the hotspot in number of pixels from the top.
        uint32_t imageDataSize; // 8	4B	Specifies the size of the image's data in bytes
        uint32_t offsetData;    // 12	4B	Specifies the offset of BMP or PNG data from the beginning of the ICO / CUR file
    };

    struct IconFile
    {
        IcoDir icoDir;
        IcoDirEntry entry[1];
    };

    struct BitmapInfoHeader
    {
        uint32_t      biSize;
        int32_t       biWidth;
        int32_t       biHeight;
        uint16_t      biPlanes;
        uint16_t      biBitCount;
        uint32_t      biCompression;
        uint32_t      biSizeImage;
        int32_t       biXPelsPerMeter;
        int32_t       biYPelsPerMeter;
        uint32_t      biClrUsed;
        uint32_t      biClrImportant;
    };


#pragma pack(pop)

    class CodecIcon : public IImagePlugin
    {
    private:
            PluginProperties mPluginProperties;
            static constexpr uint8_t MaskBitCount = 1;
    public:
        inline thread_local static bool sIsLoading = false;

        CodecIcon(IImageCodec* imageLoader) : mPluginProperties(
            { 
                // {1599FC40-58DF-4950-A49B-2880E728CE00}

                { 0x1599fc40, 0x58df, 0x4950, { 0xa4, 0x9b, 0x28, 0x80, 0xe7, 0x28, 0xce, 0x0 } }

                 ,CodecCapabilities::Decode
                , LLUTILS_TEXT("Icon/Cursor Codec")
                ,
                {
                    {
                        { LLUTILS_TEXT("Windows Icon File")}
                            ,{ LLUTILS_TEXT("ico"),LLUTILS_TEXT("icon")}
                    }
                    ,
                    {
                        {LLUTILS_TEXT("Windows Cursor File")}
                            ,{ LLUTILS_TEXT("cur")}
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

        uint8_t GetValue(uint8_t bitWidth, const uint8_t* address, size_t position)
        {
            const size_t mask = (1 << bitWidth) - 1;
            const size_t PixelsInOneByte = CHAR_BIT / bitWidth;
            const size_t byteOffset = position / PixelsInOneByte;
            const uint8_t bitOffset = static_cast<uint8_t>((CHAR_BIT - ((position % PixelsInOneByte) + 1) * bitWidth));
            const uint8_t currentByte = address[byteOffset];
            uint8_t value = ((currentByte & (mask << bitOffset))) >> bitOffset;
            return value;
        }

        ImageResult Decode(const std::byte* buffer, std::size_t size, [[maybe_unused]] ImageLoadFlags loadFlags, const Parameters& params, ImageSharedPtr& out_image) override
        {
            using namespace std;
            using namespace IMCodec;
            ImageResult result = ImageResult::UnknownError;
            try
            {  
                if (size > sizeof(IcoDir))
                {
                    const IconFile* icoFile = reinterpret_cast<const IconFile*>(buffer);
                    const uint8_t* baseAddress = reinterpret_cast<const uint8_t*>(icoFile);
                    
                    if (icoFile->icoDir.reserved != 0 ||  (icoFile->icoDir.type != 1 && icoFile->icoDir.type != 2))
                        return ImageResult::UnknownError; // Not an Ico file 

                    
                    const uint16_t numImages = icoFile->icoDir.numImages;
                    const bool isMultiImage = numImages > 1;
                    bool error = false;

                    if (isMultiImage)
                    {
                        auto imageItem = std::make_shared<ImageItem>();
                        imageItem->itemType = ImageItemType::Container;
                        out_image = std::make_shared<Image>(imageItem, ImageItemType::Image);
                        out_image->SetNumSubImages(numImages);
                    }
 
                    for (uint32_t i = 0; i < numImages; i++)
                    {
                        ImageItemSharedPtr imageItem = std::make_shared<ImageItem>();
                        imageItem->itemType = ImageItemType::Image;
                 
                        auto& currentDescriptor = imageItem->descriptor;
                        const IcoDirEntry* currentEntry = (&icoFile->entry)[i];
                        const BitmapInfoHeader* bitmapInfo = reinterpret_cast<const BitmapInfoHeader*>(baseAddress + currentEntry->offsetData);
                        if (bitmapInfo->biSize == 40)
                        {
                            if (bitmapInfo->biCompression != 0)
                                LL_EXCEPTION_NOT_IMPLEMENT("Bitmap icons currently support only uncompressed images ");

                            currentDescriptor.width = currentEntry->width != 0 ? currentEntry->width : 256;
                            currentDescriptor.height = currentEntry->height != 0 ? currentEntry->height : 256;;
                            currentDescriptor.rowPitchInBytes = bitmapInfo->biBitCount * currentDescriptor.width / CHAR_BIT ;
                            
                            //Convert images to 32 bit to add transparency channel using the image mask.
                            currentDescriptor.texelFormatStorage = IMCodec::TexelFormat::I_B8_G8_R8_A8;
                            currentDescriptor.texelFormatDecompressed = IMCodec::TexelFormat::I_B8_G8_R8_A8;
                            currentDescriptor.rowPitchInBytes = 32 * currentDescriptor.width / CHAR_BIT;
                            imageItem->data.Allocate(currentDescriptor.width * currentDescriptor.height * 4);

                            const uint8_t* baseSourceAddress = reinterpret_cast<const uint8_t*>(bitmapInfo + 1);
                            const size_t sourceRowPitch = ((bitmapInfo->biBitCount * currentDescriptor.width + 31) & ~31) >> 3;
                            const size_t sourceRowPitchMask = (((MaskBitCount * currentDescriptor.width) + 31) & ~31) >> 3;
                            const size_t masktartOffset = currentDescriptor.height * sourceRowPitch;

                            switch (bitmapInfo->biBitCount)
                            {
                            case 1:
                            case 4:
                            case 8:
                            {
                                const uint32_t* colorTable = reinterpret_cast<const uint32_t*>(baseSourceAddress);
                                baseSourceAddress += (bitmapInfo->biClrUsed == 0 ? (1 << bitmapInfo->biBitCount) : bitmapInfo->biClrUsed) * sizeof(uint32_t);

                                if (bitmapInfo->biHeight != currentEntry->height * 2)
                                {
                                    LL_EXCEPTION(LLUtils::Exception::ErrorCode::InvalidState, "Could not find mask data");
                                }

                                for (size_t line = 0; line < currentDescriptor.height; line++)
                                {
                                    auto sourceLineOffset = (currentDescriptor.height - line - 1) * sourceRowPitch;
                                    auto SourceLineMaskOffset = masktartOffset + (currentDescriptor.height - line - 1) * sourceRowPitchMask;
                                    auto destLineOffset = line * currentDescriptor.rowPitchInBytes;

                                    for (size_t x = 0; x < currentDescriptor.width; x++)
                                    {
                                        uint8_t pixelIndex = GetValue(static_cast<uint8_t>(bitmapInfo->biBitCount), baseSourceAddress + sourceLineOffset, x);
                                        uint8_t opacity = GetValue(MaskBitCount, baseSourceAddress + SourceLineMaskOffset, x);
                                        uint32_t color = ((opacity == 1 ? 0x00 : 0xFF) << 24) | colorTable[pixelIndex];
                                        uint32_t* currentpixel = reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(imageItem->data.data()) + destLineOffset) + x;
                                        *currentpixel = color;
                                    }
                                }

                            }
                                break;
                            case 24:
                                if (bitmapInfo->biHeight != currentEntry->height * 2)
                                {
                                    LL_EXCEPTION(LLUtils::Exception::ErrorCode::InvalidState, "Could not find mask data");
                                }

                                for (size_t line = 0; line < currentDescriptor.height; line++)
                                {
                                    auto sourceLineOffset = (currentDescriptor.height - line - 1) * sourceRowPitch;
                                    auto SourceLineMaskOffset = masktartOffset + (currentDescriptor.height - line - 1) * sourceRowPitchMask;
                                    auto destLineOffset = line * currentDescriptor.rowPitchInBytes;

                                    for (size_t x = 0; x < currentDescriptor.width; x++)
                                    {
#pragma pack(push,1)
                                        struct Color24
                                        {
                                            uint8_t B;
                                            uint8_t G;
                                            uint8_t R;
                                            operator int() const
                                            {
                                                return (R << 16) | (G << 8) | B;
                                            }
                                        };
#pragma pack(pop)

                                        Color24 color24 =  *reinterpret_cast<const Color24*>(baseSourceAddress + sourceLineOffset + x * sizeof(Color24));
                                        uint8_t opacity = GetValue(MaskBitCount, baseSourceAddress + SourceLineMaskOffset, x);
                                        uint32_t color = ((opacity == 1 ? 0x00 : 0xFF) << 24) | color24;
                                        uint32_t* currentpixel = reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(imageItem->data.data()) + destLineOffset) + x;
                                        *currentpixel = color;
                                    }
                                }


                                break;
                            case 32:
                                for (size_t line = 0; line < currentDescriptor.height; line++)
                                {
                                    auto sourceOffset = (currentDescriptor.height - line - 1) * currentDescriptor.rowPitchInBytes;
                                    auto destoffset = line * currentDescriptor.rowPitchInBytes;
                                    imageItem->data.Write(reinterpret_cast<const std::byte*>(baseSourceAddress + sourceOffset), destoffset, currentDescriptor.rowPitchInBytes);
                                }
                                break;


                            default:
                                LL_EXCEPTION_NOT_IMPLEMENT("Bit depth is currently unsupported");
                            }
                            
                        }
                        else if (sIsLoading == false)
                        {
                            // if a PNG icon
                            ImageSharedPtr pngImage;
                            sIsLoading = true;
                            static thread_local ImageLoader helper;
                            ImageResult pngRes = helper.Decode(reinterpret_cast<const std::byte*>(baseAddress + currentEntry->offsetData), currentEntry->imageDataSize
                                , ImageLoadFlags::None, {}, LLUTILS_TEXT("png"), PluginTraverseMode::AnyPlugin, pngImage);

                            if (pngRes == ImageResult::Success)
                            {
                                imageItem = pngImage->GetImageItem();
                            }
                            else
                            {
                                error = true;
                                break;
                            }
                            sIsLoading = false;
                            
                        }
                        else
                        {
                            error = true; // recursive call
                        }

                        if (!error)
                        {
                            if (isMultiImage)
                            {
                                out_image->SetSubImage(i, std::make_shared<Image>(imageItem, ImageItemType::Pages));
                            }
                            else
                            {
                                out_image = std::make_shared<Image>(imageItem, ImageItemType::Unknown);
                            }
                        }
                   
                    }
                    result = error ? ImageResult::UnknownError : ImageResult::Success;
                }
            }
            catch (...)
            {
                sIsLoading = false;
                result = ImageResult::UnknownError;
            }

            
            
            return result;
        }
    };
}