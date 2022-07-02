#pragma once
#include <IImageCodec.h>
namespace IMCodec
{
    class ImageCodec : public IImageCodec
    {
    public:
        ImageCodec();
        ImageResult Decode(const std::byte* buffer
            , std::size_t size
            , const PluginID& pluginID
            , ImageLoadFlags imageLoadFlags
            , const Parameters& params
            , ImageSharedPtr& out_image) override;

        ImageResult Encode(const ImageSharedPtr image, const PluginID& pluginId, const Parameters& params, LLUtils::Buffer& encoded) override;

        ImageResult InstallPlugin(IImagePlugin* plugin) override;
        ImageResult InstallPlugin(const std::wstring& pluginFilePath) override;

        ImageResult GetEncoderParameters(const PluginID& pluginID, ListParameterDescriptors& out_encodeParameters) override;

        std::vector<PluginProperties> GetPluginsInfo() override;

        ImageResult GetPluginInfo(const PluginID& pluginID, PluginProperties& pluginProperties) override;

        using ListPlugin = std::vector<IImagePlugin*>;
        using MapStringListPlugin = std::unordered_map<std::wstring, ListPlugin>;
        using MapPluginIDPlugin = std::map<PluginID, IImagePlugin*>;

    private: //methods
        IImagePlugin* GetPluginByID(const PluginID& pluginId);
    private:
        MapPluginIDPlugin fMapPluginIDPlugin;
        ListPlugin fListPlugins;
    };
}