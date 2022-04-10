#pragma once
#include <Image.h>
#include <LLUtils/Warnings.h>
LLUTILS_DISABLE_WARNING_PUSH
LLUTILS_DISABLE_WARNING_SEMICOLON_OUTSIDE_FUNCTION
#include <webp/decode.h>
LLUTILS_DISABLE_WARNING_POP
namespace IMCodec
{
    class CodecWebP : public IImagePlugin
    {

    public:
        PluginProperties& GetPluginProperties() override
        {
            static PluginProperties pluginProperties = { L"WebP codec","webp" };
            return pluginProperties;
        }

        ImageResult LoadMemoryImageFile(const std::byte* buffer, std::size_t size, [[maybe_unused]] ImageLoadFlags loadFlags, ImageSharedPtr& out_image) override
        {
            int width;
            int height;
            
            
            if (WebPGetInfo(reinterpret_cast<const std::uint8_t*>(buffer), size, &width, &height) != 0)
            {
                const size_t decodedBufferSize = width * height * 4;
                LLUtils::Buffer decodedBuffer(decodedBufferSize);
                
                if (WebPDecodeBGRAInto(reinterpret_cast<const std::uint8_t*>(buffer), size, reinterpret_cast<uint8_t*>(decodedBuffer.data()),
                    decodedBufferSize, width * 4) != nullptr)
                {
                    auto imageItem = std::make_shared<ImageItem>();
                    imageItem->itemType = ImageItemType::Image;
                    imageItem->descriptor.height = height;
                    imageItem->descriptor.width = width;
                    imageItem->descriptor.rowPitchInBytes = width * 4;
                    imageItem->descriptor.texelFormatStorage = TexelFormat::I_B8_G8_R8_A8;
                    imageItem->descriptor.texelFormatDecompressed = TexelFormat::I_B8_G8_R8_A8;
                    imageItem->data = std::move(decodedBuffer);
                    out_image = std::make_shared<Image>(imageItem, ImageItemType::Unknown);
                    return ImageResult::Success;
                }

            }
            return ImageResult::Fail;
        }
    };
}
