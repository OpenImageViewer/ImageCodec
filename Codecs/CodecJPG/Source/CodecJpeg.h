#pragma once

#include <Image.h>
#include <turbojpeg.h>

namespace IMCodec
{
    class CodecJpeg : public IImagePlugin
    {
    private:
       


    public:
        PluginProperties& GetPluginProperties() override
        {
            static PluginProperties pluginProperties = { L"Jpeg plugin codec","jpg;jpeg" };
            return pluginProperties;
        }

        ImageResult LoadMemoryImageFile(const std::byte* buffer, std::size_t size, [[maybe_unused]] ImageLoadFlags loadFlags, ImageSharedPtr& out_image) override
        {
            static tjhandle ftjHandle = tjInitDecompress();
            ImageResult result = ImageResult::Fail;
        
            int width = 0;
            int height = 0;

            int bytesPerPixel = 4;
            int subsamp;
            unsigned long jpegSize = static_cast<unsigned long>(size);
            if (tjDecompressHeader2(ftjHandle,reinterpret_cast<unsigned char*>(const_cast<std::byte*>( buffer)), jpegSize, &width, &height, &subsamp) != -1)
            {
                size_t imageDataSize = width * height * bytesPerPixel;
                auto imageItem = std::make_shared<ImageItem>();
                imageItem->itemType = ImageItemType::Image;
                imageItem->data.Allocate(imageDataSize);

                if (tjDecompress2(ftjHandle, const_cast< uint8_t*>(reinterpret_cast<const uint8_t*>( buffer)), jpegSize, reinterpret_cast<unsigned char*>(imageItem->data.data()), width, width * bytesPerPixel, height, TJPF_RGBA, 0) != -1)
                {
                    imageItem->descriptor.texelFormatDecompressed = TexelFormat::I_R8_G8_B8_A8;
                    imageItem->descriptor.width = width;
                    imageItem->descriptor.height = height;
                    imageItem->descriptor.rowPitchInBytes = bytesPerPixel * width;
                    out_image = std::make_shared<Image>(imageItem, ImageItemType::Unknown);
                    result = ImageResult::Success;
                }
            }
            return result;
        }
    };
}
