#include "LinearDng.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <span>
#include <ranges>
#if IMCODEC_TIFF_DNG_JPEG
    #include <turbojpeg.h>
#endif

namespace IMCodec
{
#if IMCODEC_TIFF_DNG_JPEG
    namespace
    {
        constexpr uint16_t LinearRaw = 34892;

        bool HasTag(TIFF* tiff, uint32_t tag)
        {
            const int count = TIFFGetTagListCount(tiff);
            return std::ranges::any_of(std::views::iota(0, count),
                                       [=](int index) { return TIFFGetTagListEntry(tiff, index) == tag; });
        }

        // A table does not establish RGB primaries. This first path accepts only the
        // D50-referenced sRGB matrix and neutral calibration used by rendered RGB DNGs.
        bool HasSrgbCalibration(TIFF* tiff)
        {
            constexpr std::array<float, 9> xyzToSrgbD50{3.1336F, -1.6168F, -0.4907F, -0.9787F, 1.9161F,
                                                        0.0335F, 0.0721F,  -0.2291F, 1.4054F};
            uint16_t count = 0;
            float* matrix  = nullptr;
            if (!TIFFGetField(tiff, TIFFTAG_COLORMATRIX1, &count, &matrix) || count != xyzToSrgbD50.size())
                return false;
            for (size_t i = 0; i < xyzToSrgbD50.size(); ++i)
                if (!std::isfinite(matrix[i]) || std::abs(matrix[i] - xyzToSrgbD50[i]) > 0.00015F)
                    return false;
            float* white = nullptr;
            if (!TIFFGetField(tiff, TIFFTAG_ASSHOTWHITEXY, &white) || !std::isfinite(white[0]) ||
                !std::isfinite(white[1]) || std::abs(white[0] - 0.3457F) > 0.0001F ||
                std::abs(white[1] - 0.3585F) > 0.0001F)
                return false;
            float* balance = nullptr;
            if (TIFFGetField(tiff, TIFFTAG_ANALOGBALANCE, &count, &balance) &&
                (count != 3 ||
                 !std::ranges::all_of(std::span(balance, count), [](float value) { return value == 1.0F; })))
                return false;
            float exposure = 0;
            return !TIFFGetField(tiff, TIFFTAG_BASELINEEXPOSURE, &exposure) || exposure == 0;
        }

        // These operations require raw development beyond a sample lookup. Do not attach
        // an unapplied table to pixels produced by a fallback that has already developed them.
        bool NeedsDevelopment(TIFF* tiff)
        {
            constexpr std::array tags{TIFFTAG_COLORMATRIX2,
                                      TIFFTAG_COLORMATRIX3,
                                      TIFFTAG_CAMERACALIBRATION1,
                                      TIFFTAG_CAMERACALIBRATION2,
                                      TIFFTAG_REDUCTIONMATRIX1,
                                      TIFFTAG_REDUCTIONMATRIX2,
                                      TIFFTAG_ASSHOTNEUTRAL,
                                      TIFFTAG_ASSHOTICCPROFILE,
                                      TIFFTAG_CURRENTICCPROFILE,
                                      TIFFTAG_ICCPROFILE,
                                      TIFFTAG_FORWARDMATRIX1,
                                      TIFFTAG_FORWARDMATRIX2,
                                      TIFFTAG_PROFILEHUESATMAPDATA1,
                                      TIFFTAG_PROFILEHUESATMAPDATA2,
                                      TIFFTAG_PROFILELOOKTABLEDATA,
                                      TIFFTAG_PROFILETONECURVE,
                                      TIFFTAG_OPCODELIST1,
                                      TIFFTAG_OPCODELIST2,
                                      TIFFTAG_OPCODELIST3,
                                      TIFFTAG_BASELINEEXPOSUREOFFSET,
                                      TIFFTAG_BLACKLEVELDELTAH,
                                      TIFFTAG_BLACKLEVELDELTAV,
                                      TIFFTAG_MASKEDAREAS};
            return std::ranges::any_of(tags, [=](uint32_t tag) { return HasTag(tiff, tag); });
        }

        bool HasFullRange(TIFF* tiff, uint32_t width, uint32_t height)
        {
            uint16_t count = 0;
            float* black   = nullptr;
            if (TIFFGetField(tiff, TIFFTAG_BLACKLEVEL, &count, &black) &&
                ((count != 1 && count != 3) ||
                 !std::ranges::all_of(std::span(black, count), [](float value) { return value == 0; })))
                return false;
            uint32_t* white = nullptr;
            if (!TIFFGetField(tiff, TIFFTAG_WHITELEVEL, &count, &white) || (count != 1 && count != 3) ||
                !std::ranges::all_of(std::span(white, count), [](uint32_t value) { return value == 65535; }))
                return false;
            uint32_t* active = nullptr;
            if (TIFFGetField(tiff, TIFFTAG_ACTIVEAREA, &active) &&
                (active[0] != 0 || active[1] != 0 || active[2] != height || active[3] != width))
                return false;
            // Keep original texels only for square-pixel layouts. Aspect correction belongs to raw development.
            float* scale = nullptr;
            if (TIFFGetField(tiff, TIFFTAG_DEFAULTSCALE, &scale) &&
                (!std::isfinite(scale[0]) || !std::isfinite(scale[1]) || scale[0] <= 0 || scale[1] <= 0 ||
                 scale[0] != scale[1]))
                return false;
            float* crop = nullptr;
            if (TIFFGetField(tiff, TIFFTAG_DEFAULTCROPORIGIN, &crop) && (crop[0] != 0 || crop[1] != 0))
                return false;
            if (TIFFGetField(tiff, TIFFTAG_DEFAULTCROPSIZE, &crop) &&
                (crop[0] != static_cast<float>(width) || crop[1] != static_cast<float>(height)))
                return false;
            return true;
        }
    }  // namespace
#endif

    ImageResult DecodeLinearDng(TIFF* tiff, ImageSharedPtr& output)
    {
        output.reset();
#if IMCODEC_TIFF_DNG_JPEG
        const auto rootDirectory = TIFFCurrentDirOffset(tiff);
        if (NeedsDevelopment(tiff) || !HasSrgbCalibration(tiff))
            return ImageResult::FormatNotSupported;
        uint16_t directoryCount    = 0;
        uint64_t* directoryOffsets = nullptr;
        std::vector<uint64_t> directories;
        if (TIFFGetField(tiff, TIFFTAG_SUBIFD, &directoryCount, &directoryOffsets))
            directories.assign(directoryOffsets, directoryOffsets + directoryCount);
        // TIFF directory switches invalidate borrowed tag pointers. Copy offsets before traversal.
        directories.insert(directories.begin(), TIFFCurrentDirOffset(tiff));
        bool found = false;
        for (size_t index = 0; index < directories.size() && !found; ++index)
        {
            if (!TIFFSetSubDirectory(tiff, directories[index]))
                return ImageResult::FileIsCorrupted;
            uint32_t type        = 0;
            uint16_t photometric = 0;
            TIFFGetFieldDefaulted(tiff, TIFFTAG_SUBFILETYPE, &type);
            TIFFGetField(tiff, TIFFTAG_PHOTOMETRIC, &photometric);
            found = type == 0 && photometric == LinearRaw;
        }
        if (!found || NeedsDevelopment(tiff))
            return ImageResult::FormatNotSupported;
        if (TIFFCurrentDirOffset(tiff) != rootDirectory &&
            (HasTag(tiff, TIFFTAG_COLORMATRIX1) || HasTag(tiff, TIFFTAG_ASSHOTWHITEXY) ||
             HasTag(tiff, TIFFTAG_ANALOGBALANCE) || HasTag(tiff, TIFFTAG_BASELINEEXPOSURE)))
            return ImageResult::FormatNotSupported;
        uint32_t width = 0, height = 0, tileWidth = 0, tileHeight = 0;
        uint16_t bits = 0, samples = 0, planar = 0, compression = 0, sampleFormat = 0;
        TIFFGetField(tiff, TIFFTAG_IMAGEWIDTH, &width);
        TIFFGetField(tiff, TIFFTAG_IMAGELENGTH, &height);
        TIFFGetField(tiff, TIFFTAG_TILEWIDTH, &tileWidth);
        TIFFGetField(tiff, TIFFTAG_TILELENGTH, &tileHeight);
        TIFFGetField(tiff, TIFFTAG_BITSPERSAMPLE, &bits);
        TIFFGetField(tiff, TIFFTAG_SAMPLESPERPIXEL, &samples);
        TIFFGetFieldDefaulted(tiff, TIFFTAG_PLANARCONFIG, &planar);
        TIFFGetFieldDefaulted(tiff, TIFFTAG_SAMPLEFORMAT, &sampleFormat);
        TIFFGetField(tiff, TIFFTAG_COMPRESSION, &compression);
        if (bits != 8 || samples != 3 || planar != PLANARCONFIG_CONTIG || sampleFormat != SAMPLEFORMAT_UINT ||
            compression != COMPRESSION_JPEG || !TIFFIsTiled(tiff) || !HasFullRange(tiff, width, height))
            return ImageResult::FormatNotSupported;
        const uint64_t pitch     = static_cast<uint64_t>(width) * 3;
        const uint64_t tilePitch = static_cast<uint64_t>(tileWidth) * 3;
        if (pitch == 0 || height == 0 || tilePitch == 0 || tileHeight == 0 ||
            pitch > std::numeric_limits<uint32_t>::max() || height > std::numeric_limits<uint32_t>::max() / pitch ||
            tilePitch > std::numeric_limits<int>::max() ||
            tileHeight > std::numeric_limits<uint32_t>::max() / tilePitch)
            return ImageResult::FileIsCorrupted;
        const auto size     = pitch * height;
        const auto tileSize = tilePitch * tileHeight;
        uint16_t count      = 0;
        uint16_t* table     = nullptr;
        if (!TIFFGetField(tiff, TIFFTAG_LINEARIZATIONTABLE, &count, &table) || count != 256)
            return ImageResult::FormatNotSupported;
        auto item                        = std::make_shared<ImageItem>();
        item->itemType                   = ImageItemType::Image;
        item->descriptor                 = {.width                   = width,
                                            .height                  = height,
                                            .rowPitchInBytes         = static_cast<uint32_t>(pitch),
                                            .texelFormatDecompressed = TexelFormat::I_R8_G8_B8,
                                            .texelFormatStorage      = TexelFormat::I_R8_G8_B8};
        item->colorInfo.encoding         = ImageItem::ColorInfo::Encoding::SRGB;
        item->colorInfo.transferFunction = ImageItem::ColorInfo::TransferFunction::Sampled;
        item->colorInfo.alphaMode        = ImageItem::ColorInfo::AlphaMode::Opaque;
        const auto curve                 = std::make_shared<const SampledTransferCurve>(SampledTransferCurve{
            .entries     = std::vector<uint16_t>(table, table + count),
            .indexing    = SampledTransferCurve::Indexing::DirectInteger,
            .outputScale = 1.0 / 65535.0,
        });
        item->colorInfo.sampledTransfer  = SampledTransferFunction{.channels = {curve, curve, curve}};
        item->metaData                   = std::make_shared<ItemMetaData>();
        item->metaData->sourceColorSpace = SourceColorSpace::RGB;
        item->data.Allocate(static_cast<size_t>(size));
        std::vector<unsigned char> tile(static_cast<size_t>(tileSize));
        std::vector<unsigned char> compressed;
        for (uint64_t y = 0; y < height; y += tileHeight)
        {
            for (uint64_t x = 0; x < width; x += tileWidth)
            {
                const auto tileIndex = TIFFComputeTile(tiff, static_cast<uint32_t>(x), static_cast<uint32_t>(y), 0, 0);
                // Each supported tile is a complete JPEG stream. A fresh decoder prevents a damaged
                // tile from silently inheriting Huffman tables left by the preceding tile.
                const std::unique_ptr<void, decltype(&tj3Destroy)> jpeg(tj3Init(TJINIT_DECOMPRESS), tj3Destroy);
                if (!jpeg || tj3Set(jpeg.get(), TJPARAM_STOPONWARNING, 1) != 0)
                    return ImageResult::UnknownError;
                int tileError                 = 0;
                const uint64_t compressedSize = TIFFGetStrileByteCountWithErr(tiff, tileIndex, &tileError);
                const uint64_t fileSize       = TIFFGetSizeProc(tiff)(TIFFClientdata(tiff));
                if (tileError || compressedSize == 0 || compressedSize > fileSize ||
                    compressedSize > static_cast<uint64_t>(std::numeric_limits<tmsize_t>::max()))
                    return ImageResult::FileIsCorrupted;
                compressed.resize(static_cast<size_t>(compressedSize));
                if (TIFFReadRawTile(tiff, tileIndex, compressed.data(), static_cast<tmsize_t>(compressedSize)) !=
                        static_cast<tmsize_t>(compressedSize) ||
                    tj3DecompressHeader(jpeg.get(), compressed.data(), compressed.size()) != 0)
                    return ImageResult::FileIsCorrupted;
                if (tj3Get(jpeg.get(), TJPARAM_PRECISION) != 8 || tj3Get(jpeg.get(), TJPARAM_COLORSPACE) != TJCS_RGB ||
                    tj3Get(jpeg.get(), TJPARAM_LOSSLESS) != 1)
                    return ImageResult::FormatNotSupported;
                if (tj3Get(jpeg.get(), TJPARAM_JPEGWIDTH) != static_cast<int>(tileWidth) ||
                    tj3Get(jpeg.get(), TJPARAM_JPEGHEIGHT) != static_cast<int>(tileHeight) ||
                    tj3Decompress8(jpeg.get(), compressed.data(), compressed.size(), tile.data(),
                                   static_cast<int>(tilePitch), TJPF_RGB) != 0)
                    return ImageResult::FileIsCorrupted;
                const size_t copyWidth = static_cast<size_t>(std::min<uint64_t>(tileWidth, width - x)) * 3;
                const uint64_t rows    = std::min<uint64_t>(tileHeight, height - y);
                for (uint64_t row = 0; row < rows; ++row)
                    std::memcpy(item->data.data() + (y + row) * pitch + x * 3, tile.data() + row * tilePitch,
                                copyWidth);
            }
        }
        output = std::make_shared<Image>(std::move(item), ImageItemType::Unknown);
        return ImageResult::Success;
#else
        static_cast<void>(tiff);
        return ImageResult::FormatNotSupported;
#endif
    }
}  // namespace IMCodec
