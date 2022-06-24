#pragma once
#include <Image.h>
#include <libpsd.h>

namespace IMCodec
{
    class CodecPSD : public IImagePlugin
    {

    public:
        const PluginProperties& GetPluginProperties() override
        {
            static PluginProperties pluginProperties = 
            { 
                CodecCapabilities::Decode
                 ,L"PSDLib Codec"
                ,
                {
                    {
                        { L"Photosop document"}
                            ,{ L"psd"}
                    }
                }
            };
            return pluginProperties;
        }

        ImageResult Decode(const std::byte* buffer, std::size_t size, [[maybe_unused]] ImageLoadFlags loadFlags, const Parameters& params, ImageSharedPtr& out_image) override
        {
            ImageResult result = ImageResult::Fail;
            psd_context * context = nullptr;
            psd_status status;
            status = psd_image_load_merged_from_memory(&context, 
                reinterpret_cast<psd_char*>(const_cast<std::byte*>(buffer)), size);

            if (status == psd_status_done)
            {
                const uint32_t numChannels = 4;
               // merged is always BGBRA8

                auto imageItem = std::make_shared<ImageItem>();
                
                size_t mergedImageSize = numChannels * context->width * context->height;
                imageItem->data.Allocate(mergedImageSize);
                imageItem->data.Write(reinterpret_cast<std::byte*>(context->merged_image_data), 0, mergedImageSize);
                imageItem->itemType = ImageItemType::Image;
                
                imageItem->descriptor.width = context->width;
                imageItem->descriptor.height = context->height;
                imageItem->descriptor.texelFormatDecompressed = TexelFormat::I_B8_G8_R8_A8;
                imageItem->descriptor.rowPitchInBytes = numChannels * context->width;

                out_image = std::make_shared<Image>(imageItem, ImageItemType::Unknown);
                
                psd_image_free(context);
                result = ImageResult::Success;
            }

            return result;
        }
    };
 
}
