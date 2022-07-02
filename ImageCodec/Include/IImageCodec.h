#pragma once
#include <cstddef>
#include <LLUtils/EnumClassBitwise.h>
#include "IImagePlugin.h"

namespace IMCodec
{
    class IImageCodec
    {
    public:
        // Decode memory buffer into ImageSharedPtr
        virtual ImageResult Decode(const std::byte* buffer
            , std::size_t size
            , const PluginID& pluginID
            , ImageLoadFlags imageLoadFlags
            , const Parameters& params
            , ImageSharedPtr& out_image) = 0;

        // Encode to memory buffer
        virtual ImageResult Encode(const ImageSharedPtr image, const PluginID& pluginId, const Parameters& params, LLUtils::Buffer& encoded) = 0;
        virtual ImageResult GetEncoderParameters(const PluginID& pluginID, ListParameterDescriptors& out_encodeParameters) = 0;
        virtual ImageResult InstallPlugin(IImagePlugin* plugin) = 0;
        virtual ImageResult InstallPlugin(const std::wstring& pluginFilePath) = 0;
        virtual ImageResult GetPluginInfo(const PluginID& pluginID, PluginProperties& pluginProperties) = 0;
        virtual std::vector<PluginProperties> GetPluginsInfo() = 0;
    };
}