#pragma once
#include <cstdint>
#include <LLUtils/Platform.h>
#include <LLUtils/Exception.h>

namespace IMCodec
{
    enum class ChannelSemantic
    {
          None
        , Red
        , Green
        , Blue
        , Opacity
        , Float
        , Monochrome
    };

    enum class ChannelDataType
    {
          None
        , UnsignedInt
        , SignedInt
        , Float
    };

    using ChannelWidth = uint8_t;

    struct ChannelInfo
    {
        ChannelDataType ChannelDataType;
        ChannelSemantic semantic;
        ChannelWidth width;
    };

    template <typename First, typename ...Args>
    class _TexelInfo
    {

    public:
        uint8_t texelSize = 0;
        uint8_t numChannles = 0;
        static constexpr std::size_t channels_capacity = sizeof...(Args) + 1;


        std::array<ChannelInfo, channels_capacity> channles;

        _TexelInfo(First first, Args ...args)
        {
            Init<0>(first, args...);
        }

        _TexelInfo(const std::initializer_list<ChannelInfo>& initList)
        {
            int i = 0;
            for (const auto& e : initList)
            {
                texelSize += e.width;
                channles.at(i++) = e;
            }
            numChannles = static_cast<uint8_t>(initList.size());
        }

        template <size_t count, typename _First, typename ..._Args>
        void Init(_First first, _Args ...args)
        {
            channles.at(count) = first;
            texelSize += first.width;
            Init<count + 1>(args...);
        }

        template <size_t count, typename _First>
        void Init(_First first)
        {
            channles.at(count) = first;
            numChannles = count + 1;
            texelSize += first.width;
        }
    };

    enum class TexelFormat : uint16_t
    {
          BEGIN
        , UNKNOWN = BEGIN
        , I_R8_G8_B8
        , I_R16_G16_B16
        , I_R8_G8_B8_A8
        , I_R16_G16_B16_A16
        , I_B8_G8_R8
        , I_B5_G5_R5_X1 // 16 bit
        , I_B5_G6_R5    // 16 bit
        , I_B16_G16_R16
        , I_B8_G8_R8_A8
        , I_B16_G16_R16_A16
        , I_A8_R8_G8_B8
        , I_A16_R16_G16_B16
        , I_A8_B8_G8_R8
        , I_A16_B16_G16_R16
        , I_A8
        , I_X1
        , I_X4
        , I_X8
        , I_X16
        , S_X8
        , S_X16
        , F_X16
        , F_X24
        , F_X32
        , F_X64
        , COUNT
    };

    using TexelInfo = _TexelInfo<ChannelInfo, ChannelInfo, ChannelInfo, ChannelInfo>;

    const TexelInfo TexelFormatInfo[]
    {

     {}       // TF_UNKNOWN
     ,{ {ChannelDataType::UnsignedInt, ChannelSemantic::Red,8 },         {ChannelDataType::UnsignedInt, ChannelSemantic::Green,8} ,    {ChannelDataType::UnsignedInt, ChannelSemantic::Blue, 8} }     //    ,24      // TF_I_R8_G8_B8
     ,{{ ChannelDataType::UnsignedInt, ChannelSemantic::Red,16 },        {ChannelDataType::UnsignedInt, ChannelSemantic::Green,16} ,   {ChannelDataType::UnsignedInt, ChannelSemantic::Blue, 16}}//    ,48      // TF_I_R16_G16_B16
     ,{{ ChannelDataType::UnsignedInt, ChannelSemantic::Red,8 },         {ChannelDataType::UnsignedInt, ChannelSemantic::Green,8},     {ChannelDataType::UnsignedInt, ChannelSemantic::Blue, 8}, {ChannelDataType::UnsignedInt, ChannelSemantic::Opacity, 8}}//    ,32      // TF_I_R8_G8_B8_A8
     ,{{ ChannelDataType::UnsignedInt, ChannelSemantic::Red,16 },        {ChannelDataType::UnsignedInt, ChannelSemantic::Green,16},    {ChannelDataType::UnsignedInt, ChannelSemantic::Blue, 16}, {ChannelDataType::UnsignedInt, ChannelSemantic::Opacity, 16}}//    ,64      // TF_I_R16_G16_B16_A16
     ,{{ ChannelDataType::UnsignedInt, ChannelSemantic::Blue,8 },        {ChannelDataType::UnsignedInt, ChannelSemantic::Green,8},     {ChannelDataType::UnsignedInt, ChannelSemantic::Red, 8}}//    ,24      // TF_I_B8_G8_R8
     ,{{ ChannelDataType::UnsignedInt, ChannelSemantic::Blue,5 },        {ChannelDataType::UnsignedInt, ChannelSemantic::Green,5},     {ChannelDataType::UnsignedInt, ChannelSemantic::Red, 5},{ChannelDataType::UnsignedInt, ChannelSemantic::None, 1} }//    ,16      // TF_I_B5_G5_R5_X1
     ,{{ ChannelDataType::UnsignedInt, ChannelSemantic::Blue,5 },        {ChannelDataType::UnsignedInt, ChannelSemantic::Green,6},     {ChannelDataType::UnsignedInt, ChannelSemantic::Red, 5}}//    ,16      // TF_I_B5_G6_R5
     ,{{ ChannelDataType::UnsignedInt, ChannelSemantic::Blue,16 },       {ChannelDataType::UnsignedInt, ChannelSemantic::Green,16},    {ChannelDataType::UnsignedInt, ChannelSemantic::Red, 16}}//    ,48      // TF_I_B16_G16_R16
     ,{{ ChannelDataType::UnsignedInt, ChannelSemantic::Blue,8 },        {ChannelDataType::UnsignedInt, ChannelSemantic::Green,8},     {ChannelDataType::UnsignedInt, ChannelSemantic::Red, 8},  {ChannelDataType::UnsignedInt, ChannelSemantic::Opacity, 8}}//    ,32      // TF_I_B8_G8_R8_A8
     ,{{ ChannelDataType::UnsignedInt, ChannelSemantic::Blue,16 },       {ChannelDataType::UnsignedInt, ChannelSemantic::Green,16},    {ChannelDataType::UnsignedInt, ChannelSemantic::Red, 16},  {ChannelDataType::UnsignedInt, ChannelSemantic::Opacity, 16}}//    ,64      // TF_I_B16_G16_R16_A16
     ,{{ ChannelDataType::UnsignedInt, ChannelSemantic::Opacity,8 },     {ChannelDataType::UnsignedInt, ChannelSemantic::Red,8},       {ChannelDataType::UnsignedInt, ChannelSemantic::Green, 8}, {ChannelDataType::UnsignedInt, ChannelSemantic::Blue, 8}}//    ,32      // TF_I_A8_R8_G8_B8
     ,{{ ChannelDataType::UnsignedInt, ChannelSemantic::Opacity,16 },    {ChannelDataType::UnsignedInt, ChannelSemantic::Blue,16},      {ChannelDataType::UnsignedInt, ChannelSemantic::Green, 16}, {ChannelDataType::UnsignedInt, ChannelSemantic::Blue, 16}}//    ,64      // TF_I_A16_R16_G16_B16
     ,{{ ChannelDataType::UnsignedInt, ChannelSemantic::Opacity,8 },     {ChannelDataType::UnsignedInt, ChannelSemantic::Blue,8},      {ChannelDataType::UnsignedInt, ChannelSemantic::Green, 8},{ChannelDataType::UnsignedInt, ChannelSemantic::Red, 8}}//    ,32      // TF_I_A8_B8_G8_R8
     ,{{ ChannelDataType::UnsignedInt, ChannelSemantic::Opacity,16 },    {ChannelDataType::UnsignedInt, ChannelSemantic::Blue,16},     {ChannelDataType::UnsignedInt, ChannelSemantic::Green, 16}, {ChannelDataType::UnsignedInt, ChannelSemantic::Red, 16}}//    ,64      // TF_I_A16_B16_G16_R16
     ,{{ ChannelDataType::UnsignedInt, ChannelSemantic::Monochrome,8 }}//    ,8       // TF_I_A8
     ,{{ ChannelDataType::UnsignedInt, ChannelSemantic::Monochrome,1 }}//    ,1       // TF_I_X1
     ,{{ ChannelDataType::UnsignedInt, ChannelSemantic::Monochrome,4 }}//    ,4       // TF_I_X4
     ,{{ ChannelDataType::UnsignedInt, ChannelSemantic::Monochrome,8 }}//    ,8       // TF_I_X8
     ,{{ ChannelDataType::UnsignedInt, ChannelSemantic::Monochrome,16 }}//    ,16      // TF_I_X16
     ,{{ ChannelDataType::SignedInt  , ChannelSemantic::Monochrome,8 }}//    ,8       // TF_S_X8
     ,{{ ChannelDataType::SignedInt  , ChannelSemantic::Monochrome,16 }}//    ,16      // TF_S_X16
     ,{{ ChannelDataType::Float      , ChannelSemantic::Float,16 }}//    ,16      // TF_F_X16
     ,{{ ChannelDataType::Float      , ChannelSemantic::Float ,24 }}//    ,24      // TF_F_X24
     ,{{ ChannelDataType::Float      , ChannelSemantic::Float ,32 }}//    ,32      // TF_F_X32
     ,{{ ChannelDataType::Float      , ChannelSemantic::Float ,64 }}//    ,64      // TF_F_X64
    };

    LLUTILS_FORCE_INLINE const TexelInfo& GetTexelInfo(TexelFormat format)
    {
        static_assert(sizeof(TexelFormatInfo) / sizeof(TexelFormatInfo[0]) == static_cast<std::underlying_type<TexelFormat>::type>(TexelFormat::COUNT), " Wrong array size");

        if (format >= TexelFormat::BEGIN && format < TexelFormat::COUNT)
            return TexelFormatInfo[static_cast<std::underlying_type<TexelFormat>::type>(format)];
        else
            LL_EXCEPTION(LLUtils::Exception::ErrorCode::LogicError, " index out of bounds.");
    }

    LLUTILS_FORCE_INLINE ChannelWidth GetTexelFormatSize(TexelFormat format)
    {
        return GetTexelInfo(format).texelSize;
    }
    
}
