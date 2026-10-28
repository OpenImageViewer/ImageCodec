#pragma once
#include <vector>
#include <array>
#include <variant>
#include <memory>
#include <optional>
#include "ImageDescriptor.h"
#include "ImageBuffer.h"
#include <limits>
#include "ImageCommon.h"

namespace IMCodec
{
    enum class ImageItemType
    {
        Image  // Default
        ,
        Container,
        Mipmap,
        Pages,
        AnimationFrame,
        Unknown
    };

    struct ExifData
    {
        int orientation         = 0;
        double longitude        = std::numeric_limits<double>::max();
        double latitude         = std::numeric_limits<double>::max();
        double altitude         = std::numeric_limits<double>::max();
        int8_t relativeAltitude = std::numeric_limits<int8_t>::max();  // 0 = Above Sea Level, 1 = Below Sea Level
        std::string make;
        std::string model;
        std::string software;
        std::string copyright;

        union
        {
            uint16_t flashValue;
            struct _flash
            {
                unsigned char flashFired : 1;
                unsigned char returnLight : 2;
                unsigned char flashMode : 2;
                unsigned char flashPresent : 1;
                unsigned char redeyeCorretion : 1;
                unsigned char reserved1 : 1;
                unsigned char reserved2 : 8;
            } flash;
        } flash;
    };

    enum class SourceColorSpace : uint8_t
    {
        Unknown,
        RGB,
        YCbCr,
        Gray,
        CMYK,
        YCCK
    };

    // Source provenance is shared by derived images and remains unchanged after loading.
    // An ICC profile here describes the source, not necessarily the current pixel buffer.
    struct ItemMetaData
    {
        ExifData exifData{};
        SourceColorSpace sourceColorSpace = SourceColorSpace::Unknown;
        // Whether decoded CMYK samples were inverted before normalization to ink amounts.
        bool sourceCmykInverted = false;
        std::vector<std::byte> iccProfile;
    };

    using ItemMetaDataSharedPtr = std::shared_ptr<ItemMetaData>;

    struct ItemProcessData
    {
        double processTime{};  // Process time for decode / encode in ms
        PluginID pluginUsed;
    };

    using ItemProcessDataSharedPtr = std::shared_ptr<ItemProcessData>;

    struct AnimationData
    {
        uint32_t delayMilliseconds{};
    };

    // Sample codes map to linear intensity. Floating-point entries, nonuniform knots, formula curves,
    // custom input domains, extrapolation, and multidimensional color LUTs are not represented here.
    struct SampledTransferCurve
    {
        enum class Indexing
        {
            DirectInteger,
            NormalizedLinear
        };
        using Entries = std::variant<std::vector<uint8_t>, std::vector<uint16_t>>;

        Entries entries;
        // Direct indices must cover the input format's integer range. Normalized sampling maps [0, 1]
        // across uniformly spaced entries with linear interpolation, including both endpoints.
        Indexing indexing = Indexing::DirectInteger;
        // Multiply entries by this positive finite scale to obtain linear intensity in [0, 1].
        double outputScale = 1.0;
    };

    struct SampledTransferFunction
    {
        using CurvePtr = std::shared_ptr<const SampledTransferCurve>;
        // All R, G, B pointers must be valid. Shared curves reuse a pointer. Alpha is excluded.
        std::array<CurvePtr, 3> channels;
    };

    struct ImageItem
    {
        // Describes the decoded samples without applying display conversions.
        // Unknown defaults let codecs adopt this independently. ICC profile data
        // is owned by the image and shared by animation frames and image copies.
        struct ColorInfo
        {
            // The color-space declaration is separate from the sample transfer. An ICC profile can define both.
            enum class Encoding
            {
                Unknown,    // No color-space declaration. This does not imply linear samples.
                SRGB,       // sRGB color basis. transferFunction describes the current sample encoding.
                ICCProfile  // iccProfile defines the color space and tone response.
            };
            enum class TransferFunction
            {
                Unknown,  // No explicit standalone transfer declaration.
                Linear,   // Samples are proportional to light intensity.
                SRGB,     // Samples use the standard nonlinear sRGB transfer curve.
                Gamma,    // Power-law encoding. gamma stores the source encoding exponent.
                Sampled   // sampledTransfer supplies RGB mappings from stored samples to linear intensity.
            };
            enum class AlphaMode
            {
                Unknown,       // Alpha association has not been declared.
                Opaque,        // Every pixel is opaque. Alpha is absent or full scale.
                Straight,      // RGB values are stored independently of alpha (unassociated).
                Premultiplied  // RGB values have been multiplied by alpha (associated).
            };

            enum class RenderingIntent
            {
                Perceptual,            // Preserve visual relationships when mapping colors into a destination gamut.
                RelativeColorimetric,  // Preserve in-gamut colors and adapt source white to destination white.
                Saturation,            // Favor color saturation over colorimetric accuracy.
                AbsoluteColorimetric   // Preserve the source reference white, typically for proofing.
            };

            Encoding encoding                 = Encoding::Unknown;
            TransferFunction transferFunction = TransferFunction::Unknown;
            AlphaMode alphaMode               = AlphaMode::Unknown;
            // The source encoding exponent, not the viewer's gamma adjustment.
            // An ICC profile takes precedence over sRGB, which takes precedence
            // over gamma alone. Retain gamma even when a stronger declaration exists.
            std::optional<double> gamma;
            // Rendering-intent metadata currently modeled on PNG's sRGB chunk.
            // PNG preserves it when saving and defaults to Perceptual when absent.
            // Other codecs may map compatible intent metadata here as support is added.
            // Currently unused by viewer rendering, gamma correction, and linearization.
            std::optional<RenderingIntent> srgbRenderingIntent;
            std::shared_ptr<const std::vector<std::byte>> iccProfile;
            // Active for TransferFunction::Sampled. Applied conversions clear it on their output.
            std::optional<SampledTransferFunction> sampledTransfer;

            // The current PNG/JPEG RGB writers reject pending sampled transfers and linear sRGB samples.
            // Callers must bake those into a derived, display-encoded image before encoding.
            // This query neither converts pixels nor validates every color-metadata combination.
            bool RequiresTransferBakeForEncoding() const
            {
                return transferFunction == TransferFunction::Sampled ||
                       (encoding == Encoding::SRGB && transferFunction == TransferFunction::Linear);
            }
        };

        ColorInfo colorInfo{};

        /// <summary>
        /// Image description , e.g. width and height.
        /// </summary>
        ImageDescriptor descriptor{};

        /// <summary>
        /// Data added to the item that resolves in runtime.
        /// </summary>
        // ItemRuntimeData runtimeData{};

        /// <summary>
        /// The raw image data
        /// </summary>
        ImageBuffer data{};
        /// <summary>
        /// THe type of the item
        /// </summary>
        ImageItemType itemType{};

        /// <summary>
        /// Animation data, applicable only form AnimationData item type.
        /// </summary>
        AnimationData animationData{};

        /// <summary>
        /// codec process information
        /// </summary>
        ItemProcessData processData{};
        ItemMetaDataSharedPtr metaData;
    };

    using ImageItemSharedPtr = std::shared_ptr<ImageItem>;
}  // namespace IMCodec
