#pragma once

namespace IMCodec
{
    enum class ImageResult
    {
          Success
        , FileIsCorrupted
        , NotImplemented
        , FormatNotSupported
        , BadParameters
        , NotFound
        , UnknownError
    };

    struct PluginID
    {
        unsigned long  Data1;
        unsigned short Data2;
        unsigned short Data3;
        unsigned char  Data4[8];
        std::strong_ordering operator<=>(const PluginID& rhs) const = default;
    };
}