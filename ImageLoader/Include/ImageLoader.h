#pragma once
#include "IImagePlugin.h"
#include <unordered_map>
#include <chrono>
#include <vector>


namespace IMCodec
{
    class ImageLoader
    {
        typedef std::vector<IImagePlugin*> ListPlugin;
        typedef std::unordered_map<std::wstring, ListPlugin> MapStringListPlugin;
        MapStringListPlugin fMapPlugins;
        ListPlugin fListPlugins;
        IImagePlugin* GetFirstPlugin(const std::wstring& hint) const;
        bool TryLoad(IImagePlugin* plugin, uint8_t* buffer, std::size_t size, VecImageSharedPtr& out_images) const;

    public: // const methods
        std::wstring GetKnownFileTypes() const;
        bool Load(uint8_t* buffer, std::size_t size, char* extension, bool onlyRegisteredExtension , VecImageSharedPtr& out_images) const;
        
    public: // methods
        ImageLoader();
        void InstallPlugin(IImagePlugin* plugin);
    };
}
