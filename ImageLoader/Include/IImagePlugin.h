#pragma once
#include <string>
#include "Image.h"

namespace IMCodec
{
    struct PluginProperties
    {
        std::string pluginDescription;
        std::string supportedExtentions;
        bool hasMultipleImages = false;
    };
    class IImagePlugin
    {
    public:
        virtual bool LoadImage(const uint8_t* buffer, std::size_t size, ImageDescriptor& out_properties) = 0;
        virtual bool LoadImages([[maybe_unused]] const uint8_t* buffer, [[maybe_unused]]  std::size_t size
            , [[maybe_unused]] std::vector<ImageDescriptor>& out_vec_properties)
        {
            LL_EXCEPTION(LLUtils::Exception::ErrorCode::NotImplemented, std::string("Multiple image load for codec ") + GetPluginProperties().pluginDescription + " is not implemented");
        }
        virtual bool SaveImage([[maybe_unused]] const uint8_t* buffer, [[maybe_unused]] std::size_t size,[[maybe_unused]]  ImageDescriptor& out_properties)
        {
            LL_EXCEPTION(LLUtils::Exception::ErrorCode::NotImplemented, std::string("save image for codec ") + GetPluginProperties().pluginDescription + " is not implemented");
        }
        virtual PluginProperties& GetPluginProperties() = 0;
    };

    class IImageLoader
    {
    public:
        virtual bool Load(uint8_t* buffer, std::size_t size, char* extension, bool onlyRegisteredExtension, VecImageSharedPtr& out_images) const = 0;
    };
}
