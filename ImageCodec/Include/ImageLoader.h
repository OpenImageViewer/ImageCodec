#pragma once
#include <map>
#include <algorithm>
#include <filesystem>
#include <LLUtils/StringUtility.h>
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
        NoTraverse = 0 << 0

        // Traverse suitable plugins in reference to the file extension.
        ,
        SuitablePlugin = 1 << 0

        // Traverse all plugins.
        ,
        AnyPlugin = 1 << 1

        // Traverse only for known file type
        ,
        OnlyKnownFileType = 1 << 2

        // Traverse all installed plugin if first attempt to load the file has failed.
        ,
        AnyFileType = 1 << 3
    };

    LLUTILS_DEFINE_ENUM_CLASS_FLAG_OPERATIONS(PluginTraverseMode)

    class ImageLoader
    {
      private:

        ImageCodec fImageCodec;
        ImageMetaDataLoader fImageMetaDataLoader;
        using ListPluginID = std::vector<PluginID>;
        ListPluginID fListPlugins;
        using MapStringListPlugin = std::map<string_type, ListPluginID>;
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
                        auto it                 = fMapPlugins.find(extensionLowerCase);
                        if (it == fMapPlugins.end())
                            it = fMapPlugins.emplace(extensionLowerCase, ListPluginID{}).first;

                        ListPluginID& pluginsList = it->second;

                        pluginsList.push_back(plugin.id);
                    }
                }
            }
        }

        PluginID GetFirstPlugin(const string_type& extension) const
        {
            using namespace std;
            PluginID result{};
            auto it = fMapPlugins.find(extension);
            if (it != fMapPlugins.end())
            {
                const ListPluginID& pluginsList = it->second;
                result                          = *pluginsList.begin();
            }

            return result;
        }

        ImageCodec& GetImageCodec() { return fImageCodec; }

        ImageResult Decode(const std::byte* buffer, std::size_t size, ImageLoadFlags imageLoadFlags,
                           const Parameters& params, const string_type& extension, PluginTraverseMode traverseMode,
                           ImageSharedPtr& out_image)
        {
            ImageResult result = ImageResult::UnknownError;
            PluginID choosenPlugin{};

            const auto compatible = fMapPlugins.find(LLUtils::StringUtility::ToLower<string_type>(extension));
            if (compatible != fMapPlugins.end())
                choosenPlugin = compatible->second.front();

            if (choosenPlugin != PluginID())
                result = fImageCodec.Decode(buffer, size, choosenPlugin, imageLoadFlags, params, out_image);

            const bool isKnownFileType = choosenPlugin != PluginID{};

            const bool shouldTraversPlugins = result != ImageResult::Success  // 1. File hasn't been loaded
                                              &&
                                              ((traverseMode & PluginTraverseMode::AnyFileType) ==
                                                   PluginTraverseMode::AnyFileType  // 2. Allow non registered extension
                                               || (isKnownFileType &&
                                                   (traverseMode & PluginTraverseMode::OnlyKnownFileType) ==
                                                       PluginTraverseMode::OnlyKnownFileType));

            if ((traverseMode & PluginTraverseMode::SuitablePlugin) == PluginTraverseMode::SuitablePlugin)
            {
                // Suitable traversal is restricted to this extension, even with broader flags present.
                // Unsupported formats may fall through, while corruption and parameter errors stay visible.
                const auto unsupported = [](ImageResult value)
                { return value == ImageResult::FormatNotSupported || value == ImageResult::NotImplemented; };
                if (isKnownFileType)
                {
                    const auto& candidates = compatible->second;
                    for (size_t index = 1; index < candidates.size() && unsupported(result); ++index)
                    {
                        // Extension aliases may register one decoder more than once.
                        if (std::none_of(candidates.begin(), candidates.begin() + index,
                                         [&](const PluginID& previous) { return previous == candidates[index]; }))
                        {
                            ImageSharedPtr candidate;
                            result = fImageCodec.Decode(buffer, size, candidates[index], imageLoadFlags, params,
                                                        candidate);
                            if (result == ImageResult::Success)
                                out_image = std::move(candidate);
                        }
                    }
                }
            }
            else if (shouldTraversPlugins)
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

        ImageResult Decode(const string_type& filePath, ImageLoadFlags imageLoadFlags, const Parameters& params,
                           PluginTraverseMode traverseMode, ImageSharedPtr& out_image)
        {
            using namespace LLUtils;
            const std::filesystem::path path(
                StringUtility::ConvertString<std::filesystem::path::string_type>(filePath));
            string_type extension = StringUtility::ConvertString<string_type>(path.extension().native());
            if (!extension.empty())
                extension.erase(0, 1);
            FileMapping fileMapping(filePath);
            const auto* buffer = static_cast<const std::byte*>(fileMapping.GetBuffer());
            const auto size    = fileMapping.GetSize();
            const auto result  = buffer && size
                                     ? Decode(buffer, size, imageLoadFlags, params, extension, traverseMode, out_image)
                                     : ImageResult::BadParameters;
            return result;
        }

        ImageResult Encode(const ImageSharedPtr image, const string_type& extension, LLUtils::Buffer& encoded)
        {
            auto choosenPlugin = GetFirstPlugin(LLUtils::StringUtility::ToLower(extension));

            if (choosenPlugin != PluginID{})
            {
                Parameters encoderParameters;

                ListParameterDescriptors parametersDescriptors;

                return fImageCodec.Encode(image, choosenPlugin, encoderParameters, encoded);
            }
            return ImageResult::NotImplemented;
        }

        ImageResult Encode(const ImageSharedPtr image, const string_type& filePath)
        {
            string_type extension = LLUtils::StringUtility::ToLower(
                std::filesystem::path(filePath).extension().string<char_type>());
            std::basic_string_view<char_type> sv(extension);

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
        ImageResult LoadMetaData(LLUtils::native_string_type filePath, ItemMetaDataSharedPtr& out_metaData)
        {
            using namespace LLUtils;
            FileMapping fileMapping(filePath);
            const void* buffer = fileMapping.GetBuffer();
            size_t size        = fileMapping.GetSize();
            return LoadMetaData(static_cast<const std::byte*>(buffer), size, out_metaData);
        }
    };
}  // namespace IMCodec