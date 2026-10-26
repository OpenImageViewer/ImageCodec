#pragma once
#include "ImageDescriptor.h"
#include "ImageItem.h"
#include <LLUtils/Point.h>
#include <utility>

namespace IMCodec
{

    class Image;
    using ImageSharedPtr = std::shared_ptr<Image>;
    /// <summary>
    /// ImageDesc is a utility wrapper for the POD ImageDescriptor
    /// </summary>
    class ImageDesc
    {
      public:

        static constexpr size_t NumBitsInOneByte = CHAR_BIT;
        ImageDesc(ImageDescriptor* descriptor) : fImageDescriptor(descriptor) {}
        uint32_t GetWidth() const { return fImageDescriptor->width; }
        uint32_t GetHeight() const { return fImageDescriptor->height; }
        uint32_t GetRowPitchInBytes() const { return fImageDescriptor->rowPitchInBytes; }
        ChannelWidth GetBitsPerTexel() const { return GetTexelFormatSize(fImageDescriptor->texelFormatDecompressed); }
        uint32_t GetBytesPerRowOfPixels() const
        {
            return CheckedSize(static_cast<uint64_t>(GetWidth()) * GetBitsPerTexel() / NumBitsInOneByte);
        }
        uint32_t GetRowPitchInTexels() const
        {
            return CheckedSize(static_cast<uint64_t>(GetRowPitchInBytes()) * NumBitsInOneByte / GetBitsPerTexel());
        }
        uint32_t GetSlicePitchInBytes() const
        {
            return CheckedSize(static_cast<uint64_t>(GetRowPitchInBytes()) * GetHeight());
        }
        uint32_t GetSlicePitchInTexels() const
        {
            return CheckedSize(static_cast<uint64_t>(GetRowPitchInTexels()) * GetHeight());
        }
        uint32_t GetTotalPixels() const { return CheckedSize(static_cast<uint64_t>(GetWidth()) * GetHeight()); }
        uint32_t GetTotalSizeOfImageTexels() const
        {
            return CheckedSize(static_cast<uint64_t>(GetTotalPixels()) * GetBitsPerTexel() / NumBitsInOneByte);
        }
        uint32_t GetBytesPerTexel() const { return GetBitsPerTexel() / NumBitsInOneByte; }
        uint32_t GetSizeInMemory() const { return GetSlicePitchInBytes(); }
        LLUtils::Point<uint32_t> GetDimensions() const { return {GetWidth(), GetHeight()}; }

        bool GetIsRowPitchNormalized() const { return GetRowPitchInBytes() == GetBytesPerRowOfPixels(); }
        bool GetIsByteAligned() const { return GetBitsPerTexel() % NumBitsInOneByte == 0; }
        TexelFormat GetTexelFormat() const { return fImageDescriptor->texelFormatDecompressed; }
        TexelFormat GetOriginalTexelFormat() const { return fImageDescriptor->texelFormatStorage; }
        const TexelInfo& GetTexelInfo() const { return ::IMCodec::GetTexelInfo(GetTexelFormat()); }
        const TexelInfo& GetOriginalTexelInfo() const { return ::IMCodec::GetTexelInfo(GetOriginalTexelFormat()); }

      private:

        // Keep the public 32-bit layout contract, but reject an unrepresentable result instead
        // of wrapping. In particular, multiplying pixels by bits must happen before narrowing.
        static uint32_t CheckedSize(uint64_t value)
        {
            if (value > std::numeric_limits<uint32_t>::max())
                LL_EXCEPTION(LLUtils::Exception::ErrorCode::BadParameters, "Image layout exceeds 32-bit limits");
            return static_cast<uint32_t>(value);
        }

        ImageDescriptor* fImageDescriptor;
    };

    /// <summary>
    /// A tree structure of Images.
    /// </summary>
    class Image : public ImageDesc
    {
      public:

        Image(ImageItemSharedPtr imageItem, ImageItemType subImageType)
            : ImageDesc(&imageItem->descriptor), fSubItemsGroupType(subImageType), fImageItem(imageItem)
        {
            // If storage texel format is not set, assume it's identical to the decompressed texel format.
            if (imageItem->descriptor.texelFormatStorage == TexelFormat::UNKNOWN)
                imageItem->descriptor.texelFormatStorage = imageItem->descriptor.texelFormatDecompressed;
        }
        const std::byte* GetBufferAt(int32_t x, int32_t y) const
        {
            return std::as_const(fImageItem->data).data() +
                   (static_cast<uint64_t>(y) * GetRowPitchInBytes() +
                    static_cast<uint64_t>(x) * GetBitsPerTexel() / ImageDesc::NumBitsInOneByte);
        }
        const std::byte* GetBuffer() const { return std::as_const(fImageItem->data).data(); }
        uint32_t GetNumSubImages() const { return static_cast<uint32_t>(fSubImages.size()); }

        const ImageDescriptor& GetDescriptor() { return fImageItem->descriptor; }
        // const ItemRuntimeData& GetRuntimeData() const { return fImageItem->runtimeData. }
        const AnimationData& GetAnimationData() const { return fImageItem->animationData; }
        const ItemProcessData& GetProcessData() const { return fImageItem->processData; }
        const ImageItemSharedPtr& GetImageItem() const { return fImageItem; }
        ImageItemType GetItemType() const { return fImageItem->itemType; }

        ImageSharedPtr GetSubImage(uint16_t index)
        {
            if (index >= fSubImages.size())
                LL_EXCEPTION(LLUtils::Exception::ErrorCode::BadParameters, "index out of range");

            return fSubImages[index];
        }

        void SetSubImage(uint16_t index, ImageSharedPtr image)
        {
            if (fSubImages.size() <= index)
                fSubImages.resize(index + 1);
            fSubImages[index] = image;
        }

        void SetNumSubImages(uint16_t size) { fSubImages.resize(size); }

        ImageItemType GetSubImageGroupType() const { return fSubItemsGroupType; }

      private:

        /// <summary>
        /// the type of subitems in case all subitems shares a common type.
        /// </summary>
        ImageItemType fSubItemsGroupType{};
        std::vector<ImageSharedPtr> fSubImages;
        ImageItemSharedPtr fImageItem;
    };
}  // namespace IMCodec
