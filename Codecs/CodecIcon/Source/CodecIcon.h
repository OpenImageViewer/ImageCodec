#pragma once

#include <IImagePlugin.h>

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

#pragma pack(pop)

    class CodecIcon : public IImagePlugin
    {
    private:
            PluginProperties mPluginProperties;
            IImageLoader* fImageLoader = nullptr;
            static constexpr uint8_t MaskBitCount = 1;
    public:
        inline thread_local static bool sIsLoading = false;

        CodecIcon(IImageLoader* imageLoader) : mPluginProperties({ "Builtin Icon codec","ico;icon;cur" , true}), fImageLoader(imageLoader)
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

        virtual bool LoadImages(const uint8_t* buffer, std::size_t size, std::vector<ImageDescriptor>& out_vec_properties)
        {
            using namespace std;
            bool success = false;
            try
            {  
                if (size > sizeof(IcoDir))
                {
                    const IconFile* icoFile = reinterpret_cast<const IconFile*>(buffer);
                    const uint8_t* baseAddress = reinterpret_cast<const uint8_t*>(icoFile);
                    
                    if (icoFile->icoDir.reserved != 0 ||  (icoFile->icoDir.type != 1 && icoFile->icoDir.type != 2))
                        return false; // Not an Ico file 

                    
                    uint16_t numImages = icoFile->icoDir.numImages;
                    bool error = false;
                    out_vec_properties.resize(numImages);

                    for (uint32_t i = 0; i < numImages; i++)
                    {
                        auto& currentDescriptor = out_vec_properties[numImages - i - 1]; // Reverse order, so largest icon is first.
                        const IcoDirEntry* currentEntry = (&icoFile->entry)[i];
                        
                        const BITMAPINFOHEADER* bitmapInfo = reinterpret_cast<const BITMAPINFOHEADER*>(baseAddress + currentEntry->offsetData);
                        if (bitmapInfo->biSize == 40)
                        {
                            if (bitmapInfo->biCompression != 0)
                                LL_EXCEPTION_NOT_IMPLEMENT("Bitmap icons currently support only uncompressed images ");

                            currentDescriptor.fProperties.Width = currentEntry->width != 0 ? currentEntry->width : 256;
                            currentDescriptor.fProperties.Height = currentEntry->height != 0 ? currentEntry->height : 256;;
                            currentDescriptor.fProperties.NumSubImages = 0;
                            currentDescriptor.fProperties.RowPitchInBytes = bitmapInfo->biBitCount * currentDescriptor.fProperties.Width / CHAR_BIT ;
                            
                            //Convert images to 32 bit to add transparency channel using the image mask.
                            currentDescriptor.fProperties.TexelFormatStorage = IMCodec::TexelFormat::I_B8_G8_R8_A8;
                            currentDescriptor.fProperties.TexelFormatDecompressed = IMCodec::TexelFormat::I_B8_G8_R8_A8;
                            currentDescriptor.fProperties.RowPitchInBytes = 32 * currentDescriptor.fProperties.Width / CHAR_BIT;
                            currentDescriptor.fData.Allocate(currentDescriptor.fProperties.Width * currentDescriptor.fProperties.Height * 4);

                            const uint8_t* baseSourceAddress = reinterpret_cast<const uint8_t*>(bitmapInfo + 1);
                            const size_t sourceRowPitch = ((bitmapInfo->biBitCount * currentDescriptor.fProperties.Width + 31) & ~31) >> 3;
                            const size_t sourceRowPitchMask = (((MaskBitCount * currentDescriptor.fProperties.Width) + 31) & ~31) >> 3;
                            const size_t masktartOffset = currentDescriptor.fProperties.Height * sourceRowPitch;

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

                                for (size_t line = 0; line < currentDescriptor.fProperties.Height; line++)
                                {
                                    auto sourceLineOffset = (currentDescriptor.fProperties.Height - line - 1) * sourceRowPitch;
                                    auto SourceLineMaskOffset = masktartOffset + (currentDescriptor.fProperties.Height - line - 1) * sourceRowPitchMask;
                                    auto destLineOffset = line * currentDescriptor.fProperties.RowPitchInBytes;

                                    for (int x = 0; x < currentDescriptor.fProperties.Width; x++)
                                    {
                                        uint8_t pixelIndex = GetValue(bitmapInfo->biBitCount, baseSourceAddress + sourceLineOffset, x);
                                        uint8_t opacity = GetValue(MaskBitCount, baseSourceAddress + SourceLineMaskOffset, x);
                                        uint32_t color = ((opacity == 1 ? 0x00 : 0xFF) << 24) | colorTable[pixelIndex];
                                        uint32_t* currentpixel = reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(currentDescriptor.fData.GetBuffer()) + destLineOffset) + x;
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

                                for (size_t line = 0; line < currentDescriptor.fProperties.Height; line++)
                                {
                                    auto sourceLineOffset = (currentDescriptor.fProperties.Height - line - 1) * sourceRowPitch;
                                    auto SourceLineMaskOffset = masktartOffset + (currentDescriptor.fProperties.Height - line - 1) * sourceRowPitchMask;
                                    auto destLineOffset = line * currentDescriptor.fProperties.RowPitchInBytes;

                                    for (int x = 0; x < currentDescriptor.fProperties.Width; x++)
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
                                        uint32_t* currentpixel = reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(currentDescriptor.fData.GetBuffer()) + destLineOffset) + x;
                                        *currentpixel = color;
                                    }
                                }


                                break;
                            case 32:
                                for (size_t line = 0; line < currentDescriptor.fProperties.Height; line++)
                                {
                                    auto sourceOffset = (currentDescriptor.fProperties.Height - line - 1) * currentDescriptor.fProperties.RowPitchInBytes;
                                    auto destoffset = line * currentDescriptor.fProperties.RowPitchInBytes;
                                    currentDescriptor.fData.Write(reinterpret_cast<const std::byte*>(baseSourceAddress + sourceOffset), destoffset, currentDescriptor.fProperties.RowPitchInBytes);
                                }
                                break;


                            default:
                                LL_EXCEPTION_NOT_IMPLEMENT("Bit depth is currently unsupported");
                            }
                            
                        }
                        else if (sIsLoading == false)
                        {
                            IMCodec::VecImageSharedPtr image;
                            sIsLoading = true;
                            if (fImageLoader->Load(const_cast<uint8_t*>(baseAddress + currentEntry->offsetData), currentEntry->imageDataSize, nullptr, false, image) == true)
                            {
                                currentDescriptor = image.at(0)->GetDescriptor();

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
                   
                    }

                    success = !error;
                    out_vec_properties[0].fProperties.NumSubImages = numImages > 1 ?  numImages : 0;
                }
            }
            catch (...)
            {
                sIsLoading = false;
                success = false;
            }

            
            
            return success;
        }


        //Base abstract methods
        bool LoadImage(const uint8_t* buffer, std::size_t size, ImageDescriptor& out_properties) override
        {
            return false;
         
        }
    };
}