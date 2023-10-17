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
                // {D5DD791B-D177-4584-A076-226CC79D4CF9}
                 { 0xd5dd791b, 0xd177, 0x4584, { 0xa0, 0x76, 0x22, 0x6c, 0xc7, 0x9d, 0x4c, 0xf9 } }
                ,CodecCapabilities::Decode
                ,LLUTILS_TEXT("PSDLib Codec")
                ,
                {
                    {
                        { LLUTILS_TEXT("Photosop document")}
                            ,{ LLUTILS_TEXT("psd")}
                    }
                }
            };
            return pluginProperties;
        }

        ImageResult Decode(const std::byte* buffer, std::size_t size, [[maybe_unused]] ImageLoadFlags loadFlags, const Parameters& params, ImageSharedPtr& out_image) override
        {
            ImageResult result = ImageResult::UnknownError;
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
