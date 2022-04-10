#pragma once

#include <Image.h>
#include <png.h>
#include <LLUtils/BitFlags.h>

namespace IMCodec
{
    class CodecPNG : public IImagePlugin
    {
    private:
        enum class PngFormatFlags : png_uint_32
        {
              None = 0 << 0
            , Alpha = 1 << 0
            , Color = 1 << 1
            , Linear = 1 << 2
            , ColorMap = 1 << 3
            , Reserved5 = 1 << 4
            , Reserved6 = 1 << 5
            , Reserved7 = 1 << 6
            , Reserved8 = 1 << 7
            , Reserved9 = 1 << 8

        };

		LLUTILS_DEFINE_ENUM_CLASS_FLAG_OPERATIONS_IN_CLASS(PngFormatFlags)

            
    public:

        PluginProperties& GetPluginProperties() override
        {
            static PluginProperties pluginProperties = { L"PNG plugin codec","png" };
            return pluginProperties;
        }

        //Base abstract methods
        ImageResult LoadMemoryImageFile(const std::byte* buffer, std::size_t size, [[maybe_unused]] ImageLoadFlags loadFlags, ImageSharedPtr& out_image) override
        {
            using namespace std;
            using namespace IMCodec;
            
            ImageResult result = ImageResult::Fail;
            png_image image; /* The control structure used by libpng */

            /* Initialize the 'png_image' structure. */
            memset(&image, 0, (sizeof image));
            image.version = PNG_IMAGE_VERSION;
            if (png_image_begin_read_from_memory(&image, buffer,size) != 0)
            {
                auto imageItem = std::make_shared<ImageItem>();
                imageItem->itemType = ImageItemType::Image;
                //Assign image properties
                const auto sizeofChannel = static_cast<unsigned int>(PNG_IMAGE_PIXEL_COMPONENT_SIZE(image.format));
                const auto rowPitch = PNG_IMAGE_ROW_STRIDE(image) * sizeofChannel;
                
                imageItem->descriptor.rowPitchInBytes = rowPitch;
                imageItem->descriptor.width = image.width;
                imageItem->descriptor.height = image.height;

                int numChannles = 0;


                LLUtils::Buffer colorMap;


                TexelFormat textFormat = TexelFormat::UNKNOWN;
                const bool isSingleChannel = sizeofChannel == 1;
                switch (image.format)
                {
                case PNG_FORMAT_GRAY | PNG_FORMAT_FLAG_LINEAR:
                case PNG_FORMAT_GRAY:
                    textFormat = isSingleChannel ? TexelFormat::I_X8 : TexelFormat::I_X16;
                      break;
                case PNG_FORMAT_RGBA | PNG_FORMAT_FLAG_LINEAR:
                case PNG_FORMAT_RGBA:
                    textFormat = isSingleChannel ? TexelFormat::I_R8_G8_B8_A8 : TexelFormat::I_R16_G16_B16_A16;
                    break;
                case PNG_FORMAT_RGB:
                case PNG_FORMAT_RGB | PNG_FORMAT_FLAG_LINEAR:
                    textFormat = isSingleChannel ? TexelFormat::I_R8_G8_B8 : TexelFormat::I_R16_G16_B16;
                    break;
                case PNG_FORMAT_FLAG_ALPHA | PNG_FORMAT_FLAG_COLOR | PNG_FORMAT_FLAG_COLORMAP | PNG_FORMAT_FLAG_LINEAR:
                case PNG_FORMAT_FLAG_ALPHA | PNG_FORMAT_FLAG_COLOR | PNG_FORMAT_FLAG_COLORMAP:
                    numChannles = 4;
                    textFormat = isSingleChannel ? TexelFormat::I_R8_G8_B8_A8 : TexelFormat::I_R16_G16_B16_A16;
                    //out_properties.fProperties.RowPitchInBytes = image.width * numChannles;
                    break;
                case  PNG_FORMAT_FLAG_COLOR | PNG_FORMAT_FLAG_COLORMAP | PNG_FORMAT_FLAG_LINEAR:
                case  PNG_FORMAT_FLAG_COLOR | PNG_FORMAT_FLAG_COLORMAP:
                    numChannles = 3;
                    textFormat = isSingleChannel ? TexelFormat::I_R8_G8_B8 : TexelFormat::I_R16_G16_B16;
                    //out_properties.fProperties.RowPitchInBytes = image.width * numChannles;
                    break;
                default:
                    LL_EXCEPTION_NOT_IMPLEMENT("Png image format not supported");
                }
                 
                uint8_t const colormapWidth = 1;

                imageItem->descriptor.texelFormatDecompressed = textFormat;


                 LLUtils::BitFlags<PngFormatFlags> formatFlags( static_cast<PngFormatFlags>(image.format));


                 if (formatFlags.test(PngFormatFlags::ColorMap))
                 {
                     colorMap.Allocate(numChannles * colormapWidth * image.colormap_entries);
                     imageItem->descriptor.rowPitchInBytes = numChannles * image.width;
                 }

                //read buffer
                 imageItem->data.Allocate(PNG_IMAGE_SIZE(image));

                if (imageItem->data.data() != nullptr &&
                    png_image_finish_read (&image, nullptr/*background*/, imageItem->data.data(),
                        PNG_IMAGE_ROW_STRIDE(image), colorMap.data()) != 0)
                {

                    if (colorMap != nullptr)
                    {
                        //Resolve color map to a bitmap
                        size_t pixelSize = numChannles * sizeofChannel;
                        LLUtils::Buffer unmappedBuuffer(numChannles * image.width * image.height);
                        
                        for (size_t pixelIndex = 0; pixelIndex < image.width * image.height; pixelIndex++)
                        {
                            size_t colorIndex = ((uint8_t*)(imageItem->data.data()))[pixelIndex];

                            size_t sourcePos = colorIndex * pixelSize;
                            size_t destPos = pixelIndex * pixelSize;
                            
                            memcpy( reinterpret_cast<uint8_t*>(unmappedBuuffer.data()) + destPos
                                , reinterpret_cast<uint8_t*>(colorMap.data()) + sourcePos, pixelSize);
                        }
                        
                        imageItem->data = std::move(unmappedBuuffer);
                    }

                    out_image = std::make_shared<Image>(imageItem, ImageItemType::Unknown);
                    result = ImageResult::Success;
                }
            }
            return result;
        }
    };
}
