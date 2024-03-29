#pragma once

#include <Image.h>
#include <turbojpeg.h>
#include <LLUtils/FileHelper.h>
namespace IMCodec
{
    class CodecJpeg : public IImagePlugin
    {
    public:
        const PluginProperties& GetPluginProperties() override
        {
            static PluginProperties pluginProperties = 
            { 
                // {7798CB6A-5546-4EAD-BA95-A114C8520D0D}
                { 0x7798cb6a, 0x5546, 0x4ead, { 0xba, 0x95, 0xa1, 0x14, 0xc8, 0x52, 0xd, 0xd } }
                , CodecCapabilities::Encode | CodecCapabilities::Decode 
                ,LLUTILS_TEXT("TurboJpeg") 
                ,
                {
                    {
                        { LLUTILS_TEXT("Joint Photographic Experts Group")}
                            ,{ LLUTILS_TEXT("jpg"),LLUTILS_TEXT("jpeg")}
                    }
                }
            };
            return pluginProperties;
        }

        void ComputeDesiredDimensions(int canvasWidth, int canvasHeight, int &imageWidth, int &imageHeight)
        {
            double desiredScale = std::min((double)canvasWidth / imageWidth, (double)canvasHeight / imageHeight);
            
            int numscalingFactors = std::numeric_limits<int>::max();
            auto scalinfFactors = tjGetScalingFactors(&numscalingFactors);

            double minDist = std::numeric_limits<double>::max();
            //int minDistIndex
            int i = 0;
            for (i = 0; i < numscalingFactors; i++)
            {
                auto currentScale = static_cast<double>(scalinfFactors[i].num) / static_cast<double>(scalinfFactors[i].denom);
                auto currentDist = std::abs(currentScale - desiredScale);
                if (currentDist < minDist)
                {
                    minDist = currentDist;
                }
                else
                {
                    break;
                }
            }


            
            imageWidth = TJSCALED(imageWidth, scalinfFactors[std::min(numscalingFactors - 1, i)]);
            imageHeight = TJSCALED(imageHeight, scalinfFactors[std::min(numscalingFactors - 1, i)]);


        }

        ImageResult Decode(const std::byte* buffer, std::size_t size, [[maybe_unused]] ImageLoadFlags loadFlags,const Parameters& params, ImageSharedPtr& out_image) override
        {
            static thread_local tjhandle ftjHandle = tjInitDecompress();
            ImageResult result = ImageResult::UnknownError;
/*
            int canvasWidth = 0;
            int canvasHeight = 0;

            if (params.contains(L"canvasWidth"))
                canvasWidth = std::get<int>(params.at(L"canvasWidth"));

            if (params.contains(L"canvasHeight"))
                canvasHeight = std::get<int>(params.at(L"canvasHeight"));
*/
        
        
            int width = 0;
            int height = 0;

            int bytesPerPixel = 4;
            int subsamp;
            unsigned long jpegSize = static_cast<unsigned long>(size);
            if (tjDecompressHeader2(ftjHandle,reinterpret_cast<unsigned char*>(const_cast<std::byte*>( buffer)), jpegSize, &width, &height, &subsamp) != -1)
            {
                /*if (canvasWidth != 0 && canvasHeight != 0)
                    ComputeDesiredDimensions(canvasWidth, canvasHeight, width, height);*/

                uint64_t imageDataSize = static_cast<uint64_t>(width) * height * bytesPerPixel;
                auto imageItem = std::make_shared<ImageItem>();
                imageItem->itemType = ImageItemType::Image;
                imageItem->data.Allocate(static_cast<size_t>(imageDataSize));


                auto resultCode = tjDecompress2(ftjHandle, const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(buffer)), jpegSize, reinterpret_cast<unsigned char*>(imageItem->data.data()), width, width * bytesPerPixel, height, TJPF_RGBA, 0);
                
                if (resultCode == 0 || tjGetErrorCode(ftjHandle) == TJERR_WARNING)
                {
                    imageItem->descriptor.texelFormatDecompressed = TexelFormat::I_R8_G8_B8_A8;
                    imageItem->descriptor.texelFormatStorage = TexelFormat::I_R8_G8_B8;
                    imageItem->descriptor.width = width;
                    imageItem->descriptor.height = height;
                    imageItem->descriptor.rowPitchInBytes = bytesPerPixel * width;
                    out_image = std::make_shared<Image>(imageItem, ImageItemType::Unknown);
                    result = ImageResult::Success;
                }
            }

            //tjDestroy(ftjHandle);
            return result;
        }


        int GetPixelFormat(TexelFormat texelFormat)
        {
            switch (texelFormat)
            {
            case TexelFormat::I_R8_G8_B8_A8:
                return TJPF_RGBA;
            case TexelFormat::I_R8_G8_B8:
                return TJPF_RGB;
            default:
                return TJPF_UNKNOWN;
            }
        }

        ImageResult Encode(const ImageSharedPtr& image, const IMCodec::Parameters& params, LLUtils::Buffer& encodedBuffer) override
        {
            const int pixelFormat = GetPixelFormat(image->GetTexelFormat());
            ImageResult result = ImageResult::UnknownError;

            if (pixelFormat != TJPF_UNKNOWN)
            {
                tjhandle compressHandle = tjInitCompress();
                //const auto maxBufferSize = tjBufSize(image->GetWidth(), image->GetHeight(), 0);

                unsigned long tjBufferSize = image->GetWidth() * image->GetHeight();
                auto tjBuffer = tjAlloc(tjBufferSize);

                unsigned long jpegSize = tjBufferSize;

                if (
                    tjCompress2(compressHandle, reinterpret_cast<const unsigned char*>(image->GetBuffer())
                        , image->GetWidth(), image->GetRowPitchInBytes()
                        , image->GetHeight(), pixelFormat, &tjBuffer
                        , &jpegSize, 0, 80, TJFLAG_ACCURATEDCT) == 0)
                {

                    encodedBuffer = { reinterpret_cast<const std::byte*>(tjBuffer) , static_cast<size_t>(jpegSize) };
                    result = ImageResult::Success;
                }

                tjFree(tjBuffer);
                tjDestroy(compressHandle);
                
            }
            return result;
        }

        ImageResult GetEncoderParameters(ListParameterDescriptors& out_encodeParameters) override 
        {
            out_encodeParameters = { {LLUTILS_TEXT("quality"),LLUTILS_TEXT("double"),LLUTILS_TEXT("Jpeg compression quality")
            ,LLUTILS_TEXT("Higher value means better quality"),LLUTILS_TEXT("80"),LLUTILS_TEXT("0"),LLUTILS_TEXT("100")} };
            return ImageResult::Success;
        }
    };
}
