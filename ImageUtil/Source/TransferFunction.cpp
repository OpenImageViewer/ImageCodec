#include <ImageUtil/ImageUtil.h>
#include <LLUtils/Exception.h>
#include <array>
#include <cmath>
#include <cstring>
#include <span>
#include <algorithm>
#include <type_traits>

namespace IMUtil
{
    namespace
    {
        // Resolve metadata once per distinct curve, keeping interpolation and variant dispatch out of
        // the pixel loop. Full-range 16-bit direct tables can be borrowed without a copy or rescaling.
        std::span<const uint16_t> PrepareCurve(const IMCodec::SampledTransferCurve& curve, size_t sampleCount,
                                               std::vector<uint16_t>& storage)
        {
            using Indexing = IMCodec::SampledTransferCurve::Indexing;
            if (!std::isfinite(curve.outputScale) || curve.outputScale <= 0 ||
                (curve.indexing != Indexing::DirectInteger && curve.indexing != Indexing::NormalizedLinear))
                LL_EXCEPTION(LLUtils::Exception::ErrorCode::BadParameters, "Invalid sampled transfer curve");
            return std::visit(
                [&](const auto& entries) -> std::span<const uint16_t>
                {
                    if (entries.empty() || (curve.indexing == Indexing::DirectInteger && entries.size() < sampleCount))
                        LL_EXCEPTION(LLUtils::Exception::ErrorCode::BadParameters, "Incomplete sampled transfer curve");
                    if constexpr (std::is_same_v<typename std::decay_t<decltype(entries)>::value_type, uint16_t>)
                    {
                        if (curve.indexing == Indexing::DirectInteger && curve.outputScale == 1.0 / 65535.0)
                            return std::span(entries).first(sampleCount);
                    }
                    if (*std::ranges::max_element(entries) * curve.outputScale > 1.0)
                        LL_EXCEPTION(LLUtils::Exception::ErrorCode::BadParameters,
                                     "Transfer output exceeds the linear 16-bit range");
                    storage.resize(sampleCount);
                    for (size_t code = 0; code < sampleCount; ++code)
                    {
                        double value;
                        if (curve.indexing == Indexing::DirectInteger)
                            value = entries[code];
                        else
                        {
                            const double position = static_cast<double>(code) * (entries.size() - 1) /
                                                    (sampleCount - 1);
                            const auto lower      = std::min(static_cast<size_t>(position), entries.size() - 1);
                            const auto upper      = std::min(lower + 1, entries.size() - 1);
                            value = std::lerp(static_cast<double>(entries[lower]), static_cast<double>(entries[upper]),
                                              position - lower);
                        }
                        storage[code] = static_cast<uint16_t>(std::lround(value * curve.outputScale * 65535.0));
                    }
                    return storage;
                },
                curve.entries);
        }

        // The caller validates source layout and lookup domains and allocates packed RGBA16 output.
        // Fixed sample widths and alpha presence keep format decisions out of the pixel loop.
        template <class Sample, bool HasAlpha>
        void ApplyTransferPixels(const IMCodec::Image& image, IMCodec::ImageItem& output,
                                 const std::array<std::span<const uint16_t>, 3>& mappings)
        {
            static_assert(std::is_same_v<Sample, uint8_t> || std::is_same_v<Sample, uint16_t>);
            constexpr size_t channels         = HasAlpha ? 4 : 3;
            constexpr size_t sourcePixelBytes = channels * sizeof(Sample);
            for (uint32_t y = 0; y < image.GetHeight(); ++y)
            {
                const auto* source = image.GetBufferAt(0, y);
                auto* target = reinterpret_cast<uint16_t*>(output.data.data() +
                                                           static_cast<size_t>(y) * output.descriptor.rowPitchInBytes);
                for (uint32_t x = 0; x < image.GetWidth(); ++x)
                {
                    std::array<Sample, channels> samples;
                    std::memcpy(samples.data(), source + static_cast<size_t>(x) * sourcePixelBytes, sourcePixelBytes);
                    target[x * 4]     = mappings[0][samples[0]];
                    target[x * 4 + 1] = mappings[1][samples[1]];
                    target[x * 4 + 2] = mappings[2][samples[2]];
                    if constexpr (HasAlpha)
                        target[x * 4 + 3] = static_cast<uint16_t>(samples[3] * (sizeof(Sample) == 1 ? 257 : 1));
                    else
                        target[x * 4 + 3] = 65535;
                }
            }
        }

        void ValidateTransferSource(const IMCodec::ImageSharedPtr& source)
        {
            if (source->GetWidth() == 0 || source->GetHeight() == 0 ||
                source->GetRowPitchInBytes() < source->GetBytesPerRowOfPixels() ||
                source->GetSizeInMemory() > source->GetImageItem()->data.Size())
                LL_EXCEPTION(LLUtils::Exception::ErrorCode::BadParameters, "Invalid transfer-function image layout");
        }
    }  // namespace

    IMCodec::ImageSharedPtr ImageUtil::ApplyTransferFunction(const IMCodec::ImageSharedPtr& image)
    {
        using namespace IMCodec;
        using ColorInfo = ImageItem::ColorInfo;
        auto result     = image;
        if (image && image->GetImageItem()->colorInfo.transferFunction == ColorInfo::TransferFunction::Sampled)
        {
            const auto& info    = image->GetImageItem()->colorInfo;
            const auto format   = image->GetTexelFormat();
            const bool hasAlpha = format == TexelFormat::I_R8_G8_B8_A8 || format == TexelFormat::I_R16_G16_B16_A16;
            const bool wide     = format == TexelFormat::I_R16_G16_B16 || format == TexelFormat::I_R16_G16_B16_A16;
            if ((!hasAlpha && format != TexelFormat::I_R8_G8_B8 && format != TexelFormat::I_R16_G16_B16) ||
                !info.sampledTransfer || info.encoding != ColorInfo::Encoding::SRGB || info.iccProfile ||
                (hasAlpha && info.alphaMode != ColorInfo::AlphaMode::Straight &&
                 info.alphaMode != ColorInfo::AlphaMode::Opaque))
                LL_EXCEPTION(LLUtils::Exception::ErrorCode::BadParameters, "Unsupported sampled transfer metadata");
            const auto& curves       = info.sampledTransfer->channels;
            const size_t sampleCount = wide ? 65536 : 256;
            std::array<std::vector<uint16_t>, 3> storage;
            std::array<std::span<const uint16_t>, 3> mappings;
            for (size_t channel = 0; channel < curves.size(); ++channel)
            {
                if (!curves[channel])
                    LL_EXCEPTION(LLUtils::Exception::ErrorCode::BadParameters, "Missing RGB transfer curve");
                const auto previous = std::find(curves.begin(), curves.begin() + channel, curves[channel]);
                mappings[channel]   = previous != curves.begin() + channel
                                          ? mappings[static_cast<size_t>(previous - curves.begin())]
                                          : PrepareCurve(*curves[channel], sampleCount, storage[channel]);
            }
            ValidateTransferSource(image);
            auto output = CreateDerivedImageItem(image, TexelFormat::I_R16_G16_B16_A16, image->GetWidth(),
                                                 image->GetHeight());
            output->colorInfo.sampledTransfer.reset();
            output->colorInfo.gamma.reset();
            output->colorInfo.transferFunction = ColorInfo::TransferFunction::Linear;
            output->colorInfo.alphaMode        = hasAlpha ? info.alphaMode : ColorInfo::AlphaMode::Opaque;
            switch (format)
            {
                case TexelFormat::I_R8_G8_B8:
                    ApplyTransferPixels<uint8_t, false>(*image, *output, mappings);
                    break;
                case TexelFormat::I_R8_G8_B8_A8:
                    ApplyTransferPixels<uint8_t, true>(*image, *output, mappings);
                    break;
                case TexelFormat::I_R16_G16_B16:
                    ApplyTransferPixels<uint16_t, false>(*image, *output, mappings);
                    break;
                case TexelFormat::I_R16_G16_B16_A16:
                    ApplyTransferPixels<uint16_t, true>(*image, *output, mappings);
                    break;
                default:
                    LL_EXCEPTION(LLUtils::Exception::ErrorCode::LogicError,
                                 "Validated transfer format has no pixel kernel");
            }
            result = std::make_shared<Image>(std::move(output), ImageItemType::Unknown);
        }
        return result;
    }

    IMCodec::ImageSharedPtr ImageUtil::EncodeLinearImageForDisplay(const IMCodec::ImageSharedPtr& image)
    {
        using namespace IMCodec;
        using ColorInfo = ImageItem::ColorInfo;
        auto result     = image;
        if (image && image->GetTexelFormat() == TexelFormat::I_R16_G16_B16_A16 &&
            image->GetImageItem()->colorInfo.encoding == ColorInfo::Encoding::SRGB &&
            image->GetImageItem()->colorInfo.transferFunction == ColorInfo::TransferFunction::Linear)
        {
            // Build the fixed inverse transfer once, avoiding a power operation for every displayed channel.
            static const auto encoding = []
            {
                std::array<uint8_t, 65536> table{};
                for (size_t i = 0; i < table.size(); ++i)
                {
                    const double linear = static_cast<double>(i) / 65535.0;
                    const double srgb   = linear <= 0.0031308 ? linear * 12.92
                                                              : 1.055 * std::pow(linear, 1.0 / 2.4) - 0.055;
                    table[i]            = static_cast<uint8_t>(std::lround(srgb * 255.0));
                }
                return table;
            }();
            ValidateTransferSource(image);
            auto output = CreateDerivedImageItem(image, TexelFormat::I_R8_G8_B8_A8, image->GetWidth(),
                                                 image->GetHeight());
            output->colorInfo.sampledTransfer.reset();
            output->colorInfo.gamma.reset();
            output->colorInfo.transferFunction = ColorInfo::TransferFunction::SRGB;
            for (uint32_t y = 0; y < image->GetHeight(); ++y)
            {
                auto* target = reinterpret_cast<uint8_t*>(output->data.data() +
                                                          static_cast<size_t>(y) * output->descriptor.rowPitchInBytes);
                for (uint32_t x = 0; x < image->GetWidth(); ++x)
                {
                    std::array<uint16_t, 4> pixel;
                    std::memcpy(pixel.data(), image->GetBufferAt(x, y), sizeof(pixel));
                    for (size_t channel = 0; channel < 3; ++channel)
                        target[x * 4 + channel] = encoding[pixel[channel]];
                    target[x * 4 + 3] = static_cast<uint8_t>((static_cast<uint32_t>(pixel[3]) + 128) / 257);
                }
            }
            result = std::make_shared<Image>(std::move(output), ImageItemType::Unknown);
        }
        return result;
    }
}  // namespace IMUtil
