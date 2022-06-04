#pragma once
#include <Image.h>
#include <LLUtils/Warnings.h>
LLUTILS_DISABLE_WARNING_PUSH
LLUTILS_DISABLE_WARNING_SEMICOLON_OUTSIDE_FUNCTION
#include <webp/decode.h>
#include <webp/demux.h>

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
            ImageResult result = ImageResult::Fail;

            WebPData webp_data
            {
                reinterpret_cast<const uint8_t*>(buffer)
                , size
            };
            
            WebPAnimDecoder* dec = WebPAnimDecoderNew(&webp_data, nullptr);
            struct AnimDecoderDeletor
            {
                WebPAnimDecoder* decoder;
                ~AnimDecoderDeletor()
                {
                    WebPAnimDecoderDelete(decoder);
                }
                
            } deletor{ dec };

            if (dec != nullptr)
            {
                WebPAnimInfo animInfo;
                if (!WebPAnimDecoderGetInfo(dec, &animInfo))
                    LL_EXCEPTION(LLUtils::Exception::ErrorCode::InvalidState, "Could not create WebP animation info");

                const auto totalFrames = animInfo.frame_count;

                if (totalFrames > 1)
                {
                    //Decode an animation
                    constexpr auto numChannels = 4;

                    auto canvasWidth = animInfo.canvas_width;
                    auto canvasHeight = animInfo.canvas_height;
                    auto loopCount = animInfo.loop_count;
                    auto bgColor = animInfo.bgcolor;

                    const auto frameSizeInBytes = canvasWidth * canvasHeight * numChannels;

                    auto imageItem = std::make_shared<ImageItem>();
                    imageItem->itemType = ImageItemType::Container;
                    out_image = std::make_shared<Image>(imageItem, ImageItemType::AnimationFrame);
                    out_image->SetNumSubImages(totalFrames);


                    int frameIndex = 0;
                    int lastTimeStamp = 0;

                    while (WebPAnimDecoderHasMoreFrames(dec))
                    {

                        uint8_t* frame_rgba;
                        int timestamp;
                        if (!WebPAnimDecoderGetNext(dec, &frame_rgba, &timestamp))
                            LL_EXCEPTION(LLUtils::Exception::ErrorCode::InvalidState, "Could not decode WebP frame");

                        auto frameImageItem = std::make_shared<ImageItem>();
                        frameImageItem->itemType = ImageItemType::Image;
                        frameImageItem->data.Allocate(frameSizeInBytes);
                        frameImageItem->data.Write(reinterpret_cast<const std::byte*>(frame_rgba), 0, frameSizeInBytes);
                        frameImageItem->animationData.delayMilliseconds = timestamp - lastTimeStamp;
                        frameImageItem->descriptor.height = canvasHeight;
                        frameImageItem->descriptor.width = canvasWidth;
                        frameImageItem->descriptor.texelFormatDecompressed = TexelFormat::I_R8_G8_B8_A8;
                        frameImageItem->descriptor.texelFormatStorage = TexelFormat::I_R8_G8_B8_A8;
                        frameImageItem->descriptor.rowPitchInBytes = frameImageItem->descriptor.width * GetTexelFormatSize(frameImageItem->descriptor.texelFormatDecompressed) / CHAR_BIT;
                        out_image->SetSubImage(frameIndex, std::make_shared<Image>(frameImageItem, ImageItemType::Unknown));
                        lastTimeStamp = timestamp;
                        frameIndex++;
                    }
                    result = ImageResult::Success;
                }
            }
            else
            {
                //Decode an Image
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
                        result = ImageResult::Success;
                    }
                }
            }
           return result;
       }
    };
}
