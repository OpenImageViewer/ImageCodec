#pragma once

#include <assert.h>
#include <IImagePlugin.h>
#include <gif_lib.h>
#include <Image.h>
#include <LLUtils/StopWatch.h>
namespace IMCodec
{

    struct GifReadContext
    {
        const std::byte* buffer;
        size_t bufferSize;
        size_t pos;
    };

 
    int ReadGifBuffer(GifFileType * gifType, GifByteType * byteType, int length)
    {
        GifReadContext* context = (GifReadContext*)(gifType->UserData);
        // dont overflow buffer;
        size_t bytesToRead = std::min<size_t>(length, context->bufferSize - context->pos);
        if (bytesToRead != static_cast<size_t>(length))
        {
            //TODO: warning
        }
        memcpy(byteType, context->buffer + context->pos, bytesToRead);
        context->pos += bytesToRead;
        return static_cast<int>(bytesToRead);
    }

    class CodecGif : public IImagePlugin
    {
    private:
        PluginProperties mPluginProperties = 
        { 
            // {1D076943-F907-426A-8DC6-838B8CCE320C}
             { 0x1d076943, 0xf907, 0x426a, { 0x8d, 0xc6, 0x83, 0x8b, 0x8c, 0xce, 0x32, 0xc } }
            ,CodecCapabilities::Decode
            , L"Gif Codec"
                ,
                {
                    {
                        { L"Graphics Interchange Format"}
                       ,{ L"gif"}
                    }
                }
        };
    public:
        const PluginProperties& GetPluginProperties() override
        {
            return mPluginProperties;
        }


        struct FrameData
        {
            GraphicsControlBlock gcb{ 0, false , 0 , -1 };
            GifImageDesc imagedesc;
        };

        


        FrameData GetFrameData(SavedImage* gifImage)
        {
            FrameData frameData{};
            GraphicsControlBlock gcb;
            
            frameData.imagedesc = gifImage->ImageDesc;

            for (int i = 0; i < gifImage->ExtensionBlockCount; i++)
            {
                const ExtensionBlock& eb = gifImage->ExtensionBlocks[i];
                switch (eb.Function)
                {
                case CONTINUE_EXT_FUNC_CODE:    /* continuation subblock */
                    break;
                case COMMENT_EXT_FUNC_CODE:    /* comment */
                    break;
                case GRAPHICS_EXT_FUNC_CODE:    /* graphics control (GIF89) */
                    if (DGifExtensionToGCB(eb.ByteCount, eb.Bytes, &gcb) == GIF_OK)
                        frameData.gcb = gcb;
                    break;
                case PLAINTEXT_EXT_FUNC_CODE:    /* plaintext */
                    break;
                case APPLICATION_EXT_FUNC_CODE:    /* application block */
                    break;
                }

            }
            return frameData;
        }

        LLUtils::Buffer GetFrameBuffer(const SavedImage* image, const FrameData& frameData, const ColorMapObject* SColorMap)
        {

            const auto colorMap = image->ImageDesc.ColorMap != nullptr ? image->ImageDesc.ColorMap : SColorMap;
            LLUtils::Buffer frameBuffer(image->ImageDesc.Height * image->ImageDesc.Width * 4);
            for (GifWord y = 0; y < image->ImageDesc.Height; y++)
                for (GifWord x = 0; x < image->ImageDesc.Width; x++)
                {
                    const uint32_t bufPos = (y * image->ImageDesc.Width) + x;
                    int idx = image->RasterBits[bufPos];
                    unsigned long alpha = idx == frameData.gcb.TransparentColor ? 0 : 0xff;
                    GifColorType colorType = colorMap->Colors[idx];
                    const uint32_t RGBACOLOR = (alpha << 24) | (colorType.Blue << 16) | (colorType.Green << 8) | colorType.Red;
                    ((uint32_t*)frameBuffer.data())[bufPos] = RGBACOLOR;

                }
            return frameBuffer;
        }
        
        std::vector<FrameData> GetFramesData(GifFileType* gif)
        {
            
            const auto numFrames = gif->ImageCount;
            std::vector<FrameData> framesData(numFrames);
            for (auto frameNumber = 0; frameNumber < numFrames; frameNumber++)
            {
                auto currentImage = gif->SavedImages + frameNumber;
                framesData.at(frameNumber) = GetFrameData(currentImage);
            }

            return framesData;
        }

        std::vector<LLUtils::Buffer> BakeGifFrames(GifFileType* gif, std::vector<FrameData> framesData)
        {
            const auto numBuffers = gif->ImageCount;
            const auto bpp = gif->SColorResolution;

            if (bpp > 8)
                LL_EXCEPTION(LLUtils::Exception::ErrorCode::NotImplemented, "unsupported gif type");

            std::vector<LLUtils::Buffer> buffers(numBuffers);

            for (auto i = 0; i < numBuffers; i++)
            {
                auto currentImage = gif->SavedImages + i;
                buffers.at(i) = GetFrameBuffer(currentImage, framesData.at(i), gif->SColorMap);
            }

            LLUtils::Buffer compositeImage(gif->SHeight * gif->SWidth * 4);

            // Clear background first.
            for (int i = 0; i < gif->SHeight * gif->SWidth; i++)
                reinterpret_cast<GifWord*>(compositeImage.data())[i] = gif->SBackGroundColor;


            for (auto i = 0; i < numBuffers; i++)
            {
                const auto& frameData = framesData.at(i);
                auto& currentBuffer = buffers.at(i);

                switch (frameData.gcb.DisposalMode)
                {
                case DISPOSAL_UNSPECIFIED:
                    for (auto y = 0; y < frameData.imagedesc.Height; y++)
                        for (auto x = 0; x < frameData.imagedesc.Width; x++)
                        {
                            const auto& sourceColor = reinterpret_cast<const uint32_t*>(currentBuffer.data())[x + y * frameData.imagedesc.Width];
                            auto& targetColor = reinterpret_cast<uint32_t*>(compositeImage.data())[(x + frameData.imagedesc.Left) + (y + frameData.imagedesc.Top) * gif->SWidth];
                            targetColor = sourceColor;
                        }
                    break;
                case DISPOSE_DO_NOT:
                    for (auto y = 0 ; y < frameData.imagedesc.Height; y++)
                        for (auto x = 0; x < frameData.imagedesc.Width; x++)
                        {
                            const auto& sourceColor = reinterpret_cast<const uint32_t*>(currentBuffer.data())[x + y * frameData.imagedesc.Width];
                            auto& targetColor = reinterpret_cast<uint32_t*>(compositeImage.data())[ (x + frameData.imagedesc.Left) + (y + frameData.imagedesc.Top) * gif->SWidth];
                            const int sourceAlpha = sourceColor >> 24;
                            if (sourceAlpha > 0)
                                targetColor = sourceColor;
                        }

                    break;
                case DISPOSE_BACKGROUND:   
                    for (auto y = 0; y < frameData.imagedesc.Height; y++)
                        for (auto x = 0; x < frameData.imagedesc.Width; x++)
                        {
                            const auto& sourceColor = reinterpret_cast<const uint32_t*>(currentBuffer.data())[x + y * frameData.imagedesc.Width];
                            auto& targetColor = reinterpret_cast<uint32_t*>(compositeImage.data())[(x + frameData.imagedesc.Left) + (y + frameData.imagedesc.Top) * gif->SWidth];
                            const int sourceAlpha = sourceColor >> 24;
                            targetColor = sourceAlpha > 0 ? sourceColor : gif->SBackGroundColor;
                        }

                    break;
                case DISPOSE_PREVIOUS:
                    break;
                }
                
                currentBuffer = compositeImage.Clone();
            }

            return buffers;
        }

        //Base abstract methods
        ImageResult Decode(const std::byte* buffer, std::size_t size, [[maybe_unused]] ImageLoadFlags loadFlags, const Parameters& params, ImageSharedPtr& out_image) override
        {
            LLUtils::StopWatch stopWatch(true);
            
            ImageResult result = ImageResult::UnknownError;
            int error;
            GifReadContext context{ buffer, size, 0 };

            GifFileType* gif = DGifOpen((void*)&context, &ReadGifBuffer, &error);

            if (gif != nullptr && DGifSlurp(gif) == GIF_OK)
            {
                ImageItemType subItemType = ImageItemType::Unknown;
                
                auto isMultiImage = gif->ImageCount > 1;

                if (isMultiImage)
                {
                    subItemType = ImageItemType::AnimationFrame;
                    auto containerImageItem = std::make_shared<ImageItem>();
                    containerImageItem->itemType = ImageItemType::Container;
                    out_image = std::make_shared<Image>(containerImageItem, subItemType);
                }
                else
                {
                    subItemType = ImageItemType::Image;
                }

                auto framesData = GetFramesData(gif);
                auto frameBuffers = BakeGifFrames(gif, framesData);

                double loadTIme = -1;

                for (auto imageIndex = 0; imageIndex < gif->ImageCount; imageIndex++)
                {
                    const auto& currentFrameData = framesData.at(imageIndex);
                    ImageItemSharedPtr imageItem = std::make_shared<ImageItem>();
                    imageItem->itemType = subItemType;
                    
                    //When baking frames, the size of the image should be the gif virtual canvas for all the frames
                    //, instead of the current frame size itself.
                    imageItem->descriptor.width = gif->SWidth;
                    imageItem->descriptor.height = gif->SHeight;
                    imageItem->descriptor.texelFormatDecompressed = IMCodec::TexelFormat::I_R8_G8_B8_A8;
                    imageItem->descriptor.texelFormatStorage = IMCodec::TexelFormat::I_R8_G8_B8_A8;
                    imageItem->descriptor.rowPitchInBytes = IMCodec::GetTexelFormatSize(imageItem->descriptor.texelFormatDecompressed) * imageItem->descriptor.width / CHAR_BIT;
                    imageItem->data = std::move(frameBuffers.at(imageIndex));
                    imageItem->animationData.delayMilliseconds = currentFrameData.gcb.DelayTime * 10; // multiply by 10 to convert centiseconds to milliseconds.
                    imageItem->processData.pluginUsed = GetPluginProperties().id;

                    //Estimation on frame loading time, since frame are loaded together is a bit cumbersome to precisly time the 'load time'

                    if (loadTIme == -1)
                    {
                        using namespace LLUtils;
                        loadTIme = static_cast<double>(stopWatch.GetElapsedTimeReal(StopWatch::TimeUnit::Milliseconds));
                    }

                    imageItem->processData.processTime = loadTIme;
                    
                    if (isMultiImage)
                        out_image->SetSubImage(imageIndex, std::make_shared<Image>(imageItem, ImageItemType::Unknown));
                    else
                        out_image = std::make_shared<Image>(imageItem, ImageItemType::Unknown);

                }

                DGifCloseFile(gif, &error);

                result = ImageResult::Success;
            }

            return result;
        }
    };
}
