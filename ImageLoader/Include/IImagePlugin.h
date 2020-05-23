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
        virtual bool LoadImages(const uint8_t* buffer, std::size_t size, std::vector<ImageDescriptor>& out_vec_properties)
        {
            LL_EXCEPTION(LLUtils::Exception::ErrorCode::NotImplemented, std::string("Multiple image load for codec ") + GetPluginProperties().pluginDescription + " is not implemented");
        }
        virtual bool SaveImage(const uint8_t* buffer, std::size_t size, ImageDescriptor& out_properties)
        {
            LL_EXCEPTION(LLUtils::Exception::ErrorCode::NotImplemented, std::string("save image for codec ") + GetPluginProperties().pluginDescription + " is not implemented");
        }
        virtual PluginProperties& GetPluginProperties() = 0;
    };
}
