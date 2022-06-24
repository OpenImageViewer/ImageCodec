#pragma once

#include <Image.h>
#include <png.h>

#include <LLUtils/BitFlags.h>

typedef struct png_control
{
    png_structp png_ptr;
    png_infop   info_ptr;
    png_voidp   error_buf;           /* Always a jmp_buf at present. */

    png_const_bytep memory;          /* Memory buffer. */
    size_t          size;            /* Size of the memory buffer. */

    unsigned int for_write : 1; /* Otherwise it is a read structure */
    unsigned int owned_file : 1; /* We own the file in io_ptr */
} png_control;

namespace IMCodec
{
    class CodecPNG : public IImagePlugin
    {
    private:
        static constexpr int PNG_COLOR_TYPE_UNKNOWN = -1;
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

        const PluginProperties& GetPluginProperties() override
        {
            static PluginProperties pluginProperties =
            {
                CodecCapabilities::Decode | CodecCapabilities::Encode
                , L"PNG Codec"
                ,
                {
                    {
                        { L"Portable Network Graphics"}
                       ,{ L"png"}
                    }
                }
            };
            return pluginProperties;
        }

        int readStatuscallback(png_structp pngstruct, png_uint_32 status)
        {

        }

        void pngError(png_const_structrp png_ptr,
            png_const_charp error_message)
        {

        }

        TexelFormat ResolveTexelFormat(png_uint_32 pngFormat, unsigned int channelSize)
        {
            TexelFormat textFormat = TexelFormat::UNKNOWN;
            const bool isSingleChannel = channelSize == 1;
            switch (pngFormat)
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
                textFormat = isSingleChannel ? TexelFormat::I_R8_G8_B8_A8 : TexelFormat::I_R16_G16_B16_A16;
                break;
            case  PNG_FORMAT_FLAG_COLOR | PNG_FORMAT_FLAG_COLORMAP | PNG_FORMAT_FLAG_LINEAR:
            case  PNG_FORMAT_FLAG_COLOR | PNG_FORMAT_FLAG_COLORMAP:
                textFormat = isSingleChannel ? TexelFormat::I_R8_G8_B8 : TexelFormat::I_R16_G16_B16;
                break;
            default:
                LL_EXCEPTION_NOT_IMPLEMENT("Png image format not supported");
            }

            return textFormat;
        }

        void ExtractColorMap(LLUtils::Buffer& dest, const LLUtils::Buffer& mappedBuffer, const LLUtils::Buffer& colorMap, uint32_t targetPixelSize
            , uint32_t width, uint32_t height)
        {
            for (size_t pixelIndex = 0; pixelIndex < width * height; pixelIndex++)
            {
                const size_t colorIndex = ((uint8_t*)(mappedBuffer.data()))[pixelIndex];
                const size_t sourcePos = colorIndex * targetPixelSize;
                const size_t destPos = pixelIndex * targetPixelSize;

                memcpy(reinterpret_cast<uint8_t*>(dest.data()) + destPos
                    , reinterpret_cast<const uint8_t*>(colorMap.data()) + sourcePos, targetPixelSize);
            }
        }

        //Base abstract methods
        ImageResult Decode(const std::byte* buffer, std::size_t size, [[maybe_unused]] ImageLoadFlags loadFlags, const Parameters& params, ImageSharedPtr& out_image) override
        {
            using namespace std;
            ImageResult result = ImageResult::Fail;
            png_image image{};
            /* Initialize the 'png_image' structure. */
            image.version = PNG_IMAGE_VERSION;

            if (png_image_begin_read_from_memory(&image, buffer, size) != 0)
            {
                //The size of the image buffer.
                const auto version = image.version;
                const auto sourceImageSize = PNG_IMAGE_SIZE(image);
                const auto sizeofChannel = static_cast<unsigned int>(PNG_IMAGE_PIXEL_COMPONENT_SIZE(image.format));
                const auto sourceRowPitchInComponents = PNG_IMAGE_ROW_STRIDE(image);
                const auto sourceRowPitchInBytes = sourceRowPitchInComponents * sizeofChannel;
                const auto canvaswidth = image.width;
                const auto canvasheight = image.height;

                //Number of channels of the target image.
                auto numChannles = PNG_IMAGE_SAMPLE_CHANNELS(image.format);
                //Number of channels as found on disk, for color mapped image the value is 1
                auto numSourceChannels = PNG_IMAGE_PIXEL_CHANNELS(image.format);
                auto targetPixelSize = sizeofChannel * numChannles;
                auto sourcePixelSize = sizeofChannel * numSourceChannels;
                LLUtils::BitFlags<PngFormatFlags> formatFlags(static_cast<PngFormatFlags>(image.format));
                bool isColorMapped = formatFlags.test(PngFormatFlags::ColorMap);
                TexelFormat texelFormat = ResolveTexelFormat(image.format, sizeofChannel);

                auto targetRowPitchInBytes = isColorMapped ? targetPixelSize * sourceRowPitchInBytes : sourceRowPitchInBytes; // might change if it's a color mapped png.
                auto targetImageSize = isColorMapped ? targetRowPitchInBytes * canvasheight : sourceImageSize;

                png_controlp control = image.opaque;
                png_structrp png_ptr = control->png_ptr;
                png_inforp info_ptr = control->info_ptr;
                png_uint_32 numFrames{};
                png_uint_32 numPlays{};

                png_get_acTL(png_ptr, info_ptr, &numFrames, &numPlays);

                if (numFrames > 1)
                {
                    png_uint_32 framewidth{};
                    png_uint_32 frameheight{};
                    png_uint_32 frametop{};
                    png_uint_32 frameleft{};
                    png_uint_16 delaynom{};
                    png_uint_16 delaydenom{};
                    png_byte disposeOp{};
                    png_byte blendOp{};

                    png_byte colorType = png_get_color_type(png_ptr, info_ptr);

                    if (colorType == PNG_COLOR_TYPE_RGB)
                    {
                        numSourceChannels = 3;
                        sourcePixelSize = sizeofChannel * numSourceChannels;
                        targetPixelSize = sourcePixelSize;
                        texelFormat = TexelFormat::I_R8_G8_B8;
                        targetRowPitchInBytes = targetPixelSize * canvaswidth;
                    }

                    png_get_acTL(png_ptr, info_ptr, &numFrames, &numPlays);

                    std::vector<LLUtils::Buffer> frameBuffers(numFrames);
                    auto imageItem = std::make_shared<ImageItem>();
                    imageItem->itemType = ImageItemType::Container;
                    out_image = std::make_shared<Image>(imageItem, ImageItemType::AnimationFrame);
                    out_image->SetNumSubImages(numFrames);



                    for (auto frameIndex = 0; frameIndex < numFrames; frameIndex++)
                    {
                        auto& currentFrameBuffer = frameBuffers.at(frameIndex);
                        currentFrameBuffer.Allocate(canvasheight * targetRowPitchInBytes);
                        memset(currentFrameBuffer.data(), 0, currentFrameBuffer.size());

                        png_read_frame_head(png_ptr, info_ptr);

                        if (png_get_valid(png_ptr, info_ptr, PNG_INFO_fcTL))
                        {
                            auto res = png_get_next_frame_fcTL(png_ptr, info_ptr, &framewidth, &frameheight, &frameleft, &frametop, &delaynom, &delaydenom, &disposeOp, &blendOp);


                            LLUtils::Buffer readBuffer(sourceRowPitchInBytes);
                            png_bytep row_buf = (png_bytep)readBuffer.data();
                            for (auto y = 0; y < frameheight; y++)
                            {
                                const size_t rowBytes = framewidth * targetPixelSize;
                                png_read_rows(png_ptr, (png_bytepp)&row_buf, nullptr, 1);

                                auto lineOffset = (y + frametop) * targetRowPitchInBytes + frameleft * targetPixelSize;
                                currentFrameBuffer.Write(reinterpret_cast<const std::byte*>(row_buf), lineOffset, rowBytes);
                            }
                        }


                        auto frameImageItem = std::make_shared<ImageItem>();
                        frameImageItem->itemType = ImageItemType::Image;
                        frameImageItem->descriptor.rowPitchInBytes = targetRowPitchInBytes;
                        frameImageItem->descriptor.width = canvaswidth;
                        frameImageItem->descriptor.height = canvasheight;
                        frameImageItem->descriptor.texelFormatDecompressed = texelFormat;
                        frameImageItem->data = std::move(currentFrameBuffer);
                        frameImageItem->animationData.delayMilliseconds = delaynom * 1000 / delaydenom;
                        out_image->SetSubImage(frameIndex, std::make_shared<Image>(frameImageItem, ImageItemType::Unknown));

                    }
                    result = ImageResult::Success;
                }
                else
                {
                    auto imageItem = std::make_shared<ImageItem>();
                    imageItem->itemType = ImageItemType::Image;

                    imageItem->descriptor.rowPitchInBytes = targetRowPitchInBytes;
                    imageItem->descriptor.width = image.width;
                    imageItem->descriptor.height = image.height;
                    imageItem->descriptor.texelFormatDecompressed = texelFormat;
                    imageItem->data.Allocate(targetImageSize);

                    LLUtils::Buffer colorMap;
                    LLUtils::Buffer mappedBuffer;

                    if (isColorMapped)
                    {
                        colorMap.Allocate(PNG_IMAGE_COLORMAP_SIZE(image));
                        mappedBuffer.Allocate(sourceImageSize);
                    }

                    auto& pngReadBuffer = mappedBuffer != nullptr ? mappedBuffer : imageItem->data;

                    if (png_image_finish_read(&image, nullptr/*background*/, pngReadBuffer.data(),
                        sourceRowPitchInComponents, colorMap.data()) != 0)
                    {
                        if (colorMap != nullptr)
                            ExtractColorMap(imageItem->data, mappedBuffer, colorMap, targetPixelSize, canvaswidth, canvasheight);

                        out_image = std::make_shared<Image>(imageItem, ImageItemType::Unknown);
                        result = ImageResult::Success;
                    }
                }
            }
            return result;
        }

        static void PngWriteCallback(png_structp  png_ptr, png_bytep data, png_size_t length)
        {
            std::vector<uint8_t>* p = (std::vector<uint8_t>*)png_get_io_ptr(png_ptr);
            p->insert(p->end(), data, data + length);
        }


        struct TPngDestructor {
            png_struct* p;
            TPngDestructor(png_struct* p) : p(p) {}
            ~TPngDestructor() { if (p) { png_destroy_write_struct(&p, NULL); } }
        };



        int GetPixelFormat(TexelFormat texelFormat)
        {
            switch (texelFormat)
            {
            case TexelFormat::I_R8_G8_B8_A8:
                return PNG_COLOR_TYPE_RGBA;
            case TexelFormat::I_R8_G8_B8:
                return PNG_COLOR_TYPE_RGB;
            default:
                return PNG_COLOR_TYPE_UNKNOWN;
            }
        }

        ImageResult Encode([[maybe_unused]] const ImageSharedPtr& image, const Parameters& EncodeParametrs
            , [[maybe_unused]] LLUtils::Buffer& encodedBuffer) override
        {

            auto texelFormat = GetPixelFormat(image->GetTexelFormat());

            if (texelFormat != PNG_COLOR_TYPE_UNKNOWN)
            {
                constexpr int compressionLevel = 6;
                png_structp p = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
                png_infop info_ptr = png_create_info_struct(p);

                TPngDestructor destroyPng(p);

                png_set_IHDR(p, info_ptr, image->GetWidth(), image->GetHeight(), 8
                    , texelFormat
                    , PNG_INTERLACE_NONE
                    , PNG_COMPRESSION_TYPE_DEFAULT
                    , PNG_FILTER_TYPE_DEFAULT);

                png_set_compression_level(p, compressionLevel);


                std::vector<uint8_t> pngEncodedBuffer;

                std::vector<uint8_t*> rows(image->GetHeight());
                for (size_t y = 0; y < image->GetHeight(); ++y)
                    rows[y] = (uint8_t*)image->GetBuffer() + y * image->GetRowPitchInBytes();

                png_set_rows(p, info_ptr, rows.data());
                png_set_write_fn(p, &pngEncodedBuffer, PngWriteCallback, nullptr);
                png_write_png(p, info_ptr, PNG_TRANSFORM_IDENTITY, nullptr);
                encodedBuffer = { reinterpret_cast<const std::byte*>(pngEncodedBuffer.data()) , pngEncodedBuffer.size() };

                return ImageResult::Success;
            }
            else
                return ImageResult::FormatNotSupported;
        }

    };
}
