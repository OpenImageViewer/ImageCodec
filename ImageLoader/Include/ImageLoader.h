#pragma once
#include "IImagePlugin.h"
#include "Image.h"
#include <unordered_map>
#include <chrono>
#include <vector>
#include <NetSettings/GuiProvider.h>


namespace IMCodec
{
    class ImageCodec : public IImageCodec
    {
    public: // const methods
        ImageCodec();
        std::wstring GetKnownFileTypes() const;
        std::vector<PluginProperties> GetCodecsInfo();
        
        void InstallPlugin(IImagePlugin* plugin);


        ImageResult Encode(const ImageSharedPtr image, const std::wstring& extension , LLUtils::Buffer& encoded) override;

        ImageResult Decode(const std::byte* buffer
            , std::size_t size
            , const char* extensionHint
            , ImageLoadFlags imageLoadFlags
            , ImageLoaderFlags imageLoaderFlags
            , const Parameters& params
            , ImageSharedPtr& image) const override;
        
    private: // methods
        using ListPlugin = std::vector<IImagePlugin*>;
        using MapStringListPlugin = std::unordered_map<std::wstring, ListPlugin>;
        IImagePlugin* GetFirstPlugin(const std::wstring& hint) const;
        ImageResult TryLoad(IImagePlugin* plugin, const std::byte* buffer, std::size_t size, ImageLoadFlags loadFlags, const Parameters& params, ImageSharedPtr& image) const;
        void OpenSettingsDialog(IMCodec::ListParameterDescriptors& descriptors);
        void InitializeNetSettings();
        

        MapStringListPlugin fMapPlugins;
        ListPlugin fListPlugins;


        static void NetSettingsCallback_(ItemChangedArgs* args);
        void NetSettingsCallback(ItemChangedArgs* args);

        using netsettings_Create_func = void (*)(GuiCreateParams*);
        using netsettings_SetVisible_func = void (*)(bool);
        using netsettings_SaveSettings_func = void (*)();

        struct SettingsContext
        {
            bool created;
            netsettings_Create_func Create;
            netsettings_SetVisible_func SetVisible;
            netsettings_SaveSettings_func  SaveSettings;
        } settingsContext{};

    };
}
