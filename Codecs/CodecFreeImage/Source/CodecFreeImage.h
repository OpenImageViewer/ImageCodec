#pragma once

#include <IImagePlugin.h>
#include <FreeImage.h>
#include <Image.h>

namespace IMCodec
{
    class CodecFreeImage : public IImagePlugin
    {

    public:
        PluginProperties& GetPluginProperties() override
        {
            static PluginProperties pluginProperties =
            {
                L"FreeImage plugin",
                "BMP;ICO;JPEG;JNG;KOALA;LBM;IFF;LBM;MNG;PBM;PBMRAW;PCD;PCX;PGM;PGMRAW;PNG;PPM;PPMRAW;RAS;TGA;TIFF;TIF;WBMP;PSD;CUT;XBM;XPM;DDS;GIF;HDR;FAXG3;SGI;EXR;J2K;JP2;PFM;PICT;RAW;WEBP;JXR;CUR"
                };
            
            return pluginProperties;
        }

        ImageResult LoadMemoryImageFile(const std::byte* buffer, std::size_t size, [[maybe_unused]] ImageLoadFlags loadFlags, ImageSharedPtr& out_image) override
        {
            FIBITMAP* freeImageHandle;
            ImageResult result = ImageResult::Fail;

            using namespace IMCodec;

            FIMEMORY* memStream = FreeImage_OpenMemory(reinterpret_cast<BYTE*>(const_cast<std::byte*>( buffer)),static_cast<DWORD>(size));
            FREE_IMAGE_FORMAT format =  FreeImage_GetFileTypeFromMemory(memStream, static_cast<int>(size));
            freeImageHandle = FreeImage_LoadFromMemory(format, memStream, 0);
            if (freeImageHandle && FreeImage_FlipVertical(freeImageHandle))
            {

                BITMAPINFO* imageInfo = FreeImage_GetInfo(freeImageHandle);
                FREE_IMAGE_TYPE TexelFormat = FreeImage_GetImageType(freeImageHandle);

                const BITMAPINFOHEADER& header = imageInfo->bmiHeader;

                auto imageItem = std::make_shared<ImageItem>();
                imageItem->itemType = ImageItemType::Image;

                imageItem->descriptor.width = header.biWidth;
                imageItem->descriptor.height = header.biHeight;
                imageItem->descriptor.rowPitchInBytes = FreeImage_GetPitch(freeImageHandle);

                std::size_t imageSizeInMemory = header.biHeight * imageItem->descriptor.rowPitchInBytes;
                
                imageItem->data.Allocate(imageSizeInMemory);
                imageItem->data.Write(reinterpret_cast<std::byte*>(FreeImage_GetBits(freeImageHandle)), 0, imageSizeInMemory);


                switch (TexelFormat)
                {
                case FIT_BITMAP:
                {
                    switch (header.biBitCount)
                    {
                    case 8:
                        imageItem->descriptor.texelFormatDecompressed = TexelFormat::I_X8;
                        break;
                    case 32:
                        if (format == FIF_BMP)
                        {
                            // Hack: BMP isn't read with an alpha channel.
                            uint32_t* line = (uint32_t*)imageItem->data.data();
                            for (uint32_t y = 0; y < imageItem->descriptor.height; y++)
                            {
                                for (uint32_t x = 0; x < imageItem->descriptor.width; x++)
                                    line[x] = line[x] | 0xFF000000;

                                line = reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(line) + imageItem->descriptor.rowPitchInBytes);

                            }
                        }
                        imageItem->descriptor.texelFormatDecompressed = TexelFormat::I_B8_G8_R8_A8;
                        break;
                    case 24:
                        imageItem->descriptor.texelFormatDecompressed = TexelFormat::I_B8_G8_R8;
                        break;
                    default:
                        imageItem->descriptor.texelFormatDecompressed = TexelFormat::UNKNOWN;

                    }
                }
                break;
                default:
                    LL_EXCEPTION_NOT_IMPLEMENT("Decoding the type of texture is yet to be implemented.");

                }

                if (freeImageHandle != nullptr)
                    FreeImage_Unload(freeImageHandle);

                out_image = std::make_shared<Image>(imageItem, ImageItemType::Unknown);

                result = ImageResult::Success;
            }

            if (memStream != nullptr)
                FreeImage_CloseMemory(memStream);
                
            return result;
        }
    };
}