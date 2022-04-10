#include "ImageLoader.h"
#include "EmbeddedPluginInstaller.h"
#include <LLUtils/StringUtility.h>
#include <LLUtils/StopWatch.h>
//#include <TinyEXIF.h>

namespace IMCodec
{

    ImageLoader::ImageLoader()
    {
        EmbeddedPluginInstaller::InstallPlugins(this);
    }

    IImagePlugin* ImageLoader::GetFirstPlugin(const std::wstring& hint) const
    {
        using namespace std;
        IImagePlugin* result = nullptr;
        auto it = fMapPlugins.find(hint);
        if (it != fMapPlugins.end())
        {
            const ListPlugin& pluginsList = it->second;
            result = *pluginsList.begin();
        }

        return result;
    }

    void ImageLoader::InstallPlugin(IImagePlugin* plugin)
    {
        using namespace LLUtils;
        fListPlugins.push_back(plugin);
        ListWString tokens = StringUtility::split(StringUtility::ToLower(
            StringUtility::ToWString(plugin->GetPluginProperties().supportedExtentions)), L';');

        for (auto token : tokens)
        {
            auto it = fMapPlugins.find(token);
            if (it == fMapPlugins.end())
                it = fMapPlugins.insert(std::make_pair(token, ListPlugin())).first;

            ListPlugin& pluginsList = it->second;
            pluginsList.push_back(plugin);
        }
    }

    ImageResult ImageLoader::TryLoad(IImagePlugin* plugin, const std::byte* buffer, std::size_t size, ImageLoadFlags loadFlags, ImageSharedPtr& image) const
    {
        LLUtils::StopWatch stopWatch(true);
        ImageResult result = plugin->LoadMemoryImageFile(buffer, size, loadFlags, image);
        auto loadTime = stopWatch.GetElapsedTimeReal(LLUtils::StopWatch::TimeUnit::Milliseconds);

        if (result == ImageResult::Success)
        {
            if ((image->IsValid() == true)) // verify image is properly initialized
            {
                auto& runtimeData = const_cast<ItemRuntimeData&>(image->GetRuntimeData());
                runtimeData.loadTime = loadTime;
                runtimeData.pluginUsed = plugin->GetPluginProperties().pluginDescription.c_str();
            }
        }
        return result;
    }

    ImageResult ImageLoader::Load(const std::byte* buffer
        , std::size_t size
        , const char* extensionHint
        , ImageLoadFlags imageLoadFlags
        , ImageLoaderFlags imageLoaderFlags
        , ImageSharedPtr& image) const
    {

        ImageResult result = ImageResult::Fail;

        IImagePlugin* choosenPlugin = nullptr;

        if (extensionHint != nullptr)
            choosenPlugin = GetFirstPlugin(LLUtils::StringUtility::ToLower(LLUtils::StringUtility::ToWString(extensionHint)));

        if (choosenPlugin != nullptr)
            result = TryLoad(choosenPlugin, buffer, size,imageLoadFlags, image);

        
        // If image not loaded and allow to load using unregistred file extensions, iterate over all image plugins.
        if (result != ImageResult::Success && ( (imageLoaderFlags & ImageLoaderFlags::OnlyRegisteredExtension) == ImageLoaderFlags::None))
        {
            for (auto plugin : fListPlugins)
            {
                // In case we try to a choosen plugin and it failed. don't try again.
                if (plugin != choosenPlugin)
                {
                    result = TryLoad(plugin, buffer, size,imageLoadFlags, image);
                    if (result == ImageResult::Success)
                        break;
                }
            }
        }

        return result;
    }

    std::wstring ImageLoader::GetKnownFileTypes() const
    {
        std::wstringstream ss;
        for (const auto& pair : fMapPlugins)
            ss << pair.first << ";";

        return ss.str();

    }
}
