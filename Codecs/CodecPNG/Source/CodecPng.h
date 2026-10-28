#pragma once

#include <IImagePlugin.h>
#include <Image.h>
#include <png.h>

#include <cstdint>
#include <bit>
#include <csetjmp>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>
#include <span>
#include <vector>

#ifndef PNG_READ_APNG_SUPPORTED
    #error CodecPNG requires libpng APNG read support
#endif

namespace IMCodec
{
    class CodecPNG : public IImagePlugin
    {
      private:

        // Own resources outside Call's jump boundary. Its operations contain only
        // libpng calls and trivial locals, so errors never skip C++ destructors.
        struct PngSession
        {
            png_struct* png = nullptr;
            png_info* info  = nullptr;
            bool writing;
            bool allocationFailed = false;
            std::span<const std::byte> input;
            size_t position = 0;
            std::vector<std::byte> output;

            explicit PngSession(bool write) : writing(write)
            {
                png = writing
                          ? png_create_write_struct_2(PNG_LIBPNG_VER_STRING, this, Error, Warning, this, Allocate, Free)
                          : png_create_read_struct_2(PNG_LIBPNG_VER_STRING, this, Error, Warning, this, Allocate, Free);
                if (png)
                    info = png_create_info_struct(png);
            }
            PngSession(const PngSession&)            = delete;
            PngSession& operator=(const PngSession&) = delete;
            ~PngSession()
            {
                if (writing)
                    png_destroy_write_struct(&png, &info);
                else
                    png_destroy_read_struct(&png, &info, nullptr);
            }

            template <class Operation>
            bool Call(Operation operation)
            {
                if (setjmp(png_jmpbuf(png)))
                    return false;
                operation();
                return true;
            }
            bool ReadRows(std::byte* pixels, size_t pitch, uint32_t height)
            {
                return Call(
                    [&]
                    {
                        const int passes = png_set_interlace_handling(png);
                        for (int pass = 0; pass < passes; ++pass)
                            for (size_t y = 0; y < height; ++y)
                                png_read_row(png, pixels ? reinterpret_cast<png_byte*>(pixels + y * pitch) : nullptr,
                                             nullptr);
                    });
            }
            ImageResult Failure() const
            {
                return allocationFailed ? ImageResult::UnknownError : ImageResult::FileIsCorrupted;
            }
            static void* Allocate(png_struct* png, png_alloc_size_t size)
            {
                auto* memory = std::malloc(size);
                if (!memory)
                    static_cast<PngSession*>(png_get_mem_ptr(png))->allocationFailed = true;
                return memory;
            }
            static void Free(png_struct*, void* memory) { std::free(memory); }
            [[noreturn]] static void Error(png_struct* png, const char*) { png_longjmp(png, 1); }
            static void Warning(png_struct*, const char*) {}
            static void Read(png_struct* png, png_byte* bytes, size_t count)
            {
                auto& state = *static_cast<PngSession*>(png_get_io_ptr(png));
                if (count > state.input.size() - state.position)
                    png_error(png, "Truncated PNG input");
                std::memcpy(bytes, state.input.data() + state.position, count);
                state.position += count;
            }
            static void Write(png_struct* png, png_byte* bytes, size_t count)
            {
                auto& state = *static_cast<PngSession*>(png_get_io_ptr(png));
                bool failed = false;
                try
                {
                    const auto* first = reinterpret_cast<const std::byte*>(bytes);
                    state.output.insert(state.output.end(), first, first + count);
                }
                catch (...)
                {
                    state.allocationFailed = true;
                    failed                 = true;
                }
                // Jump only after leaving the exception handler.
                if (failed)
                    png_error(png, "PNG output allocation failed");
            }
        };

        struct Frame
        {
            png_uint_32 width = 0, height = 0, x = 0, y = 0;
            png_uint_16 numerator = 0, denominator = 100;
            png_byte dispose = PNG_fcTL_DISPOSE_OP_NONE, blend = PNG_fcTL_BLEND_OP_SOURCE;
        };

        // APNG OVER is evaluated at the source depth with straight alpha. SOURCE
        // takes the direct row path and preserves even RGB under zero alpha.
        template <class Channel>
        static void Blend(std::byte* destination, const std::byte* source, size_t pixels)
        {
            constexpr uint64_t maximum = std::numeric_limits<Channel>::max();
            for (size_t i = 0; i < pixels; ++i)
            {
                Channel sourceChannels[4], destinationChannels[4];
                std::memcpy(sourceChannels, source + i * sizeof(sourceChannels), sizeof(sourceChannels));
                std::memcpy(destinationChannels, destination + i * sizeof(destinationChannels),
                            sizeof(destinationChannels));
                const uint64_t sourceAlpha = sourceChannels[3];
                if (sourceAlpha == maximum)
                    std::memcpy(destination + i * sizeof(sourceChannels), sourceChannels, sizeof(sourceChannels));
                else if (sourceAlpha != 0)
                {
                    const auto destinationWeight = uint64_t{destinationChannels[3]} * (maximum - sourceAlpha);
                    // Keep coverage scaled until the final division to avoid losing
                    // precision when converting the composed color back to straight alpha.
                    const auto combinedAlphaNumerator = sourceAlpha * maximum + destinationWeight;
                    for (size_t c = 0; c < 3; ++c)
                        destinationChannels[c] = static_cast<Channel>(
                            (uint64_t{sourceChannels[c]} * sourceAlpha * maximum +
                             uint64_t{destinationChannels[c]} * destinationWeight + combinedAlphaNumerator / 2) /
                            combinedAlphaNumerator);
                    destinationChannels[3] = static_cast<Channel>((combinedAlphaNumerator + maximum / 2) / maximum);
                    std::memcpy(destination + i * sizeof(destinationChannels), destinationChannels,
                                sizeof(destinationChannels));
                }
            }
        }

        using ColorInfo = ImageItem::ColorInfo;

        static ImageSharedPtr CreateImage(const ImageDescriptor& descriptor, const ColorInfo& colorInfo)
        {
            auto item        = std::make_shared<ImageItem>();
            item->itemType   = ImageItemType::Image;
            item->descriptor = descriptor;
            item->colorInfo  = colorInfo;
            item->data.Allocate(size_t{descriptor.rowPitchInBytes} * descriptor.height);
            return std::make_shared<Image>(item, ImageItemType::Unknown);
        }

        static ColorInfo ReadColorInfo(PngSession& state, bool alpha)
        {
            ColorInfo result;
            result.alphaMode      = alpha ? ColorInfo::AlphaMode::Straight : ColorInfo::AlphaMode::Opaque;
            png_fixed_point gamma = 0;
            if (png_get_gAMA_fixed(state.png, state.info, &gamma))
            {
                result.gamma            = gamma / 100000.0;
                result.transferFunction = gamma == 100000 ? ColorInfo::TransferFunction::Linear
                                                          : ColorInfo::TransferFunction::Gamma;
            }
            int intent = 0;
            if (png_get_sRGB(state.png, state.info, &intent))
            {
                result.srgbRenderingIntent = static_cast<ColorInfo::RenderingIntent>(intent);
                result.encoding            = ColorInfo::Encoding::SRGB;
                result.transferFunction    = ColorInfo::TransferFunction::SRGB;
            }
            char* name        = nullptr;
            png_byte* profile = nullptr;
            png_uint_32 size  = 0;
            if (png_get_iCCP(state.png, state.info, &name, nullptr, &profile, &size))
            {
                const auto* first       = reinterpret_cast<const std::byte*>(profile);
                result.iccProfile       = std::make_shared<const std::vector<std::byte>>(first, first + size);
                result.encoding         = ColorInfo::Encoding::ICCProfile;
                result.transferFunction = ColorInfo::TransferFunction::Unknown;
            }
            return result;
        }

        static void WriteColorInfo(PngSession& state, const ColorInfo& colorInfo)
        {
            if (colorInfo.gamma)
            {
                constexpr double gammaScale = 100000;
                const auto gamma            = std::round(*colorInfo.gamma * gammaScale);
                if (!std::isfinite(gamma) || gamma < 1 || gamma > std::numeric_limits<png_fixed_point>::max())
                    png_error(state.png, "Invalid PNG gamma");
                png_set_gAMA_fixed(state.png, state.info, static_cast<png_fixed_point>(gamma));
            }
            // Preserve source numbers and their interpretation. A profile carries
            // the color encoding in preference to an sRGB declaration.
            if (colorInfo.encoding == ColorInfo::Encoding::ICCProfile)
            {
                if (!colorInfo.iccProfile || colorInfo.iccProfile->empty() ||
                    colorInfo.iccProfile->size() > std::numeric_limits<png_uint_32>::max())
                    png_error(state.png, "Invalid PNG ICC profile");
                png_set_iCCP(state.png, state.info, "ICC profile", PNG_COMPRESSION_TYPE_BASE,
                             reinterpret_cast<const png_byte*>(colorInfo.iccProfile->data()),
                             static_cast<png_uint_32>(colorInfo.iccProfile->size()));
            }
            else if (colorInfo.encoding == ColorInfo::Encoding::SRGB)
            {
                const auto intent = static_cast<int>(
                    colorInfo.srgbRenderingIntent.value_or(ColorInfo::RenderingIntent::Perceptual));
                if (intent < 0 || intent >= PNG_sRGB_INTENT_LAST)
                    png_error(state.png, "Invalid PNG sRGB rendering intent");
                png_set_sRGB(state.png, state.info, intent);
            }
        }

        static ImageResult ReadImage(PngSession& state, ImageLoadFlags flags, ImageSharedPtr& decoded)
        {
            auto* png = state.png;
            if (!state.Call(
                    [&]
                    {
                        png_set_read_fn(png, &state, PngSession::Read);
                        // Let ImageCodec's storage limit decide which dimensions fit.
                        png_set_user_limits(png, PNG_UINT_31_MAX, PNG_UINT_31_MAX);
                        png_set_crc_action(png, PNG_CRC_ERROR_QUIT, PNG_CRC_ERROR_QUIT);
                        png_read_info(png, state.info);
                    }))
                return state.Failure();

            auto* info             = state.info;
            const auto width       = png_get_image_width(png, info);
            const auto height      = png_get_image_height(png, info);
            const auto sourceDepth = png_get_bit_depth(png, info);
            const auto color       = png_get_color_type(png, info);
            const bool animated    = png_get_valid(png, info, PNG_INFO_acTL) != 0;
            if (!width || !height)
                return ImageResult::FileIsCorrupted;

            const bool alpha            = (color & PNG_COLOR_MASK_ALPHA) || png_get_valid(png, info, PNG_INFO_tRNS);
            const bool gray             = color == PNG_COLOR_TYPE_GRAY || color == PNG_COLOR_TYPE_GRAY_ALPHA;
            const bool rgba             = animated || alpha;
            const unsigned channels     = rgba ? 4 : gray ? 1 : 3;
            const unsigned channelBytes = sourceDepth == 16 ? 2 : 1;
            const size_t pixelBytes     = channels * channelBytes;
            constexpr auto maximum      = std::numeric_limits<uint32_t>::max();
            if (uint64_t{width} * pixelBytes > maximum || height > maximum / (uint64_t{width} * pixelBytes))
                return ImageResult::FormatNotSupported;
            const auto pitch = static_cast<uint32_t>(width * pixelBytes);
            // Rows of this table correspond to gray, RGB, and RGBA output.
            static constexpr TexelFormat formats[][2] = {{TexelFormat::I_X8, TexelFormat::I_X16},
                                                         {TexelFormat::I_R8_G8_B8, TexelFormat::I_R16_G16_B16},
                                                         {TexelFormat::I_R8_G8_B8_A8, TexelFormat::I_R16_G16_B16_A16}};
            const auto format                         = formats[rgba ? 2 : gray ? 0 : 1][channelBytes - 1];

            // Read declarations before update_info normalizes libpng's gamma state.
            const auto colorInfo = ReadColorInfo(state, rgba);

            // Configure storage expansion only. No gamma, profile, alpha association,
            // significant-bit rescaling, or precision reduction changes the samples.
            if (!state.Call(
                    [&]
                    {
                        if (color == PNG_COLOR_TYPE_PALETTE)
                            png_set_palette_to_rgb(png);
                        if (gray && sourceDepth < 8)
                            png_set_expand_gray_1_2_4_to_8(png);
                        if (png_get_valid(png, info, PNG_INFO_tRNS))
                            png_set_tRNS_to_alpha(png);
                        if (rgba && gray)
                            png_set_gray_to_rgb(png);
                        if (rgba && !alpha)
                            png_set_add_alpha(png, channelBytes == 2 ? 65535 : 255, PNG_FILLER_AFTER);
                        if constexpr (std::endian::native == std::endian::little)
                            if (channelBytes == 2)
                                png_set_swap(png);
                        png_set_interlace_handling(png);
                        png_read_update_info(png, info);
                    }))
                return state.Failure();
            if (png_get_rowbytes(png, info) != pitch)
                return ImageResult::FormatNotSupported;

            const ImageDescriptor descriptor{width, height, pitch, format, format};
            if (animated)
                return ReadAnimation(state, descriptor, colorInfo, flags, decoded);

            auto image = CreateImage(descriptor, colorInfo);
            if (!state.ReadRows(image->GetImageItem()->data.data(), pitch, height) ||
                !state.Call([&] { png_read_end(png, info); }))
                return state.Failure();
            decoded = std::move(image);
            return ImageResult::Success;
        }

        // Published frames double as the previous canvas. Disposal is applied to
        // the next output, preserving old frames without another full-size canvas.
        static ImageResult ReadAnimation(PngSession& state, const ImageDescriptor& descriptor,
                                         const ColorInfo& colorInfo, ImageLoadFlags flags, ImageSharedPtr& decoded)
        {
            auto* png               = state.png;
            auto* info              = state.info;
            const auto width        = descriptor.width;
            const auto height       = descriptor.height;
            const auto pitch        = descriptor.rowPitchInBytes;
            const auto channelBytes = descriptor.texelFormatDecompressed == TexelFormat::I_R16_G16_B16_A16 ? 2u : 1u;
            const size_t pixelBytes = 4 * channelBytes;
            const size_t imageBytes = size_t{pitch} * height;
            const bool hidden       = png_get_first_frame_is_hidden(png, info);
            const bool firstOnly    = (flags & ImageLoadFlags::LoadOnlyFirstImage) != ImageLoadFlags::None;
            const auto frames       = png_get_num_frames(png, info) - static_cast<unsigned>(hidden);
            if (!frames)
                return ImageResult::FileIsCorrupted;
            if (!firstOnly && frames > std::numeric_limits<uint16_t>::max())
                return ImageResult::FormatNotSupported;

            std::vector<std::byte> decodedFramePixels, savedRectanglePixels;
            ImageSharedPtr container, previousImage;
            Frame previousFrameControl;
            if (!firstOnly)
            {
                auto item       = std::make_shared<ImageItem>();
                item->itemType  = ImageItemType::Container;
                item->colorInfo = colorInfo;
                container       = std::make_shared<Image>(item, ImageItemType::AnimationFrame);
                container->SetNumSubImages(static_cast<uint16_t>(frames));
            }
            // A hidden fallback does not contribute pixels to the animation.
            if (hidden && !state.ReadRows(nullptr, 0, height))
                return state.Failure();

            const auto count = firstOnly ? 1u : frames;
            for (uint32_t index = 0; index < count; ++index)
            {
                Frame frame;
                if (!state.Call([&] { png_read_frame_head(png, info); }))
                    return state.Failure();
                // The frame reader validates dimensions, offsets, and operations.
                if (!png_get_next_frame_fcTL(png, info, &frame.width, &frame.height, &frame.x, &frame.y,
                                             &frame.numerator, &frame.denominator, &frame.dispose, &frame.blend))
                    return ImageResult::FileIsCorrupted;
                if (index == 0 && frame.dispose == PNG_fcTL_DISPOSE_OP_PREVIOUS)
                    frame.dispose = PNG_fcTL_DISPOSE_OP_BACKGROUND;

                auto current                                             = CreateImage(descriptor, colorInfo);
                current->GetImageItem()->animationData.delayMilliseconds = static_cast<uint32_t>(
                    uint64_t{frame.numerator} * 1000 / (frame.denominator ? frame.denominator : 100));
                auto* pixels            = current->GetImageItem()->data.data();
                const size_t framePitch = frame.width * pixelBytes;
                const bool fullSource   = frame.width == width && frame.height == height &&
                                          frame.blend == PNG_fcTL_BLEND_OP_SOURCE;
                const bool savePrevious = frame.dispose == PNG_fcTL_DISPOSE_OP_PREVIOUS && index + 1 < count;
                if (!fullSource || savePrevious)
                {
                    if (previousImage)
                        std::memcpy(pixels, previousImage->GetBuffer(), imageBytes);
                    else
                        std::memset(pixels, 0, imageBytes);
                    if (previousImage && previousFrameControl.dispose != PNG_fcTL_DISPOSE_OP_NONE)
                        for (size_t y = 0; y < previousFrameControl.height; ++y)
                        {
                            auto* row        = pixels + (y + previousFrameControl.y) * pitch +
                                               previousFrameControl.x * pixelBytes;
                            const auto bytes = previousFrameControl.width * pixelBytes;
                            if (previousFrameControl.dispose == PNG_fcTL_DISPOSE_OP_BACKGROUND)
                                std::memset(row, 0, bytes);
                            else
                                std::memcpy(row, savedRectanglePixels.data() + y * bytes, bytes);
                        }
                }
                if (savePrevious)
                {
                    savedRectanglePixels.resize(framePitch * frame.height);
                    for (size_t y = 0; y < frame.height; ++y)
                        std::memcpy(savedRectanglePixels.data() + y * framePitch,
                                    pixels + (y + frame.y) * pitch + frame.x * pixelBytes, framePitch);
                }
                const bool over = frame.blend == PNG_fcTL_BLEND_OP_OVER;
                if (over)
                    decodedFramePixels.resize(framePitch * frame.height);
                auto* target = over ? decodedFramePixels.data() : pixels + frame.y * pitch + frame.x * pixelBytes;
                if (!state.ReadRows(target, over ? framePitch : pitch, frame.height))
                    return state.Failure();
                if (over)
                    for (size_t y = 0; y < frame.height; ++y)
                    {
                        auto* row          = pixels + (y + frame.y) * pitch + frame.x * pixelBytes;
                        const auto* source = decodedFramePixels.data() + y * framePitch;
                        if (channelBytes == 2)
                            Blend<uint16_t>(row, source, frame.width);
                        else
                            Blend<uint8_t>(row, source, frame.width);
                    }
                previousImage = std::move(current);
                if (container)
                    container->SetSubImage(static_cast<uint16_t>(index), previousImage);
                previousFrameControl = frame;
            }
            // First-only loading deliberately leaves the remaining animation unread.
            if (!firstOnly && !state.Call([&] { png_read_end(png, info); }))
                return state.Failure();
            decoded = container ? std::move(container) : std::move(previousImage);
            return ImageResult::Success;
        }

      public:

        const PluginProperties& GetPluginProperties() override
        {
            static const PluginProperties properties{
                {0x22a82f61, 0x4c92, 0x432e, {0x93, 0xeb, 0x63, 0x5e, 0x24, 0x54, 0x53, 0x6}},
                CodecCapabilities::Decode | CodecCapabilities::Encode,
                LLUTILS_TEXT("PNG Codec"),
                {{LLUTILS_TEXT("Portable Network Graphics"), {LLUTILS_TEXT("png")}}}};
            return properties;
        }

        ImageResult Decode(const std::byte* buffer, size_t size, ImageLoadFlags flags,
                           [[maybe_unused]] const Parameters& params, ImageSharedPtr& out_image) override
        {
            if (!buffer || !size)
                return ImageResult::BadParameters;
            try
            {
                PngSession state(false);
                if (!state.info)
                    return ImageResult::UnknownError;
                state.input = {buffer, size};
                ImageSharedPtr decoded;
                const auto result = ReadImage(state, flags, decoded);
                if (result == ImageResult::Success)
                    out_image = std::move(decoded);
                return result;
            }
            catch (const std::bad_alloc&)
            {
                return ImageResult::UnknownError;
            }
        }

        ImageResult Encode(const ImageSharedPtr& image, [[maybe_unused]] const Parameters& params,
                           LLUtils::Buffer& encodedBuffer) override
        {
            if (!image || !image->GetWidth() || !image->GetHeight() || !image->GetBuffer())
                return ImageResult::BadParameters;
            const auto format = image->GetTexelFormat();
            if (format != TexelFormat::I_R8_G8_B8 && format != TexelFormat::I_R8_G8_B8_A8)
                return ImageResult::FormatNotSupported;
            const auto& colorInfo = image->GetImageItem()->colorInfo;
            // Do not discard pending curves or label linear samples as display-encoded sRGB.
            if (colorInfo.RequiresTransferBakeForEncoding())
                return ImageResult::FormatNotSupported;
            // PNG stores straight alpha. Unassociating here would change samples
            // and cannot recover colors that were lost under zero alpha.
            if (colorInfo.alphaMode == ColorInfo::AlphaMode::Premultiplied)
                return ImageResult::FormatNotSupported;
            // This encoder writes RGB layouts. A grayscale profile can accompany
            // a decoded, expanded gray image, but cannot label an RGB PNG stream.
            constexpr size_t iccColorSpaceOffset = 16;
            constexpr size_t iccColorSpaceBytes  = 4;
            if (colorInfo.encoding == ColorInfo::Encoding::ICCProfile && colorInfo.iccProfile &&
                colorInfo.iccProfile->size() >= iccColorSpaceOffset + iccColorSpaceBytes &&
                std::memcmp(colorInfo.iccProfile->data() + iccColorSpaceOffset, "RGB ", iccColorSpaceBytes) != 0)
                return ImageResult::FormatNotSupported;
            const unsigned channels = format == TexelFormat::I_R8_G8_B8 ? 3 : 4;
            const uint64_t rowBytes = uint64_t{image->GetWidth()} * channels;
            const uint64_t required = uint64_t{image->GetHeight() - 1} * image->GetRowPitchInBytes() + rowBytes;
            if (rowBytes > image->GetRowPitchInBytes() || required > image->GetImageItem()->data.size())
                return ImageResult::BadParameters;
            try
            {
                PngSession state(true);
                if (!state.info)
                    return ImageResult::UnknownError;
                if (!state.Call(
                        [&]
                        {
                            png_set_write_fn(state.png, &state, PngSession::Write, nullptr);
                            png_set_IHDR(state.png, state.info, image->GetWidth(), image->GetHeight(), 8,
                                         channels == 3 ? PNG_COLOR_TYPE_RGB : PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE,
                                         PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
                            png_set_compression_level(state.png, 6);
                            WriteColorInfo(state, colorInfo);
                            png_write_info(state.png, state.info);
                            for (size_t y = 0; y < image->GetHeight(); ++y)
                                png_write_row(state.png, reinterpret_cast<const png_byte*>(
                                                             image->GetBuffer() + y * image->GetRowPitchInBytes()));
                            png_write_end(state.png, state.info);
                        }))
                    return state.allocationFailed ? ImageResult::UnknownError : ImageResult::BadParameters;
                LLUtils::Buffer encoded(state.output.size());
                if (!encoded.data())
                    return ImageResult::UnknownError;
                std::memcpy(encoded.data(), state.output.data(), state.output.size());
                encodedBuffer = std::move(encoded);
                return ImageResult::Success;
            }
            catch (const std::bad_alloc&)
            {
                return ImageResult::UnknownError;
            }
        }
    };
}  // namespace IMCodec
