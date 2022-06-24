#pragma once
namespace IMCodec
{
    class ImageCodec;
    class EmbeddedPluginInstaller
    {
    public:
        static bool InstallPlugins(ImageCodec* imageLoader);
        
    };
}