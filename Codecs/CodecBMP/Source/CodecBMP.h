#pragma once

#include <IImagePlugin.h>
#include <LLUtils/Utility.h>

namespace IMCodec
{

#pragma pack(push,1)
    struct BitmapFileHeader
    {
        uint16_t   bfType;
        uint32_t   bfSize;
        uint16_t   bfReserved1;
        uint16_t   bfReserved2;
        uint32_t   bfOffBits;
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

    enum class BitmapCompression
    {
        RGB = 0x0000,
        RLE8 = 0x0001,
        RLE4 = 0x0002,
        BitFields = 0x0003,
        JPEG = 0x0004,
        PNG = 0x0005,
        CMYK = 0x000B,
        CMYKRLE8 = 0x000C,
        CMYKRLE4 = 0x000D
    };

#pragma pack(pop)

    class CodecBMP : public IImagePlugin
    {
    private:
            PluginProperties mPluginProperties;
    public:
        inline thread_local static bool sIsLoading = false;

        CodecBMP() : mPluginProperties({ "Embedded BMP codec","bmp" , false})
        {
            
        }
        
        PluginProperties& GetPluginProperties() override
        {
            return mPluginProperties;
        }

        uint8_t GetValue(uint8_t bitWidth, const uint8_t* address, int position)
        {
            const int mask = (1 << bitWidth) - 1;
            const int PixelsInOneByte = CHAR_BIT / bitWidth;
            const int byteOffset = position / PixelsInOneByte;
            const int bitOffset = (CHAR_BIT - ((position % PixelsInOneByte) + 1) * bitWidth);
            const uint8_t currentByte = address[byteOffset];
            uint8_t value = ((currentByte & (mask << bitOffset))) >> bitOffset;
            return value;
        }

        virtual bool LoadImages([[maybe_unused]]  const uint8_t* buffer, [[maybe_unused]]  std::size_t size, [[maybe_unused]] std::vector<ImageDescriptor>& out_vec_properties) override
        {
            return false;
        }


        //Base abstract methods
        bool LoadImage(const uint8_t* buffer, std::size_t size, ImageDescriptor& out_properties) override
        {
            using namespace std;
            bool success = false;

            if (size > sizeof(BitmapFileHeader) + sizeof(BitmapInfoHeader))
            {
                const BitmapFileHeader& bmpFile = *reinterpret_cast<const BitmapFileHeader*>(buffer);
                if (bmpFile.bfType == 0x4D42) // "BM")
                {
                    const BitmapInfoHeader* bmpInfoPtr = reinterpret_cast<const BitmapInfoHeader*>(buffer + sizeof(BitmapFileHeader));
                    const BitmapInfoHeader& bmpInfo = *bmpInfoPtr;
                    const uint8_t* baseSourceAddress = reinterpret_cast<const uint8_t*>(bmpInfoPtr + 1);
                    BitmapCompression compression = static_cast<BitmapCompression>(bmpInfo.biCompression);
                    if (compression == BitmapCompression::BitFields)
                    {
                        //TODO: implement RGBMasks for 16 bit and 32 bit images.
                        [[maybe_unused]] const uint32_t* RGBMasks = reinterpret_cast<const uint32_t*>(baseSourceAddress);
                        baseSourceAddress += sizeof(uint32_t) * 4;
                    }

                    switch (compression)
                    {
                    case BitmapCompression::RGB:
                        // no compression.
                        break;
                    case BitmapCompression::BitFields:
                        //No compression but add add masks to color data.
                        break;
                    default:
                        LL_EXCEPTION_NOT_IMPLEMENT("Bitmap compression type is not supported");
                    }

                    out_properties.fProperties.Height = bmpInfo.biHeight;
                    out_properties.fProperties.Width = bmpInfo.biWidth;
                    out_properties.fProperties.NumSubImages = 0;
                    switch (bmpInfo.biBitCount)
                    {
                    case 1:
                        //Indexed color - return as RGBA
                        out_properties.fProperties.TexelFormatStorage = IMCodec::TexelFormat::I_X1;
                        out_properties.fProperties.TexelFormatDecompressed = IMCodec::TexelFormat::I_R8_G8_B8_A8;
                        break;
                    case 4:
                        //Indexed color - return as RGBA
                        out_properties.fProperties.TexelFormatStorage = IMCodec::TexelFormat::I_X4;
                        out_properties.fProperties.TexelFormatDecompressed = IMCodec::TexelFormat::I_R8_G8_B8_A8;
                        break;
                    case 8:
                        //Indexed color - return as RGBA
                        out_properties.fProperties.TexelFormatStorage = IMCodec::TexelFormat::I_X8;
                        out_properties.fProperties.TexelFormatDecompressed = IMCodec::TexelFormat::I_R8_G8_B8_A8;
                        break;
                    case 16:
                        //TODO: resolve texel format by analyzing masks instead assuming that masked image data
                        // is in the format B5G6R5
                        if (bmpInfo.biCompression == 3)
                        {
                            out_properties.fProperties.TexelFormatStorage = IMCodec::TexelFormat::I_B5_G6_R5;
                            out_properties.fProperties.TexelFormatDecompressed = IMCodec::TexelFormat::I_B5_G6_R5;
                        }
                        else
                        {
                            out_properties.fProperties.TexelFormatStorage = IMCodec::TexelFormat::I_B5_G5_R5_X1;
                            out_properties.fProperties.TexelFormatDecompressed = IMCodec::TexelFormat::I_B5_G5_R5_X1;
                        }
                        break;
                    case 24:
                        out_properties.fProperties.TexelFormatStorage = IMCodec::TexelFormat::I_B8_G8_R8;
                        out_properties.fProperties.TexelFormatDecompressed = IMCodec::TexelFormat::I_B8_G8_R8;
                        break;
                    case 32:
                        out_properties.fProperties.TexelFormatStorage = IMCodec::TexelFormat::I_B8_G8_R8_A8;
                        out_properties.fProperties.TexelFormatDecompressed = IMCodec::TexelFormat::I_B8_G8_R8_A8;
                        break;
                    default:
                        LL_EXCEPTION_UNEXPECTED_VALUE;
                    }

                    const auto texelSize = IMCodec::GetTexelInfo(out_properties.fProperties.TexelFormatDecompressed).texelSize;
                    out_properties.fProperties.RowPitchInBytes = LLUtils::Utility::Align<size_t>(bmpInfo.biWidth * texelSize / CHAR_BIT, sizeof(uint32_t));
                    const size_t destDataSize = out_properties.fProperties.RowPitchInBytes * out_properties.fProperties.Height;

                    out_properties.fData.Allocate(destDataSize);
                    const size_t sourceRowPitch = ((bmpInfo.biBitCount * bmpInfo.biWidth + 31) & ~31) >> 3;

                    switch (bmpInfo.biBitCount)
                    {
                        // Indexed color
                    case 1:
                    case 4:
                    case 8:
                    {
                        const uint32_t* colorTable = reinterpret_cast<const uint32_t*>(baseSourceAddress);
                        baseSourceAddress += (bmpInfo.biClrUsed == 0 ? (1 << bmpInfo.biBitCount) : bmpInfo.biClrUsed) * sizeof(uint32_t);
                        for (size_t line = 0; line < out_properties.fProperties.Height; line++)
                        {
                            auto sourceLineOffset = (out_properties.fProperties.Height - line - 1) * sourceRowPitch;
                            auto destLineOffset = line * out_properties.fProperties.RowPitchInBytes;

                            for (size_t x = 0; x < out_properties.fProperties.Width; x++)
                            {
                                uint8_t pixelIndex = GetValue(bmpInfo.biBitCount, baseSourceAddress + sourceLineOffset, x);
                                uint32_t color = 0xFF << 24 | colorTable[pixelIndex];
                                uint32_t* currentpixel = reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(out_properties.fData.data()) + destLineOffset) + x;
                                *currentpixel = color;
                            }
                        }
                        success = true;
                    }
                    break;
                    // Normal color
                    case 16:
                    case 24:
                    case 32:
                    {
                        
                        for (size_t line = 0; line < out_properties.fProperties.Height; line++)
                        {
                            auto sourceLineOffset = (out_properties.fProperties.Height - line - 1) * out_properties.fProperties.RowPitchInBytes;
                            auto destLineOffset = line * out_properties.fProperties.RowPitchInBytes;
                            const auto sourceLineAddress = baseSourceAddress + sourceLineOffset;
                            auto destLineAddress = reinterpret_cast<uint8_t*>(out_properties.fData.data()) + destLineOffset;

                            if (bmpInfo.biBitCount == 32)
                            {
                                //If it's a 32 bit bitmap, override alpha channel with full opacity.
                                //some application write just zeros to the alpha channel of 32 bit bitmaps.

                                for (size_t x = 0; x < out_properties.fProperties.Width; x += 1)
                                {
                                    using color32 = std::array<uint8_t, 4>;
                                    reinterpret_cast<color32*>(destLineAddress)[x] = reinterpret_cast<const color32*>(sourceLineAddress)[x];
                                    reinterpret_cast<color32*>(destLineAddress)[x][3] = 255;
                                }
                            }
                            else
                            {
                                memcpy(destLineAddress, sourceLineAddress, out_properties.fProperties.RowPitchInBytes);
                            }

                        }
                        success = true;
                        break;
                    }
                    }





                }
            }
            return success;
        }


//                if (size > sizeof(IcoDir))
//                {
//                    const IconFile* icoFile = reinterpret_cast<const IconFile*>(buffer);
//                    const uint8_t* baseAddress = reinterpret_cast<const uint8_t*>(icoFile);
//
//                    if (icoFile->icoDir.reserved != 0 || (icoFile->icoDir.type != 1 && icoFile->icoDir.type != 2))
//                        return false; // Not an Ico file 
//
//
//                    uint16_t numImages = icoFile->icoDir.numImages;
//                    bool error = false;
//                    out_vec_properties.resize(numImages);
//
//                    for (uint32_t i = 0; i < numImages; i++)
//                    {
//                        auto& currentDescriptor = out_vec_properties[numImages - i - 1]; // Reverse order, so largest icon is first.
//                        const IcoDirEntry* currentEntry = (&icoFile->entry)[i];
//
//                        const BITMAPINFOHEADER* bitmapInfo = reinterpret_cast<const BITMAPINFOHEADER*>(baseAddress + currentEntry->offsetData);
//                        if (bitmapInfo->biSize == 40)
//                        {
//                            if (bitmapInfo->biCompression != 0)
//                                LL_EXCEPTION_NOT_IMPLEMENT("Bitmap icons currently support only uncompressed images ");
//
//                            currentDescriptor.fProperties.Width = currentEntry->width != 0 ? currentEntry->width : 256;
//                            currentDescriptor.fProperties.Height = currentEntry->height != 0 ? currentEntry->height : 256;;
//                            currentDescriptor.fProperties.NumSubImages = 0;
//                            currentDescriptor.fProperties.RowPitchInBytes = bitmapInfo->biBitCount * currentDescriptor.fProperties.Width / CHAR_BIT;
//
//                            //Convert images to 32 bit to add transparency channel using the image mask.
//                            currentDescriptor.fProperties.TexelFormatStorage = IMCodec::TexelFormat::I_B8_G8_R8_A8;
//                            currentDescriptor.fProperties.TexelFormatDecompressed = IMCodec::TexelFormat::I_B8_G8_R8_A8;
//                            currentDescriptor.fProperties.RowPitchInBytes = 32 * currentDescriptor.fProperties.Width / CHAR_BIT;
//                            currentDescriptor.fData.Allocate(currentDescriptor.fProperties.Width * currentDescriptor.fProperties.Height * 4);
//
//                            const uint8_t* baseSourceAddress = reinterpret_cast<const uint8_t*>(bitmapInfo + 1);
//                            const size_t sourceRowPitch = ((bitmapInfo->biBitCount * currentDescriptor.fProperties.Width + 31) & ~31) >> 3;
//                            const size_t sourceRowPitchMask = (((MaskBitCount * currentDescriptor.fProperties.Width) + 31) & ~31) >> 3;
//                            const size_t masktartOffset = currentDescriptor.fProperties.Height * sourceRowPitch;
//
//                            switch (bitmapInfo->biBitCount)
//                            {
//                            case 1:
//                            case 4:
//                            case 8:
//                            {
//                                const uint32_t* colorTable = reinterpret_cast<const uint32_t*>(baseSourceAddress);
//                                baseSourceAddress += (bitmapInfo->biClrUsed == 0 ? (1 << bitmapInfo->biBitCount) : bitmapInfo->biClrUsed) * sizeof(uint32_t);
//
//                                if (bitmapInfo->biHeight != currentEntry->height * 2)
//                                {
//                                    LL_EXCEPTION(LLUtils::Exception::ErrorCode::InvalidState, "Could not find mask data");
//                                }
//
//                                for (size_t line = 0; line < currentDescriptor.fProperties.Height; line++)
//                                {
//                                    auto sourceLineOffset = (currentDescriptor.fProperties.Height - line - 1) * sourceRowPitch;
//                                    auto SourceLineMaskOffset = masktartOffset + (currentDescriptor.fProperties.Height - line - 1) * sourceRowPitchMask;
//                                    auto destLineOffset = line * currentDescriptor.fProperties.RowPitchInBytes;
//
//                                    for (size_t x = 0; x < currentDescriptor.fProperties.Width; x++)
//                                    {
//                                        uint8_t pixelIndex = GetValue(bitmapInfo->biBitCount, baseSourceAddress + sourceLineOffset, x);
//                                        uint8_t opacity = GetValue(MaskBitCount, baseSourceAddress + SourceLineMaskOffset, x);
//                                        uint32_t color = ((opacity == 1 ? 0x00 : 0xFF) << 24) | colorTable[pixelIndex];
//                                        uint32_t* currentpixel = reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(currentDescriptor.fData.data()) + destLineOffset) + x;
//                                        *currentpixel = color;
//                                    }
//                                }
//
//                            }
//                            break;
//                            case 24:
//                                if (bitmapInfo->biHeight != currentEntry->height * 2)
//                                {
//                                    LL_EXCEPTION(LLUtils::Exception::ErrorCode::InvalidState, "Could not find mask data");
//                                }
//
//                                for (size_t line = 0; line < currentDescriptor.fProperties.Height; line++)
//                                {
//                                    auto sourceLineOffset = (currentDescriptor.fProperties.Height - line - 1) * sourceRowPitch;
//                                    auto SourceLineMaskOffset = masktartOffset + (currentDescriptor.fProperties.Height - line - 1) * sourceRowPitchMask;
//                                    auto destLineOffset = line * currentDescriptor.fProperties.RowPitchInBytes;
//
//                                    for (size_t x = 0; x < currentDescriptor.fProperties.Width; x++)
//                                    {
//#pragma pack(push,1)
//                                        struct Color24
//                                        {
//                                            uint8_t B;
//                                            uint8_t G;
//                                            uint8_t R;
//                                            operator int() const
//                                            {
//                                                return (R << 16) | (G << 8) | B;
//                                            }
//                                        };
//#pragma pack(pop)
//
//                                        Color24 color24 = *reinterpret_cast<const Color24*>(baseSourceAddress + sourceLineOffset + x * sizeof(Color24));
//                                        uint8_t opacity = GetValue(MaskBitCount, baseSourceAddress + SourceLineMaskOffset, x);
//                                        uint32_t color = ((opacity == 1 ? 0x00 : 0xFF) << 24) | color24;
//                                        uint32_t* currentpixel = reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(currentDescriptor.fData.data()) + destLineOffset) + x;
//                                        *currentpixel = color;
//                                    }
//                                }
//
//
//                                break;
//                            case 32:
//                                for (size_t line = 0; line < currentDescriptor.fProperties.Height; line++)
//                                {
//                                    auto sourceOffset = (currentDescriptor.fProperties.Height - line - 1) * currentDescriptor.fProperties.RowPitchInBytes;
//                                    auto destoffset = line * currentDescriptor.fProperties.RowPitchInBytes;
//                                    currentDescriptor.fData.Write(reinterpret_cast<const std::byte*>(baseSourceAddress + sourceOffset), destoffset, currentDescriptor.fProperties.RowPitchInBytes);
//                                }
//                                break;
//
//
//                            default:
//                                LL_EXCEPTION_NOT_IMPLEMENT("Bit depth is currently unsupported");
//                            }
//
//                        }
//                        else if (sIsLoading == false)
//                        {
//                            IMCodec::VecImageSharedPtr image;
//                            sIsLoading = true;
//                            if (fImageLoader->Load(const_cast<uint8_t*>(baseAddress + currentEntry->offsetData), currentEntry->imageDataSize, nullptr, false, image) == true)
//                            {
//                                currentDescriptor = image.at(0)->GetDescriptor();
//
//                            }
//                            else
//                            {
//                                error = true;
//                                break;
//                            }
//                            sIsLoading = false;
//
//                        }
//                        else
//                        {
//                            error = true; // recursive call
//                        }
//
//                    }
//
//                    success = !error;
//                    out_vec_properties[0].fProperties.NumSubImages = numImages > 1 ? numImages : 0;
//                }
//            }
//            catch (...)
//            {
//                sIsLoading = false;
//                success = false;
//            }
//
//
//
//            return success;
//         
//        }
    };
}