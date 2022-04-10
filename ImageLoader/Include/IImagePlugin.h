#pragma once
#include <string>
#include <LLUtils/EnumClassBitwise.h>
#include <LLUtils/Exception.h>
#include "ImageCommon.h"
#include "ImageItem.h"

namespace IMCodec
{
    class Image;
    using ImageSharedPtr = std::shared_ptr<Image>;
    struct PluginProperties
    {
        std::wstring pluginDescription;
        std::string supportedExtentions;
    };

    enum class ImageResult
    {
          Success
        , Fail
    };


    class IImagePlugin
    {
    public:
        virtual ImageResult LoadMemoryImageFile(const std::byte* buffer, std::size_t size, [[maybe_unused]] ImageLoadFlags loadFlags, ImageSharedPtr& out_image) = 0;

      /*  virtual bool LoadImage(const uint8_t* buffer, std::size_t size, ImageDescriptor& out_properties) = 0;
        virtual bool LoadImages([[maybe_unused]] const uint8_t* buffer, [[maybe_unused]]  std::size_t size
            , [[maybe_unused]] std::vector<ImageDescriptor>& out_vec_properties)
        {
            LL_EXCEPTION(LLUtils::Exception::ErrorCode::NotImplemented, std::string("Multiple image load for codec ") + GetPluginProperties().pluginDescription + " is not implemented");
        }*/
        /*virtual bool SaveImage([[maybe_unused]] const uint8_t* buffer, [[maybe_unused]] std::size_t size,[[maybe_unused]]  ImageDescriptor& out_properties)
        {
            LL_EXCEPTION(LLUtils::Exception::ErrorCode::NotImplemented, std::string("save image for codec ") + GetPluginProperties().pluginDescription + " is not implemented");
        }*/
        virtual PluginProperties& GetPluginProperties() = 0;
    };

    enum class ImageLoaderFlags
    {
          None                    = 0 << 0
        , OnlyRegisteredExtension = 1 << 0
    };

    LLUTILS_DEFINE_ENUM_CLASS_FLAG_OPERATIONS(ImageLoaderFlags)

    class IImageLoader
    {
    public:
        virtual ImageResult Load(const std::byte* buffer
            , std::size_t size
            , const char* extensionHint
            , ImageLoadFlags imageLoadFlags
            , ImageLoaderFlags imageLoaderFlags
            , ImageSharedPtr& out_image) const = 0;
    };
}
