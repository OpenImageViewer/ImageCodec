#include "EmbeddedPluginInstaller.h"
#include "ImageCodec.h"

//TODO: Take configuration out to a new file
#if IMCODEC_BUILD_CODEC_PSD == 1
    #include "../../Codecs/CodecPSD/Include/CodecPSDFactory.h"
#endif

#if IMCODEC_BUILD_CODEC_BMP == 1
#include "../../Codecs/CodecBMP/Include/CodecBMPFactory.h"
#endif

#if IMCODEC_BUILD_CODEC_JPG == 1
    #include "../../Codecs/CodecJPG/Include/CodecJPGFactory.h"
#endif

#if IMCODEC_BUILD_CODEC_PNG == 1
    #include "../../Codecs/CodecPNG/Include/CodecPNGFactory.h"
#endif

#if IMCODEC_BUILD_CODEC_DDS == 1
#include "../../Codecs/CodecDDS/Include/CodecDDSFactory.h"
#endif

#if IMCODEC_BUILD_CODEC_FREEIMAGE == 1
    #include "../../Codecs/CodecFreeImage/Include/CodecFreeImageFactory.h"
#endif

#if IMCODEC_BUILD_CODEC_TIFF == 1
#include "../../Codecs/CodecTiff/Include/CodecTiffFactory.h"
#endif

#if IMCODEC_BUILD_CODEC_GIF == 1
#include "../../Codecs/CodecGif/Include/CodecGifFactory.h"
#endif

#if IMCODEC_BUILD_CODEC_WEBP == 1
#include "../../Codecs/CodecWebP/Include/CodecWebPFactory.h"
#endif

#if IMCODEC_BUILD_CODEC_ICON == 1
#include "../../Codecs/CodecIcon/Include/CodecIconFactory.h"
#endif

namespace IMCodec
{
    bool EmbeddedPluginInstaller::InstallPlugins(ImageCodec* imageLoader)
    {
        // install codec by priority, first installed is with the higher priority.
#if IMCODEC_BUILD_CODEC_BMP == 1
        imageLoader->InstallPlugin(CodecBMPFactory::Create());
#endif
#if IMCODEC_BUILD_CODEC_PSD == 1
        imageLoader->InstallPlugin(CodecPSDFactory::Create());
#endif
#if IMCODEC_BUILD_CODEC_JPG == 1
        imageLoader->InstallPlugin(CodecJPGFactory::Create());
#endif
#if IMCODEC_BUILD_CODEC_PNG == 1
        imageLoader->InstallPlugin(CodecPNGFactory::Create());
#endif
#if IMCODEC_BUILD_CODEC_DDS == 1
        imageLoader->InstallPlugin(CodecDDSFactory::Create());
#endif
#if IMCODEC_BUILD_CODEC_TIFF == 1
        imageLoader->InstallPlugin(CodecTiffFactory::Create());
#endif
#if IMCODEC_BUILD_CODEC_GIF == 1
        imageLoader->InstallPlugin(CodecGifFactory::Create());
#endif

#if IMCODEC_BUILD_CODEC_WEBP == 1
        imageLoader->InstallPlugin(CodecWebPFactory::Create());
#endif

#if IMCODEC_BUILD_CODEC_ICON == 1
        imageLoader->InstallPlugin(CodecIconFactory::Create(imageLoader));
#endif
    	
// keep freeimage the last priority codec as it's inferior
#if IMCODEC_BUILD_CODEC_FREEIMAGE == 1
        imageLoader->InstallPlugin(CodecFreeImageFactory::Create());
#endif
        return true;
    }
}
