#include "ImageLoader.h"
#include "EmbeddedPluginInstaller.h"
#include <LLUtils/StringUtility.h>
#include <LLUtils/StopWatch.h>
#include <TinyEXIF.h>

namespace IMCodec
{

    ImageCodec::ImageCodec()
    {
        EmbeddedPluginInstaller::InstallPlugins(this);
    }


    std::wstring parseType(std::wstring type)
    {
        if (type == L"double")
            return L"number";
        else
            return type;
    }

     ImageResult ImageCodec::Encode(const ImageSharedPtr image, const std::wstring& extension, LLUtils::Buffer& encoded) 
     {
        auto choosenPlugin = GetFirstPlugin(LLUtils::StringUtility::ToLower(extension));

        if (choosenPlugin != nullptr)
        {
            Parameters encoderParameters;

            ListParameterDescriptors parametersDescriptors;
            
            if (false)
            if (choosenPlugin->GetEncoderParameters(parametersDescriptors) == ImageResult::Success)
            {

                std::wstringstream ss;
                ss << L'{' << L'\n';
                ss << L"\"type\": \"root\"" << L'\n';
                ss << L",\"subitems\" : [" << L'\n';


                for (const auto& desc : parametersDescriptors)
                {
                    ss << '{' << L'\n';
                    ss << "\"type\": " << "\"" << parseType(desc.type) << "\"," << L'\n';
                    ss << "\"name\": " << "\"" << desc.displayName << "\"," << L'\n';
                    ss << "\"displayname\": " <<  "\"" << desc.displayName  << "\"," << L'\n';
                    ss << "\"description\": " <<  "\"" << desc.description  << "\"," << L'\n';
                    if (parseType(desc.type) == L"number")
                        ss << "\"defaultvalue\": " << desc.defaultValue << L"\n";
                    else
                        ss << "\"defaultvalue\": " << "\"" << desc.defaultValue << "\"" << L'\n';
                    ss << '}' << L'\n';
                    ss << ',' << L'\n';
                }


                if (ss.rdbuf()->in_avail() > 0)
                    ss.seekp(-2, std::ios_base::cur);
                
                ss << L"]" << L'\n';
                ss << L"}" << L'\n';

                std::wstring jsonString = ss.str();

                InitializeNetSettings();
                if (settingsContext.created)
                {
                    settingsContext.SetVisible(true);
                }
            }

            return choosenPlugin->Encode(image, encoderParameters, encoded);
        }
        return ImageResult::NotImplemented;
    }

     void ImageCodec::NetSettingsCallback_(ItemChangedArgs* args)
     {
         reinterpret_cast<ImageCodec*>(args->userData)->NetSettingsCallback(args);
     }

     void ImageCodec::NetSettingsCallback(ItemChangedArgs* args)
     {
         
     }


     void ImageCodec::InitializeNetSettings()
     {
         using namespace std::filesystem;
         const path programPath = path(LLUtils::PlatformUtility::GetExeFolder());
         const path netsettingsPath = programPath / "Extensions" / "NetSettings";
         const path cliAdapterPath = netsettingsPath / "CliAdapter.dll";

         if (exists(cliAdapterPath))
         {
             SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
             [[maybe_unused]] auto directory = AddDllDirectory((netsettingsPath.lexically_normal().wstring() + L"\\").c_str());
             HMODULE dllModule = LoadLibrary(cliAdapterPath.c_str());
             if (dllModule != nullptr)
             {
                 settingsContext.Create = reinterpret_cast<netsettings_Create_func>(GetProcAddress(dllModule, "netsettings_Create"));
                 settingsContext.SetVisible = reinterpret_cast<netsettings_SetVisible_func>(GetProcAddress(dllModule, "netsettings_SetVisible"));
                 settingsContext.SaveSettings = reinterpret_cast<netsettings_SaveSettings_func>(GetProcAddress(dllModule, "netsettings_SaveUserSettings"));

                 GuiCreateParams params{};
                 params.userData = this;
                 params.callback = &ImageCodec::NetSettingsCallback_;
                 auto templateFile = (netsettingsPath / "Resources/GuiTemplate.json");
                 params.templateFilePath = templateFile.c_str();
                 auto userSettingsFile = (programPath / "Resources/Configuration/Settings.json");
                 params.userSettingsFilePath = userSettingsFile.c_str();

                 settingsContext.Create(&params);
                 settingsContext.SetVisible(true);
                 settingsContext.created = true;
             }
         }
     }


    IImagePlugin* ImageCodec::GetFirstPlugin(const std::wstring& hint) const
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

    void ImageCodec::InstallPlugin(IImagePlugin* plugin)
    {
        using namespace LLUtils;
        fListPlugins.push_back(plugin);

        for (const auto& fileExtensions : plugin->GetPluginProperties().extensionCollection)
        {
            for (const auto& extension : fileExtensions.listExtensions)
            {
                auto extensionLowerCase = StringUtility::ToLower(extension);
                auto it = fMapPlugins.find(extensionLowerCase);
                if (it == fMapPlugins.end())
                    it = fMapPlugins.emplace(extensionLowerCase, ListPlugin{}).first;
                
                ListPlugin& pluginsList = it->second;
                
                pluginsList.push_back(plugin);
            }
        }
    }

    ImageResult ImageCodec::TryLoad(IImagePlugin* plugin, const std::byte* buffer, std::size_t size, ImageLoadFlags loadFlags
        , const Parameters& params ,ImageSharedPtr& image) const
    {
        LLUtils::StopWatch stopWatch(true);
        ImageResult result = plugin->Decode(buffer, size, loadFlags, params, image);
        auto loadTime = stopWatch.GetElapsedTimeReal(LLUtils::StopWatch::TimeUnit::Milliseconds);

        if (result == ImageResult::Success)
        {
            auto& runtimeData = const_cast<ItemRuntimeData&>(image->GetRuntimeData());
            runtimeData.loadTime = loadTime;
            runtimeData.pluginUsed = plugin->GetPluginProperties().pluginDescription.c_str();

        }
        return result;
    }

    ImageResult ImageCodec::Decode(const std::byte* buffer
        , std::size_t size
        , const char* extensionHint
        , ImageLoadFlags imageLoadFlags
        , ImageLoaderFlags imageLoaderFlags
        , const Parameters& params
        , ImageSharedPtr& image) const
    {
        ImageResult result = ImageResult::Fail;
        IImagePlugin* choosenPlugin = nullptr;

        if (extensionHint != nullptr)
            choosenPlugin = GetFirstPlugin(LLUtils::StringUtility::ToLower(LLUtils::StringUtility::ToWString(extensionHint)));

        if (choosenPlugin != nullptr)
            result = TryLoad(choosenPlugin, buffer, size,imageLoadFlags,params, image);

        const bool shouldTraversPlugins = result != ImageResult::Success // 1. File hasn't been loaded
            && (((imageLoaderFlags & ImageLoaderFlags::OnlyRegisteredExtension) == ImageLoaderFlags::None) // 2. Allow non registered extension
                || (( (imageLoaderFlags & ImageLoaderFlags::OnlyRegisteredExtensionRelaxed) == ImageLoaderFlags::OnlyRegisteredExtensionRelaxed) && choosenPlugin != nullptr) // 3. It's a registered extension, but failed to load.
                );
        
        // If image not loaded and allow to load using unregistred file extensions, iterate over all image plugins.
        if (shouldTraversPlugins)
        {
            for (auto plugin : fListPlugins)
            {
                // In case we try to a choosen plugin and it failed. don't try again.
                if (plugin != choosenPlugin)
                {
                    result = TryLoad(plugin, buffer, size,imageLoadFlags,params, image);
                    if (result == ImageResult::Success)
                        break;
                }
            }
        }

        if (result == ImageResult::Success)
        {
            TinyEXIF::EXIFInfo exifInfo(reinterpret_cast<const uint8_t*>(buffer), size);
            if (exifInfo.Fields)
            {
                ExifData& exifData = const_cast<ItemMetaData&>(image->GetImageItem()->metaData).exifData;
                exifData.orientation = exifInfo.Orientation;
                if (exifInfo.GeoLocation.hasLatLon())
                {
                    exifData.latitude = exifInfo.GeoLocation.Latitude;
                    exifData.longitude = exifInfo.GeoLocation.Longitude;
                }

                if (exifInfo.GeoLocation.hasAltitude())
                    exifData.altitude = exifInfo.GeoLocation.Altitude;

                if (exifInfo.GeoLocation.hasRelativeAltitude())
                    exifData.relativeAltitude= exifInfo.GeoLocation.RelativeAltitude;


                exifData.flash.flashValue = exifInfo.Flash;
                
                exifData.make = exifInfo.Make;
                exifData.model = exifInfo.Model;
                exifData.software = exifInfo.Software;
                exifData.copyright = exifInfo.Copyright;
            }
        }
		
        return result;
    }
    /// <summary>
    /// Get codecs information. codecs are orderd by priority.
    /// </summary>
    /// <returns></returns>
    std::vector<PluginProperties> ImageCodec::GetCodecsInfo()
    {
        std::vector<PluginProperties> result(fListPlugins.size());
        
        for (size_t i = 0; i < fListPlugins.size(); i++)
            result.at(i) = fListPlugins.at(i)->GetPluginProperties();

        return result;
    }

    std::wstring ImageCodec::GetKnownFileTypes() const
    {
        std::wstringstream ss;
        for (const auto& pair : fMapPlugins)
            ss << pair.first << ";";

        return ss.str();

    }
}
