#include <ImageMetaDataLoader.h>
#include <TinyEXIF.h>
namespace IMCodec
{
    ImageResult ImageMetaDataLoader::LoadMetaData(const std::byte* buffer, size_t size, ItemMetaDataSharedPtr& out_metaData)
    {
        using namespace TinyEXIF;
        ImageResult result = ImageResult::UnknownError;
        TinyEXIF::EXIFInfo exifInfo(reinterpret_cast<const uint8_t*>(buffer), static_cast<unsigned int>(size));

        if (exifInfo.Fields != 0)
        {
            out_metaData = std::make_shared<ItemMetaData>();
            ExifData& exifData = out_metaData->exifData;
            exifData.orientation = exifInfo.Orientation;
            if (exifInfo.GeoLocation.hasLatLon())
            {
                exifData.latitude = exifInfo.GeoLocation.Latitude;
                exifData.longitude = exifInfo.GeoLocation.Longitude;
            }

            if (exifInfo.GeoLocation.hasAltitude())
                exifData.altitude = exifInfo.GeoLocation.Altitude;

            if (exifInfo.GeoLocation.hasRelativeAltitude())
                exifData.relativeAltitude = static_cast<int8_t>(exifInfo.GeoLocation.RelativeAltitude);


            exifData.flash.flashValue = exifInfo.Flash;

            exifData.make = exifInfo.Make;
            exifData.model = exifInfo.Model;
            exifData.software = exifInfo.Software;
            exifData.copyright = exifInfo.Copyright;
            result = ImageResult::Success;
        }
        else
        {
            result = ImageResult::BadParameters;
        }
        return result;
    }
};
