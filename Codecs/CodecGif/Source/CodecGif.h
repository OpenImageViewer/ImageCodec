#pragma once

#include <IImagePlugin.h>
#include <Image.h>
#include <gif_lib.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <memory>
#include <vector>

namespace IMCodec
{
    struct GifReadContext
    {
        const std::byte* buffer;
        size_t bufferSize;
        size_t pos;
    };

    inline int ReadGifBuffer(GifFileType* gif, GifByteType* bytes, int length)
    {
        auto* context = static_cast<GifReadContext*>(gif->UserData);
        if (length <= 0 || context->pos >= context->bufferSize)
            return 0;

        const auto count = std::min(static_cast<size_t>(length), context->bufferSize - context->pos);
        std::memcpy(bytes, context->buffer + context->pos, count);
        context->pos += count;
        return static_cast<int>(count);
    }

    class CodecGif : public IImagePlugin
    {
      private:

        using Pixel = std::array<std::byte, 4>;

        PluginProperties mPluginProperties = {
            // {1D076943-F907-426A-8DC6-838B8CCE320C}
            {0x1d076943, 0xf907, 0x426a, {0x8d, 0xc6, 0x83, 0x8b, 0x8c, 0xce, 0x32, 0xc}},
            CodecCapabilities::Decode,
            LLUTILS_TEXT("Gif Codec"),
            {{LLUTILS_TEXT("Graphics Interchange Format"), {LLUTILS_TEXT("gif")}}}};

        static GraphicsControlBlock GetFrameControl(const SavedImage& image)
        {
            GraphicsControlBlock control{DISPOSAL_UNSPECIFIED, false, 0, NO_TRANSPARENT_COLOR};
            for (int i = 0; i < image.ExtensionBlockCount; ++i)
            {
                const auto& block = image.ExtensionBlocks[i];
                if (block.Function == GRAPHICS_EXT_FUNC_CODE)
                    DGifExtensionToGCB(block.ByteCount, block.Bytes, &control);
            }
            return control;
        }

        static Pixel GetBackground(const GifFileType& gif)
        {
            Pixel background{};
            if (gif.SColorMap && gif.SBackGroundColor >= 0 && gif.SBackGroundColor < gif.SColorMap->ColorCount)
            {
                const auto& color = gif.SColorMap->Colors[gif.SBackGroundColor];
                background = {std::byte{color.Red}, std::byte{color.Green}, std::byte{color.Blue}, std::byte{255}};
            }
            return background;
        }

      public:

        const PluginProperties& GetPluginProperties() override { return mPluginProperties; }

        ImageResult Decode(const std::byte* buffer, std::size_t size, ImageLoadFlags loadFlags,
                           [[maybe_unused]] const Parameters& params, ImageSharedPtr& out_image) override
        {
            if (!buffer || size == 0)
                return ImageResult::BadParameters;

            GifReadContext context{buffer, size, 0};
            int error           = 0;
            const auto closeGif = [](GifFileType* file) { DGifCloseFile(file, nullptr); };
            std::unique_ptr<GifFileType, decltype(closeGif)> gif(DGifOpen(&context, &ReadGifBuffer, &error), closeGif);
            if (!gif || DGifSlurp(gif.get()) != GIF_OK || gif->ImageCount <= 0 || gif->SWidth <= 0 || gif->SHeight <= 0)
                return ImageResult::FileIsCorrupted;

            constexpr size_t pixelSize = sizeof(Pixel);
            const auto width           = static_cast<size_t>(gif->SWidth);
            const auto height          = static_cast<size_t>(gif->SHeight);
            if (width > std::numeric_limits<size_t>::max() / pixelSize / height)
                return ImageResult::FileIsCorrupted;

            const bool firstOnly = (loadFlags & ImageLoadFlags::LoadOnlyFirstImage) != ImageLoadFlags::None;
            const int frameCount = firstOnly ? 1 : gif->ImageCount;
            // Image subimage indices and counts are represented by uint16_t.
            if (frameCount > std::numeric_limits<uint16_t>::max())
                return ImageResult::FormatNotSupported;

            const auto background   = GetBackground(*gif);
            const auto firstControl = GetFrameControl(gif->SavedImages[0]);
            // A transparent GIF needs an alpha canvas rather than an opaque logical background.
            std::vector<Pixel> canvas(width * height,
                                      firstControl.TransparentColor == NO_TRANSPARENT_COLOR ? background : Pixel{});
            std::vector<Pixel> previous;
            ImageSharedPtr decoded;
            if (frameCount > 1)
            {
                auto container      = std::make_shared<ImageItem>();
                container->itemType = ImageItemType::Container;
                decoded             = std::make_shared<Image>(container, ImageItemType::AnimationFrame);
                decoded->SetNumSubImages(static_cast<uint16_t>(frameCount));
            }

            for (int frameIndex = 0; frameIndex < frameCount; ++frameIndex)
            {
                const auto& frame    = gif->SavedImages[frameIndex];
                const auto& desc     = frame.ImageDesc;
                const auto control   = GetFrameControl(frame);
                const auto* colorMap = desc.ColorMap ? desc.ColorMap : gif->SColorMap;
                if (!colorMap || !colorMap->Colors || colorMap->ColorCount <= 0 || colorMap->ColorCount > 256 ||
                    !frame.RasterBits || desc.Width <= 0 || desc.Height <= 0 || desc.Left < 0 || desc.Top < 0)
                    return ImageResult::FileIsCorrupted;

                // Expand each palette once, then composite indexed pixels directly onto the canvas.
                std::array<Pixel, 256> palette{};
                for (int i = 0; i < colorMap->ColorCount; ++i)
                {
                    const auto& color = colorMap->Colors[i];
                    palette[i] = {std::byte{color.Red}, std::byte{color.Green}, std::byte{color.Blue}, std::byte{255}};
                }

                const bool hasNext = frameIndex + 1 < frameCount;
                if (hasNext && control.DisposalMode == DISPOSE_PREVIOUS)
                    previous = canvas;

                const auto left          = static_cast<size_t>(desc.Left);
                const auto top           = static_cast<size_t>(desc.Top);
                const auto frameWidth    = static_cast<size_t>(desc.Width);
                const auto frameHeight   = static_cast<size_t>(desc.Height);
                const auto visibleWidth  = left < width ? std::min(frameWidth, width - left) : 0;
                const auto visibleHeight = top < height ? std::min(frameHeight, height - top) : 0;
                // DGifSlurp already deinterlaces the raster. Clip frames extending beyond the logical screen.
                for (size_t y = 0; y < frameHeight; ++y)
                {
                    const auto* row = frame.RasterBits + y * frameWidth;
                    for (size_t x = 0; x < frameWidth; ++x)
                    {
                        const int index = row[x];
                        if (index == control.TransparentColor)
                            continue;
                        if (index >= colorMap->ColorCount)
                            return ImageResult::FileIsCorrupted;
                        if (y < visibleHeight && x < visibleWidth)
                            canvas[(top + y) * width + left + x] = palette[index];
                    }
                }

                auto item               = std::make_shared<ImageItem>();
                item->itemType          = frameCount > 1 ? ImageItemType::AnimationFrame : ImageItemType::Image;
                item->descriptor.width  = gif->SWidth;
                item->descriptor.height = gif->SHeight;
                item->descriptor.texelFormatDecompressed = TexelFormat::I_R8_G8_B8_A8;
                item->descriptor.texelFormatStorage      = TexelFormat::I_R8_G8_B8_A8;
                item->descriptor.rowPitchInBytes         = static_cast<uint32_t>(width * pixelSize);
                item->animationData.delayMilliseconds    = control.DelayTime * 10;
                item->data = LLUtils::Buffer(reinterpret_cast<const std::byte*>(canvas.data()),
                                             canvas.size() * pixelSize);
                auto image = std::make_shared<Image>(item, ImageItemType::Unknown);
                if (frameCount > 1)
                    decoded->SetSubImage(static_cast<uint16_t>(frameIndex), std::move(image));
                else
                    decoded = std::move(image);

                // Disposal affects the next frame, never the displayed snapshot of the current one.
                if (hasNext && control.DisposalMode == DISPOSE_BACKGROUND && visibleWidth > 0)
                {
                    const auto clearColor = control.TransparentColor == NO_TRANSPARENT_COLOR ? background : Pixel{};
                    for (size_t y = 0; y < visibleHeight; ++y)
                        std::fill_n(canvas.begin() + (top + y) * width + left, visibleWidth, clearColor);
                }
                else if (hasNext && control.DisposalMode == DISPOSE_PREVIOUS)
                    canvas.swap(previous);
            }

            // Publish only complete results so failed decodes cannot expose partial animations.
            out_image = std::move(decoded);
            return ImageResult::Success;
        }
    };
}  // namespace IMCodec
