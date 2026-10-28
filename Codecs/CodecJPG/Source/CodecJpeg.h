#pragma once

#include <IImagePlugin.h>
#include <Image.h>
#include <turbojpeg.h>
#include <LLUtils/Logging/Logger.h>
#include <array>
#include <any>
#include <optional>

#include <algorithm>
#include <bit>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <new>

namespace IMCodec
{
    class CodecJpeg : public IImagePlugin
    {
      private:

        using JpegHandle = std::unique_ptr<void, decltype(&tj3Destroy)>;
        using LogLevel   = LLUtils::LogLevel;
        static LLUtils::LogCategory Category()
        {
            static const auto category = LLUtils::Logger::RegisterCategory("JPEG");
            return category;
        }
        static constexpr std::array<std::string_view, 7> ResultNames{"Success",        "FileIsCorrupted",
                                                                     "NotImplemented", "FormatNotSupported",
                                                                     "BadParameters",  "NotFound",
                                                                     "UnknownError"};

        // The first two format arguments always come from the result being returned.
        // Keep per-operation details at the call site and let Logger format the complete entry once.
        template <class... Args>
        static ImageResult ReportResult(LogLevel level, ImageResult result,
                                        std::format_string<std::string_view, int, Args...> format,
                                        Args&&... args) noexcept
        {
            LL_LOG(Category(), level, format, std::string_view(ResultNames[static_cast<size_t>(result)]),
                   std::to_underlying(result), std::forward<Args>(args)...);
            return result;
        }

        // Capture TurboJPEG's mutable last-error state before another library call can replace it.
        // Return the same application result that is recorded, keeping translation beside the failed action.
        static ImageResult ReportJpegError(tjhandle handle, std::string_view operation, ImageResult result,
                                           std::optional<int> status = std::nullopt) noexcept
        {
            const int severity  = tj3GetErrorCode(handle);
            const char* message = tj3GetErrorStr(handle);
            try
            {
                const auto level = result == ImageResult::Success ? LogLevel::Trace
                                   : severity == TJERR_WARNING    ? LogLevel::Warning
                                                                  : LogLevel::Error;
                LL_LOG(Category(), level, "[JPEG] {}: status={}, severity={}({}), result={}({}): {}", operation,
                       status ? std::to_string(*status) : "null handle",
                       severity == TJERR_WARNING ? "TJERR_WARNING" : "TJERR_FATAL", severity,
                       ResultNames.at(static_cast<size_t>(result)), std::to_underlying(result), message);
            }
            catch (...)
            {
                // Failure to initialize logging must not alter the codec's error result.
            }
            return result;
        }

        static bool HasAdobeMarker(const unsigned char* buffer, size_t size)
        {
            // Only inspect marker segments before the first scan. Searching arbitrary bytes could
            // mistake an embedded thumbnail or compressed image data for this image's APP14 marker.
            size_t offset = 2;  // Skip SOI, already checked by TurboJPEG.
            while (offset < size)
            {
                if (buffer[offset++] != 0xff)
                    return false;
                while (offset < size && buffer[offset] == 0xff)
                    ++offset;
                if (offset == size)
                    return false;

                const unsigned char marker = buffer[offset++];
                if (marker == 0xda || marker == 0xd9)  // SOS / EOI
                    return false;
                if (marker == 0x01 || (marker >= 0xd0 && marker <= 0xd8))
                    continue;  // Standalone markers have no length.
                if (size - offset < 2)
                    return false;

                const size_t length = (static_cast<size_t>(buffer[offset]) << 8) | buffer[offset + 1];
                if (length < 2 || length > size - offset)
                    return false;
                if (marker == 0xee && length >= 14 && std::memcmp(buffer + offset + 2, "Adobe", 5) == 0)
                    return true;
                offset += length;
            }
            return false;
        }

      public:

        const PluginProperties& GetPluginProperties() override
        {
            static PluginProperties pluginProperties = {
                // {7798CB6A-5546-4EAD-BA95-A114C8520D0D}
                {0x7798cb6a, 0x5546, 0x4ead, {0xba, 0x95, 0xa1, 0x14, 0xc8, 0x52, 0xd, 0xd}},
                CodecCapabilities::Encode | CodecCapabilities::Decode,
                LLUTILS_TEXT("TurboJpeg"),
                {{{LLUTILS_TEXT("Joint Photographic Experts Group")}, {LLUTILS_TEXT("jpg"), LLUTILS_TEXT("jpeg")}}}};
            return pluginProperties;
        }

        ImageResult Decode(const std::byte* buffer, std::size_t size, [[maybe_unused]] ImageLoadFlags loadFlags,
                           [[maybe_unused]] const Parameters& params, ImageSharedPtr& out_image) override
        {
            LL_LOG(Category(), LogLevel::Trace, "[JPEG] Decode begin: inputBytes={}", size);
            if (buffer == nullptr || size == 0)
                return ReportResult(LogLevel::Warning, ImageResult::BadParameters,
                                    "[JPEG] Decode input validation: result={}({}), nullBuffer={}, inputBytes={}",
                                    buffer == nullptr, size);

            // JPEG tables survive a decoder reset. each file needs its own handle. The size_t API
            // also avoids truncating input lengths on Win64.
            JpegHandle handle(tj3Init(TJINIT_DECOMPRESS), tj3Destroy);
            if (!handle)
                return ReportJpegError(nullptr, "decode/tj3Init", ImageResult::UnknownError);
            // Reject substituted/truncated pixels, and retain ICC data without applying its transform.
            if (const int status = tj3Set(handle.get(), TJPARAM_STOPONWARNING, 1); status != 0)
                return ReportJpegError(handle.get(), "decode/tj3Set(STOPONWARNING)", ImageResult::UnknownError, status);
            if (const int status = tj3Set(handle.get(), TJPARAM_SAVEMARKERS, 4); status != 0)
                return ReportJpegError(handle.get(), "decode/tj3Set(SAVEMARKERS)", ImageResult::UnknownError, status);

            const auto* jpegBuffer = reinterpret_cast<const unsigned char*>(buffer);
            if (const int status = tj3DecompressHeader(handle.get(), jpegBuffer, size); status != 0)
                return ReportJpegError(handle.get(), "decode/tj3DecompressHeader", ImageResult::FileIsCorrupted,
                                       status);

            const int width             = tj3Get(handle.get(), TJPARAM_JPEGWIDTH);
            const int height            = tj3Get(handle.get(), TJPARAM_JPEGHEIGHT);
            const int colorSpace        = tj3Get(handle.get(), TJPARAM_COLORSPACE);
            const int precision         = tj3Get(handle.get(), TJPARAM_PRECISION);
            constexpr int bytesPerPixel = 4;
            // A tables-only stream can succeed without dimensions. ordinary headers are limited to 65500.
            if (width <= 0 || height <= 0)
                return ReportResult(LogLevel::Warning, ImageResult::FileIsCorrupted,
                                    "[JPEG] Decode dimensions: result={}({}), width={}, height={}", width, height);
            const uint64_t imageDataSize = static_cast<uint64_t>(width) * height * bytesPerPixel;
            // The image API retains 32-bit sizes. Include allocator rounding on 32-bit hosts.
            if (imageDataSize > std::numeric_limits<uint32_t>::max() ||
                imageDataSize > std::numeric_limits<size_t>::max() - LLUtils::AlignedAlloc::Alignment)
                return ReportResult(LogLevel::Info, ImageResult::FormatNotSupported,
                                    "[JPEG] Decode size limit: result={}({}), outputBytes={}", imageDataSize);
            if (precision != 8)
                return ReportResult(LogLevel::Info, ImageResult::FormatNotSupported,
                                    "[JPEG] Decode precision: result={}({}), precision={}", precision);

            struct ColorInfo
            {
                SourceColorSpace source;
                const char* name;
                bool cmyk;
            };
            static constexpr std::array colors{ColorInfo{SourceColorSpace::RGB, "RGB", false},
                                               ColorInfo{SourceColorSpace::YCbCr, "YCbCr", false},
                                               ColorInfo{SourceColorSpace::Gray, "Grayscale", false},
                                               ColorInfo{SourceColorSpace::CMYK, "CMYK", true},
                                               ColorInfo{SourceColorSpace::YCCK, "YCCK", true}};
            static_assert(TJCS_RGB == 0 && TJCS_YCbCr == 1 && TJCS_GRAY == 2 && TJCS_CMYK == 3 && TJCS_YCCK == 4);
            if (colorSpace < 0 || static_cast<size_t>(colorSpace) >= colors.size())
                return ReportResult(LogLevel::Info, ImageResult::FormatNotSupported,
                                    "[JPEG] Decode color space: result={}({}), colorSpace={}", colorSpace);
            const auto& color = colors[static_cast<size_t>(colorSpace)];
            LL_LOG(Category(), LogLevel::Trace,
                   "[JPEG] Decode header: {}x{}, source={}({}), precision={}, subsampling={}", width, height,
                   color.name, colorSpace, precision, tj3Get(handle.get(), TJPARAM_SUBSAMP));

            try
            {
                auto imageItem              = std::make_shared<ImageItem>();
                imageItem->metaData         = std::make_shared<ItemMetaData>();
                auto& metaData              = *imageItem->metaData;
                metaData.sourceColorSpace   = color.source;
                metaData.sourceCmykInverted = color.cmyk &&
                                              (colorSpace == TJCS_YCCK || HasAdobeMarker(jpegBuffer, size));

                unsigned char* iccBuffer = nullptr;
                size_t iccSize           = 0;
                const int iccResult      = tj3GetICCProfile(handle.get(), &iccBuffer, &iccSize);
                const std::unique_ptr<unsigned char, decltype(&tj3Free)> iccOwner(iccBuffer, tj3Free);
                // Absence is a benign ICC-query warning. pixel/header warnings still reject the image.
                if (iccResult != 0)
                {
                    const bool absent = tj3GetErrorCode(handle.get()) == TJERR_WARNING && iccSize == 0 &&
                                        iccBuffer == nullptr;
                    const auto result = ReportJpegError(handle.get(), "decode/tj3GetICCProfile",
                                                        absent ? ImageResult::Success : ImageResult::UnknownError,
                                                        iccResult);
                    if (result != ImageResult::Success)
                        return result;
                }
                LL_LOG(Category(), LogLevel::Trace, "[JPEG] Decode metadata: ICC bytes={}, inverted CMYK={}", iccSize,
                       metaData.sourceCmykInverted);
                if (iccSize != 0)
                {
                    const auto* profile = reinterpret_cast<const std::byte*>(iccBuffer);
                    metaData.iccProfile.assign(profile, profile + iccSize);
                }
                imageItem->data.Allocate(static_cast<size_t>(imageDataSize));
                if (imageItem->data.data() == nullptr)
                    return ReportResult(LogLevel::Error, ImageResult::UnknownError,
                                        "[JPEG] Decode output allocation: result={}({}), bytes={}", imageDataSize);

                auto* pixels       = reinterpret_cast<unsigned char*>(imageItem->data.data());
                const int rowPitch = width * bytesPerPixel;
                if (const int status = tj3Decompress8(handle.get(), jpegBuffer, size, pixels, rowPitch,
                                                      color.cmyk ? TJPF_CMYK : TJPF_RGBA);
                    status != 0)
                    return ReportJpegError(handle.get(), "decode/tj3Decompress8", ImageResult::FileIsCorrupted, status);
                // Complementing Adobe samples is lossless. Retain CMYK channels. display conversion owns RGB output.
                if (metaData.sourceCmykInverted)
                {
                    for (size_t offset = 0; offset < imageDataSize; ++offset)
                        pixels[offset] = static_cast<unsigned char>(255 - pixels[offset]);
                }
                imageItem->itemType                           = ImageItemType::Image;
                imageItem->descriptor.texelFormatDecompressed = color.cmyk ? TexelFormat::I_C8_M8_Y8_K8
                                                                           : TexelFormat::I_R8_G8_B8_A8;
                imageItem->descriptor.texelFormatStorage      = color.cmyk                ? TexelFormat::I_C8_M8_Y8_K8
                                                                : colorSpace == TJCS_GRAY ? TexelFormat::I_X8
                                                                                          : TexelFormat::I_R8_G8_B8;
                imageItem->descriptor.width                   = static_cast<uint32_t>(width);
                imageItem->descriptor.height                  = static_cast<uint32_t>(height);
                imageItem->descriptor.rowPitchInBytes         = static_cast<uint32_t>(rowPitch);
                out_image = std::make_shared<Image>(imageItem, ImageItemType::Unknown);
                LL_LOG(Category(), LogLevel::Info,
                       "[JPEG] Decode complete: {}x{}, source={}, output={}, bytes={}, result=Success({})", width,
                       height, color.name, color.cmyk ? "CMYK8" : "RGBA8", imageDataSize,
                       std::to_underlying(ImageResult::Success));
            }
            catch (const std::bad_alloc&)
            {
                return ReportResult(LogLevel::Error, ImageResult::UnknownError,
                                    "[JPEG] Decode allocation failed: result={}({})");
            }
            return ImageResult::Success;
        }

        int GetPixelFormat(TexelFormat texelFormat)
        {
            switch (texelFormat)
            {
                case TexelFormat::I_R8_G8_B8_A8:
                    return TJPF_RGBA;
                case TexelFormat::I_R8_G8_B8:
                    return TJPF_RGB;
                case TexelFormat::I_B8_G8_R8_A8:
                    return TJPF_BGRA;
                case TexelFormat::I_B8_G8_R8:
                    return TJPF_BGR;
                case TexelFormat::I_A8_R8_G8_B8:
                    return TJPF_ARGB;
                case TexelFormat::I_A8_B8_G8_R8:
                    return TJPF_ABGR;
                case TexelFormat::I_X8:
                    return TJPF_GRAY;
                default:
                    return TJPF_UNKNOWN;
            }
        }

        ImageResult Encode(const ImageSharedPtr& image, const Parameters& params,
                           LLUtils::Buffer& encodedBuffer) override
        {
            if (image == nullptr)
                return ReportResult(LogLevel::Warning, ImageResult::BadParameters,
                                    "[JPEG] Encode input is null: result={}({})");
            // Apply transfer and display encoding before writing RGB samples without that interpretation.
            if (image->GetImageItem()->colorInfo.RequiresTransferBakeForEncoding())
                return ReportResult(LogLevel::Info, ImageResult::FormatNotSupported,
                                    "[JPEG] Encode requires display-encoded samples: result={}({})");
            const int pixelFormat = GetPixelFormat(image->GetTexelFormat());
            if (pixelFormat == TJPF_UNKNOWN)
                return ReportResult(LogLevel::Info, ImageResult::FormatNotSupported,
                                    "[JPEG] Encode format: result={}({}), format={}",
                                    std::to_underlying(image->GetTexelFormat()));

            const uint32_t width    = image->GetWidth();
            const uint32_t height   = image->GetHeight();
            const uint32_t rowPitch = image->GetRowPitchInBytes();
            const uint64_t rowSize  = static_cast<uint64_t>(width) * tjPixelSize[pixelFormat];
            if (width == 0 || height == 0 || width > std::numeric_limits<int>::max() ||
                height > std::numeric_limits<int>::max() || rowPitch > std::numeric_limits<int>::max() ||
                rowPitch < rowSize || image->GetBuffer() == nullptr ||
                static_cast<uint64_t>(height - 1) * rowPitch + rowSize > image->GetImageItem()->data.size())
                return ReportResult(LogLevel::Warning, ImageResult::BadParameters,
                                    "[JPEG] Encode buffer validation: result={}({}), width={}, height={}, pitch={}, "
                                    "bytes={}",
                                    width, height, rowPitch, image->GetImageItem()->data.size());

            double quality = 80;
            if (const auto* parameter = params.GetCustom(LLUTILS_TEXT("quality")))
            {
                if (const auto* value = std::any_cast<double>(parameter))
                    quality = *value;
                else if (const auto* value = std::any_cast<int>(parameter))
                    quality = *value;
                else
                    return ReportResult(LogLevel::Warning, ImageResult::BadParameters,
                                        "[JPEG] Encode quality must be int or double: result={}({})");
            }
            // /fp:fast can optimize away isfinite. inspect the exponent before converting quality to int.
            static_assert(std::numeric_limits<double>::is_iec559 && sizeof(double) == sizeof(uint64_t));
            constexpr uint64_t exponentMask = 0x7ff0000000000000;
            if ((std::bit_cast<uint64_t>(quality) & exponentMask) == exponentMask || quality < 0 || quality > 100)
                return ReportResult(LogLevel::Warning, ImageResult::BadParameters,
                                    "[JPEG] Encode quality must be finite and in 0..100: result={}({})");
            // Preserve the advertised 0..100 range. TurboJPEG's minimum is 1.
            const int jpegQuality = std::max(1, static_cast<int>(quality));
            const int subsampling = pixelFormat == TJPF_GRAY ? TJSAMP_GRAY : TJSAMP_444;
            LL_LOG(Category(), LogLevel::Trace,
                   "[JPEG] Encode begin: {}x{}, pixelFormat={}, pitch={}, quality={}, subsampling={}", width, height,
                   pixelFormat, rowPitch, jpegQuality, subsampling);

            JpegHandle handle(tj3Init(TJINIT_COMPRESS), tj3Destroy);
            if (!handle)
                return ReportJpegError(nullptr, "encode/tj3Init", ImageResult::UnknownError);
            if (const int status = tj3Set(handle.get(), TJPARAM_QUALITY, jpegQuality); status != 0)
                return ReportJpegError(handle.get(), "encode/tj3Set(QUALITY)", ImageResult::UnknownError, status);
            if (const int status = tj3Set(handle.get(), TJPARAM_SUBSAMP, subsampling); status != 0)
                return ReportJpegError(handle.get(), "encode/tj3Set(SUBSAMP)", ImageResult::UnknownError, status);

            // TurboJPEG allocates its output. retain ownership even when compression or the final copy fails.
            unsigned char* jpegBuffer = nullptr;
            size_t jpegSize           = 0;
            const int resultCode      = tj3Compress8(handle.get(),
                                                     reinterpret_cast<const unsigned char*>(image->GetBuffer()),
                                                     static_cast<int>(width), static_cast<int>(rowPitch),
                                                     static_cast<int>(height), pixelFormat, &jpegBuffer, &jpegSize);
            const std::unique_ptr<unsigned char, decltype(&tj3Free)> jpegOwner(jpegBuffer, tj3Free);
            if (resultCode != 0)
                return ReportJpegError(handle.get(), "encode/tj3Compress8", ImageResult::UnknownError, resultCode);
            try
            {
                LLUtils::Buffer result(jpegSize);
                if (result.data() == nullptr)
                    return ReportResult(LogLevel::Error, ImageResult::UnknownError,
                                        "[JPEG] Encode output allocation: result={}({}), bytes={}", jpegSize);
                result.Write(reinterpret_cast<const std::byte*>(jpegBuffer), 0, jpegSize);
                encodedBuffer = std::move(result);
                LL_LOG(Category(), LogLevel::Info, "[JPEG] Encode complete: {}x{}, bytes={}, result=Success({})", width,
                       height, jpegSize, std::to_underlying(ImageResult::Success));
            }
            catch (const std::bad_alloc&)
            {
                return ReportResult(LogLevel::Error, ImageResult::UnknownError,
                                    "[JPEG] Encode allocation failed: result={}({})");
            }
            return ImageResult::Success;
        }

        ImageResult GetEncoderParameters(ListParameterDescriptors& out_encodeParameters) override
        {
            out_encodeParameters = {{LLUTILS_TEXT("quality"), LLUTILS_TEXT("double"),
                                     LLUTILS_TEXT("Jpeg compression quality"),
                                     LLUTILS_TEXT("Higher value means better quality"), LLUTILS_TEXT("80"),
                                     LLUTILS_TEXT("0"), LLUTILS_TEXT("100")}};
            return ImageResult::Success;
        }
    };
}  // namespace IMCodec
