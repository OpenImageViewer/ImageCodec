#include <Image.h>
#include <ImageCodec.h>
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

    //// Decode memory buffer
    ImageResult ImageCodec::Decode(const std::byte* buffer
        , std::size_t size
        , const PluginID& pluginID
        , ImageLoadFlags imageLoadFlags
        , const Parameters& params
        , ImageSharedPtr& out_image)
    {
        auto plugin = GetPluginByID(pluginID);
        if (plugin != nullptr)
        {
            LLUtils::StopWatch stopWatch(true);

            ImageResult result = plugin->Decode(buffer, size, imageLoadFlags, params, out_image);
            auto loadTime = stopWatch.GetElapsedTimeReal(LLUtils::StopWatch::TimeUnit::Milliseconds);

            if (result == ImageResult::Success)
            {
                auto& processData = out_image->GetImageItem()->processData;
                processData.processTime = static_cast<double>(loadTime);
                processData.pluginUsed = pluginID;
            }
            return result;
        }
        else
            return ImageResult::NotFound;
    }

    ImageResult ImageCodec::GetEncoderParameters(const PluginID& pluginID, ListParameterDescriptors& out_encodeParameters)
    {
        auto plugin = GetPluginByID(pluginID);
        if (plugin != nullptr)
            return plugin->GetEncoderParameters(out_encodeParameters);
        else
            return ImageResult::NotFound;
    }

    IImagePlugin* ImageCodec::GetPluginByID(const PluginID& pluginId)
    {
        auto it = fMapPluginIDPlugin.find(pluginId);
        return it != fMapPluginIDPlugin.end() ? it->second : nullptr;
    }

    ImageResult ImageCodec::Encode(const ImageSharedPtr image, const PluginID& pluginID, const Parameters& params, LLUtils::Buffer& encoded)
    {
        auto plugin = GetPluginByID(pluginID);
        if (plugin != nullptr)
            return plugin->Encode(image, params, encoded);
        else
            return ImageResult::NotFound;
    }

    ImageResult ImageCodec::InstallPlugin(IImagePlugin* plugin)
    {
        using namespace LLUtils;
        fListPlugins.push_back(plugin);


        //for (const auto& fileExtensions : plugin->GetPluginProperties().extensionCollection)
        //{
        //    for (const auto& extension : fileExtensions.listExtensions)
        //    {
        //        auto extensionLowerCase = StringUtility::ToLower(extension);
        //        auto it = fMapPlugins.find(extensionLowerCase);
        //        if (it == fMapPlugins.end())
        //            it = fMapPlugins.emplace(extensionLowerCase, ListPlugin{}).first;

        //        ListPlugin& pluginsList = it->second;
        //        
        //        
        //    }
        //}

        auto it = fMapPluginIDPlugin.emplace(plugin->GetPluginProperties().id, plugin);

        if (it.second == false)
            LL_EXCEPTION(LLUtils::Exception::ErrorCode::DuplicateItem, "duplicates plugins cannot coexist");

        return ImageResult::Success;
    }

    ImageResult ImageCodec::InstallPlugin(const std::wstring& pluginFilePath)
    {
        return ImageResult::NotImplemented;
    }

    std::vector<PluginProperties> ImageCodec::GetPluginsInfo()
    {
        std::vector<PluginProperties> result(fListPlugins.size());

        for (size_t i = 0; i < fListPlugins.size(); i++)
            result.at(i) = fListPlugins.at(i)->GetPluginProperties();

        return result;
    }


    ImageResult ImageCodec::GetPluginInfo(const PluginID& pluginID, PluginProperties& pluginProperties)
    {
        auto it = fMapPluginIDPlugin.find(pluginID);
        if (it != std::end(fMapPluginIDPlugin))
        {
            pluginProperties = it->second->GetPluginProperties();
            return ImageResult::Success;
        }

        return ImageResult::NotFound;
    }


    /*std::wstring parseType(std::wstring type)
    {
        if (type == L"double")
            return L"number";
        else
            return type;
    }*/

    /*  ImageResult ImageCodec::EncodeImpl(const ImageSharedPtr image, const std::wstring& extension, LLUtils::Buffer& encoded)
      {
          auto choosenPlugin = GetFirstPlugin(LLUtils::StringUtility::ToLower(extension));

          if (choosenPlugin != nullptr)
          {
              Parameters encoderParameters;

              ListParameterDescriptors parametersDescriptors;

  #if IMCODEC_NETSETTINGS_EXTENSION_EXISTS
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
                      ss << "\"displayname\": " << "\"" << desc.displayName << "\"," << L'\n';
                      ss << "\"description\": " << "\"" << desc.description << "\"," << L'\n';
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
  #endif
              return choosenPlugin->Encode(image, encoderParameters, encoded);
          }
          return ImageResult::NotImplemented;
      }*/

#if IMCODEC_NETSETTINGS_EXTENSION_EXISTS
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
#endif
}