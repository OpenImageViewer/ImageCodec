#pragma once
#include <map>
#include "Image.h"
#include "ImageCodec.h"
#include "ImageMetaDataLoader.h"
#include <LLUtils/EnumClassBitwise.h>
#include <LLUtils/FileMapping.h>
#include <LLUtils/FileHelper.h>

/// <summary>
/// ImageLoader is a helper class that currently use ImageCodec for decoding and encoding image,
/// and ImageMetaDataLoader for loading image meta data
/// </summary>

namespace IMCodec
{
    enum class PluginTraverseMode
    {
        // Try load the file only using the first compatible plugin
          NoTraverse        = 0 << 0
        
        // Traverse suitable plugins in reference to the file extension.
        , SuitablePlugin    = 1 << 0

        // Traverse all plugins.
        , AnyPlugin         = 1 << 1
        
        // Traverse only for known file type
        , OnlyKnownFileType = 1 << 2

        // Traverse all installed plugin if first attempt to load the file has failed.
        , AnyFileType       = 1 << 3
    };

    LLUTILS_DEFINE_ENUM_CLASS_FLAG_OPERATIONS(PluginTraverseMode)
    
	class ImageLoader
	{
    private:
        ImageCodec fImageCodec;
        ImageMetaDataLoader fImageMetaDataLoader;
        using ListPluginID = std::vector<PluginID>;
        ListPluginID fListPlugins;
        using MapStringListPlugin = std::map<std::wstring, ListPluginID>;
        MapStringListPlugin fMapPlugins;
        
        
    public:
        ImageLoader() 
        {
            using namespace LLUtils;
            auto pluginsInfo = fImageCodec.GetPluginsInfo();

            for (const auto& plugin : pluginsInfo)
            {
                fListPlugins.push_back(plugin.id);
                for (const auto& fileExtensions : plugin.extensionCollection)
                {
                    for (const auto& extension : fileExtensions.listExtensions)
                    {
                        auto extensionLowerCase = StringUtility::ToLower(extension);
                        auto it = fMapPlugins.find(extensionLowerCase);
                        if (it == fMapPlugins.end())
                            it = fMapPlugins.emplace(extensionLowerCase, ListPluginID{}).first;

                        ListPluginID& pluginsList = it->second;

                        pluginsList.push_back(plugin.id);
                    }
                }
            }

        }
     
        PluginID GetFirstPlugin(const std::wstring& extension) const
        {
            using namespace std;
            PluginID result{};
            auto it = fMapPlugins.find(extension);
            if (it != fMapPlugins.end())
            {
                const ListPluginID& pluginsList = it->second;
                result = *pluginsList.begin();
            }

            return result;
        }

        ImageCodec& GetImageCodec()
        {
            return fImageCodec;
        }


        ImageResult Decode(const std::byte* buffer
            , std::size_t size
            , ImageLoadFlags imageLoadFlags
            , const Parameters& params
            , const std::wstring& extension
            , PluginTraverseMode traverseMode
            , ImageSharedPtr& out_image)
        {
            ImageResult result = ImageResult:: UnknownError;
            PluginID choosenPlugin{};


             if (extension.empty() == false)
                  choosenPlugin = GetFirstPlugin(LLUtils::StringUtility::ToLower<std::wstring>(extension));

             if (choosenPlugin != PluginID())
                    result = fImageCodec.Decode(buffer, size, choosenPlugin, imageLoadFlags, params, out_image);

             const bool isKnownFileType = choosenPlugin != PluginID{};

             const bool shouldTraversPlugins = result != ImageResult::Success // 1. File hasn't been loaded
                 && ( (traverseMode & PluginTraverseMode::AnyFileType) == PluginTraverseMode::AnyFileType// 2. Allow non registered extension
                  ||  (isKnownFileType && (traverseMode & PluginTraverseMode::OnlyKnownFileType) == PluginTraverseMode::OnlyKnownFileType));
                 
             const bool shouldTraveseOnlySuitable = (traverseMode & PluginTraverseMode::SuitablePlugin) == PluginTraverseMode::SuitablePlugin;

                // If image not loaded and allow to load using unregistred file extensions, iterate over all image plugins.
                if (shouldTraversPlugins)
                {
                    for (auto plugin : fListPlugins)
                    {
                        // In case we try to a choosen plugin and it failed. don't try again.
                        if (plugin != choosenPlugin)
                        {
                            result = fImageCodec.Decode(buffer, size, plugin, imageLoadFlags, params, out_image);
                            if (result == ImageResult::Success)
                                break;
                        }
                    }
                }

                return result;
        }


        ImageResult Decode(const std::wstring& filePath 
            , ImageLoadFlags imageLoadFlags
            , const Parameters& params
            , PluginTraverseMode traverseMode
            , ImageSharedPtr& out_image)
        {

            using namespace LLUtils;
                    FileMapping fileMapping(filePath);
                    const void* buffer = fileMapping.GetBuffer();
                    std::size_t size = fileMapping.GetSize();
                    std::wstring extension = StringUtility::ConvertString<std::wstring>(StringUtility::GetFileExtension<std::wstring>(filePath));
            
                    ImageResult result = ImageResult::UnknownError;
                    if (buffer != nullptr && size != 0)
                    {
                        return this->Decode(static_cast<const std::byte*>(buffer), size,imageLoadFlags,params, extension, traverseMode,out_image);
                    }
                    else
                    {
                        result = ImageResult::BadParameters;
                    }
            
                    return result;
        }


     ImageResult Encode(const ImageSharedPtr image, const std::wstring& extension, LLUtils::Buffer& encoded)
    {
        auto choosenPlugin = GetFirstPlugin(LLUtils::StringUtility::ToLower(extension));

        if (choosenPlugin != PluginID{})
        {
            Parameters encoderParameters;

            ListParameterDescriptors parametersDescriptors;
            
            return fImageCodec.Encode(image,choosenPlugin, encoderParameters, encoded);
        }
        return ImageResult::NotImplemented;
    }


     ImageResult Encode(const ImageSharedPtr image, const std::wstring& filePath)
     {
         std::wstring extension = LLUtils::StringUtility::ToLower(std::filesystem::path(filePath).extension().wstring());
         std::wstring_view sv(extension);

         if (sv.empty() == false)
             sv = sv.substr(1);

         LLUtils::Buffer encodedBuffer;
         ImageResult result = Encode(image, sv.data(), encodedBuffer);
         if (result == ImageResult::Success)
             LLUtils::File::WriteAllBytes(filePath, encodedBuffer.size(), encodedBuffer.data());

         return result;
     }

     ImageResult LoadMetaData(const std::byte* buffer, size_t size, ItemMetaDataSharedPtr& out_metaData)
     {
          return fImageMetaDataLoader.LoadMetaData(buffer, size, out_metaData);
     }
     ImageResult LoadMetaData(std::wstring filePath, ItemMetaDataSharedPtr& out_metaData)
     {
         using namespace LLUtils;
         FileMapping fileMapping(filePath);
         const void* buffer = fileMapping.GetBuffer();
         size_t size = fileMapping.GetSize();
         return LoadMetaData(static_cast<const std::byte*>(buffer), size, out_metaData);
     }
	};
}