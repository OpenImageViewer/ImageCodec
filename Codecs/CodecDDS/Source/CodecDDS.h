#pragma once

#include <IImagePlugin.h>
#include <Image.h>
#include <ImageItem.h>
#include <DirectXTex.h>
#include <cstring>
#include <cstdio>
#include <limits>
#include <new>
#include <vector>

namespace IMCodec
{
    // The viewer consumes the first DDS surface and its mip chain. libdds retains
    // the complete array/cubemap/volume metadata for clients needing other surfaces.
    class CodecDDS : public IImagePlugin
    {
      public:

        CodecDDS()
            : mPluginProperties({{0x252347b1, 0x91e7, 0x4c5c, {0x8f, 0x6, 0x72, 0x6b, 0xa, 0x52, 0xde, 0x6d}},
                                 CodecCapabilities::Decode,
                                 LLUTILS_TEXT("DirectXTex DDS image codec"),
                                 {{LLUTILS_TEXT("DDS - DirectDraw Surface Image"), {LLUTILS_TEXT("dds")}}}})
        {
        }

        const PluginProperties& GetPluginProperties() override { return mPluginProperties; }

        ImageResult Decode(const std::byte* buffer, std::size_t size, ImageLoadFlags loadFlags,
                           [[maybe_unused]] const Parameters& params, ImageSharedPtr& out_image) override
        {
            return DecodeInput(buffer, size, loadFlags, out_image);
        }

      private:

        ImageResult DecodeInput(const std::byte* buffer, size_t size,
                                ImageLoadFlags loadFlags,
                                ImageSharedPtr& out_image)
        {
            if (!buffer || !size)
                return ImageResult::BadParameters;

            ImageResult result = ImageResult::Success;
            try
            {
                const bool firstOnly = (loadFlags & ImageLoadFlags::LoadOnlyFirstImage) != ImageLoadFlags::None;
                const auto flags     = firstOnly ? DirectX::DDS_FLAGS_IGNORE_MIPS : DirectX::DDS_FLAGS_NONE;
                DirectX::TexMetadata metadata{};
                std::vector<DirectX::Image> views;
                DirectX::ScratchImage source;
                // Borrow validated source views during decoding. Raw-pointer callers
                // receive independent final pixels before these views are released.
                HRESULT hr = DirectX::GetDDSImageViewsFromMemory(reinterpret_cast<const uint8_t*>(buffer), size, flags,
                                                                 metadata, views);
                const bool borrowed = hr == S_OK;
                if (hr == S_FALSE)
                    hr = DirectX::LoadFromDDSMemory(buffer, size, flags, &metadata, source);
                result = MapResult(hr);
                if (result == ImageResult::Success)
                {
                    // IGNORE_MIPS only trims simple DDS files; enforce the caller request for arrays too.
                    const auto mipLevels = firstOnly ? 1 : metadata.mipLevels;
                    if (mipLevels - 1 > std::numeric_limits<uint16_t>::max())
                        return ImageResult::FormatNotSupported;
                    ImageSharedPtr decoded;
                    for (std::size_t mip = 0; mip < mipLevels && result == ImageResult::Success; ++mip)
                    {
                        const auto index    = metadata.ComputeIndex(mip, 0, 0);
                        const auto* surface = borrowed ? (index < views.size() ? &views[index] : nullptr)
                                                       : source.GetImage(mip, 0, 0);
                        if (!surface)
                            return ImageResult::FormatNotSupported;

                        auto item = std::make_shared<ImageItem>();
                        result    = LoadSurface(*surface, *item);
                        if (result == ImageResult::Success)
                        {
                            item->itemType = mip == 0 ? ImageItemType::Image : ImageItemType::Mipmap;
                            auto image     = std::make_shared<Image>(std::move(item), ImageItemType::Mipmap);
                            if (mip == 0)
                            {
                                decoded = std::move(image);
                                decoded->SetNumSubImages(static_cast<uint16_t>(mipLevels - 1));
                            }
                            else
                                decoded->SetSubImage(static_cast<uint16_t>(mip - 1), std::move(image));
                        }
                    }
                    // Keep a previous caller-owned image intact if any mip fails.
                    if (result == ImageResult::Success)
                    {
                        if (DirectX::IsVideo(metadata.format))
                            std::fputs("Warning: CodecDDS temporarily converts YUV to 8-bit RGBA using limited-range "
                                       "BT.601; colors may be inaccurate and higher-bit-depth precision is lost.\n",
                                       stderr);
                        out_image = std::move(decoded);
                    }
                }
            }
            catch (const std::bad_alloc&)
            {
                result = ImageResult::UnknownError;
            }
            return result;
        }

      private:

        static ImageResult MapResult(HRESULT result)
        {
            if (SUCCEEDED(result))
                return ImageResult::Success;
            switch (result)
            {
                case E_NOTIMPL:
                    return ImageResult::NotImplemented;
                case static_cast<HRESULT>(0x80070032u):  // ERROR_NOT_SUPPORTED
                    return ImageResult::FormatNotSupported;
                case E_OUTOFMEMORY:
                    return ImageResult::UnknownError;
                default:
                    return ImageResult::FileIsCorrupted;
            }
        }

        static TexelFormat NativeFormat(DXGI_FORMAT format)
        {
            switch (format)
            {
                case DXGI_FORMAT_R8G8B8A8_UNORM:
                case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
                    return TexelFormat::I_R8_G8_B8_A8;
                case DXGI_FORMAT_B8G8R8A8_UNORM:
                case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
                    return TexelFormat::I_B8_G8_R8_A8;
                case DXGI_FORMAT_R16G16B16A16_UNORM:
                    return TexelFormat::I_R16_G16_B16_A16;
                case DXGI_FORMAT_B5G6R5_UNORM:
                    return TexelFormat::I_B5_G6_R5;
                case DXGI_FORMAT_R32G32B32_FLOAT:
                    return TexelFormat::F_R32_G32_B32;
                case DXGI_FORMAT_R8_UNORM:
                    return TexelFormat::I_X8;
                case DXGI_FORMAT_R16_UNORM:
                    return TexelFormat::I_X16;
                case DXGI_FORMAT_R8_SNORM:
                    return TexelFormat::S_X8;
                case DXGI_FORMAT_R16_SNORM:
                    return TexelFormat::S_X16;
                case DXGI_FORMAT_R16_FLOAT:
                    return TexelFormat::F_X16;
                case DXGI_FORMAT_R32_FLOAT:
                    return TexelFormat::F_X32;
                case DXGI_FORMAT_A8_UNORM:
                    return TexelFormat::I_A8;
                default:
                    return TexelFormat::UNKNOWN;
            }
        }

        ImageResult LoadSurface(const DirectX::Image& surface, ImageItem& item)
        {
            // TODO: Remove this temporary CPU fallback once ImageCodec and the renderer support native YUV
            // with explicit colorimetry. DirectXTex assumes limited-range BT.601; RGBA8 also loses source precision.
            // Only its convertible video formats are accepted; opaque and palettized layouts remain unsupported.
            switch (surface.format)
            {
                case DXGI_FORMAT_AYUV:
                case DXGI_FORMAT_Y410:
                case DXGI_FORMAT_Y416:
                case DXGI_FORMAT_YUY2:
                case DXGI_FORMAT_Y210:
                case DXGI_FORMAT_Y216:
                case DXGI_FORMAT_NV12:
                case DXGI_FORMAT_P010:
                case DXGI_FORMAT_P016:
                case DXGI_FORMAT_NV11:
                {
                    DirectX::ScratchImage packed;
                    const auto* input = &surface;
                    if (DirectX::IsPlanar(surface.format))
                    {
                        const auto result = MapResult(DirectX::ConvertToSinglePlane(surface, packed));
                        if (result != ImageResult::Success)
                            return result;
                        input = packed.GetImage(0, 0, 0);
                    }
                    DirectX::ScratchImage rgba;
                    const auto result = MapResult(DirectX::Convert(*input, DXGI_FORMAT_R8G8B8A8_UNORM,
                                                                   DirectX::TEX_FILTER_FORCE_NON_WIC,
                                                                   DirectX::TEX_THRESHOLD_DEFAULT, rgba));
                    // Copy into the normal output storage so the image outlives these conversion temporaries.
                    return result == ImageResult::Success ? LoadSurface(*rgba.GetImage(0, 0, 0), item)
                                                          : result;
                }
                default:
                    break;
            }

            // Choose the final viewer format before allocating. BC6H and signed
            // channels remain floating point, so HDR and negative values survive.
            DXGI_FORMAT format    = surface.format;
            const bool compressed = DirectX::IsCompressed(format);
            if (compressed)
            {
                switch (format)
                {
                    case DXGI_FORMAT_BC4_SNORM:
                    case DXGI_FORMAT_BC5_SNORM:
                    case DXGI_FORMAT_BC6H_TYPELESS:
                    case DXGI_FORMAT_BC6H_UF16:
                    case DXGI_FORMAT_BC6H_SF16:
                        format = DXGI_FORMAT_R32G32B32_FLOAT;
                        break;
                    default:
                        format = DirectX::IsSRGB(format) ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM;
                        break;
                }
            }

            // Normalize legacy packed RGB pixels into the viewer's RGBA layout.
            const bool convert = format == DXGI_FORMAT_B8G8R8X8_UNORM || format == DXGI_FORMAT_B8G8R8X8_UNORM_SRGB ||
                                 format == DXGI_FORMAT_B5G5R5A1_UNORM || format == DXGI_FORMAT_B4G4R4A4_UNORM;
            if (convert)
                format = DirectX::IsSRGB(format) ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM;

            const auto texelFormat = NativeFormat(format);
            if (texelFormat == TexelFormat::UNKNOWN)
                return ImageResult::FormatNotSupported;

            std::size_t rowPitch   = surface.rowPitch;
            std::size_t slicePitch = surface.slicePitch;
            if (compressed || convert)
            {
                const auto result = MapResult(
                    DirectX::ComputePitch(format, surface.width, surface.height, rowPitch, slicePitch));
                if (result != ImageResult::Success)
                    return result;
            }
            // ImageCodec's image size and pitch accessors are 32-bit.
            constexpr auto maximum = std::numeric_limits<uint32_t>::max();
            if (surface.width > maximum || surface.height > maximum || rowPitch > maximum || slicePitch > maximum)
                return ImageResult::FormatNotSupported;

            ImageResult result = ImageResult::Success;
            {
                item.data.Allocate(slicePitch);
                if (!item.data.data())
                    throw std::bad_alloc();
                const DirectX::Image destination{surface.width, surface.height,
                                                 format,        rowPitch,
                                                 slicePitch,    reinterpret_cast<uint8_t*>(item.data.data())};
                if (compressed)
                    result = MapResult(DirectX::Decompress(surface, destination));
                else if (convert)
                    result = MapResult(DirectX::Convert(surface, destination, DirectX::TEX_FILTER_DEFAULT,
                                                        DirectX::TEX_THRESHOLD_DEFAULT));
                else
                    std::memcpy(destination.pixels, surface.pixels, slicePitch);
            }

            item.descriptor.width                   = static_cast<uint32_t>(surface.width);
            item.descriptor.height                  = static_cast<uint32_t>(surface.height);
            item.descriptor.rowPitchInBytes         = static_cast<uint32_t>(rowPitch);
            item.descriptor.texelFormatDecompressed = texelFormat;
            return result;
        }

        PluginProperties mPluginProperties;
    };
}  // namespace IMCodec
