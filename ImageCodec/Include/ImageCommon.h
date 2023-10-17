#pragma once
#include <LLUtils/StringDefs.h>

namespace IMCodec
{
    using string_type = LLUtils::native_string_type;
    using char_type = LLUtils::native_char_type;
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