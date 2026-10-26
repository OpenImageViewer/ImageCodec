#pragma once
#include <vector>
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

    struct ImageItem
    {
        // Describes the decoded samples without applying display conversions.
        // Unknown defaults let codecs adopt this independently. ICC profile data
        // is owned by the image and shared by animation frames and image copies.
        struct ColorInfo
        {
            enum class Encoding
            {
                Unknown,
                SRGB,
                ICCProfile
            };
            enum class TransferFunction
            {
                Unknown,
                Linear,
                SRGB,
                Gamma
            };
            enum class AlphaMode
            {
                Unknown,
                Opaque,
                Straight,
                Premultiplied
            };

            enum class RenderingIntent
            {
                Perceptual,
                RelativeColorimetric,
                Saturation,
                AbsoluteColorimetric
            };

            Encoding encoding                 = Encoding::Unknown;
            TransferFunction transferFunction = TransferFunction::Unknown;
            AlphaMode alphaMode               = AlphaMode::Unknown;
            // The source encoding exponent, not the viewer's gamma adjustment.
            // An ICC profile takes precedence over sRGB, which takes precedence
            // over gamma alone. Retain gamma even when a stronger declaration exists.
            std::optional<double> gamma;
            std::optional<RenderingIntent> srgbRenderingIntent;
            std::shared_ptr<const std::vector<std::byte>> iccProfile;
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
