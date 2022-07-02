#pragma once
#include <string>
#include <LLUtils/EnumClassBitwise.h>
#include <LLUtils/Exception.h>
#include "ImageCommon.h"
#include "ImageItem.h"
#include <variant>
#include <map>

namespace IMCodec
{
    /// <summary>
    /// Flags for an instace of IImagePlugin
    /// </summary>
    enum class ImageLoadFlags
    {
          None = 0 << 0
        , Preview = 1 << 0
        , LoadOnlyFirstImage = 1 << 1
    };

    LLUTILS_DEFINE_ENUM_CLASS_FLAG_OPERATIONS(ImageLoadFlags)

    class Image;
    using ImageSharedPtr = std::shared_ptr<Image>;
    
    using ListFileExtensions = std::vector<std::wstring>;

    struct ExtensionCollection
    {
        std::wstring description;
        ListFileExtensions listExtensions;
    };
    
    using ListxtensionCollection = std::vector<ExtensionCollection>;

    enum class CodecCapabilities
    {
          None          = 0 << 0 
        , Encode        = 1 << 0    // Can the Codec encode data
        , Decode        = 1 << 1    // Can the Codec decode data
        , BulkCodec     = 1 << 2    // Is the codec decodes many types of formats
    };

    LLUTILS_DEFINE_ENUM_CLASS_FLAG_OPERATIONS(CodecCapabilities)


    struct PluginProperties
    {
        PluginID id;
        CodecCapabilities capabilities;
        std::wstring pluginDescription;
        ListxtensionCollection extensionCollection;
    };


    using Parameter = std::variant<int, double, std::wstring>;
    using Parameters = std::map<std::wstring, Parameter>;

    struct ParameterDescriptor
    {
        std::wstring name;
        std::wstring type;
        std::wstring displayName;
        std::wstring description;
        std::wstring defaultValue;
        std::wstring min;
        std::wstring max;
    };

    using ListParameterDescriptors = std::vector<ParameterDescriptor>;


    class IImagePlugin
    {
    public:
        /// <summary>
        /// Decode memory buffer to a bitmap image
        /// </summary>
        /// <param name="buffer">memory buffer of the encoded image</param>
        /// <param name="size">size of the memory buffer</param>
        /// <param name="loadFlags"></param>
        /// <param name="out_image">image shared pointer result</param>
        /// <returns>ImageResult::Success on success</returns>
        virtual ImageResult Decode(const std::byte* buffer, std::size_t size, [[maybe_unused]] ImageLoadFlags loadFlags, const Parameters& params, ImageSharedPtr& out_image) = 0;

        virtual ImageResult Encode([[maybe_unused]] const ImageSharedPtr& image, [[maybe_unused]] const Parameters& EncodeParametrs
            , [[maybe_unused]] LLUtils::Buffer& encodedBuffer) { return ImageResult::NotImplemented; }

        virtual ImageResult GetEncoderParameters([[maybe_unused]] ListParameterDescriptors& out_encodeParameters) { return ImageResult::NotImplemented; }
        virtual const PluginProperties& GetPluginProperties() = 0;
    };
}
