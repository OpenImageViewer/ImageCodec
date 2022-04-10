#pragma once
#include "IImagePlugin.h"
#include "Image.h"
#include <unordered_map>
#include <chrono>
#include <vector>


namespace IMCodec
{
    class ImageLoader : public IImageLoader
    {
        typedef std::vector<IImagePlugin*> ListPlugin;
        typedef std::unordered_map<std::wstring, ListPlugin> MapStringListPlugin;
        MapStringListPlugin fMapPlugins;
        ListPlugin fListPlugins;
        IImagePlugin* GetFirstPlugin(const std::wstring& hint) const;
        ImageResult TryLoad(IImagePlugin* plugin, const std::byte* buffer, std::size_t size, ImageLoadFlags loadFlags, ImageSharedPtr& image) const;

    public: // const methods
        std::wstring GetKnownFileTypes() const;
        ImageResult Load(const std::byte* buffer
            , std::size_t size
            , const char* extensionHint
            , ImageLoadFlags imageLoadFlags
            , ImageLoaderFlags imageLoaderFlags
            , ImageSharedPtr& image) const override;
        
    public: // methods
        ImageLoader();
        void InstallPlugin(IImagePlugin* plugin);
    };
}
