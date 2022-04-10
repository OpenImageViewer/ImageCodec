#pragma once
#include "ImageDescriptor.h"
#include "ImageCommon.h"
#include "ImageItem.h"
#include <LLUtils/Point.h>

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
        static constexpr size_t NumBytesInOneByte = CHAR_BIT;
        ImageDesc(ImageDescriptor* descriptor) : fImageDescriptor(descriptor) {}
        uint32_t GetWidth() const { return fImageDescriptor->width; }
        uint32_t GetHeight() const { return fImageDescriptor->height; }
        uint32_t GetRowPitchInBytes() const { return fImageDescriptor->rowPitchInBytes; }
        ChannelWidth GetBitsPerTexel() const { return GetTexelFormatSize(fImageDescriptor->texelFormatDecompressed); }
        uint32_t GetBytesPerRowOfPixels() const { return GetWidth() * GetBitsPerTexel() / NumBytesInOneByte; }
        uint32_t GetRowPitchInTexels() const { return GetRowPitchInBytes() * NumBytesInOneByte / GetBitsPerTexel(); }
        uint32_t GetSlicePitchInBytes() const { return GetRowPitchInBytes() * GetHeight(); }
        uint32_t GetSlicePitchInTexels() const { return GetRowPitchInTexels() * GetHeight(); }
        uint32_t GetTotalPixels() const { return GetWidth() * GetHeight(); }
        uint32_t GetTotalSizeOfImageTexels() const { return GetTotalPixels() * GetBitsPerTexel() / NumBytesInOneByte; }
        uint32_t GetBytesPerTexel() const { return GetBitsPerTexel() / NumBytesInOneByte; }
        uint32_t GetSizeInMemory() const { return GetRowPitchInBytes() * GetHeight(); }
        LLUtils::Point<uint32_t> GetDimensions() const { return {GetWidth(), GetHeight() }; }

        bool GetIsRowPitchNormalized() const { return GetRowPitchInBytes() == GetBytesPerRowOfPixels(); }
        bool GetIsByteAligned() const { return GetBitsPerTexel() % NumBytesInOneByte == 0; }
        TexelFormat GetTexelFormat() const { return fImageDescriptor->texelFormatDecompressed; }
        TexelFormat GetOriginalTexelFormat() const { return fImageDescriptor->texelFormatStorage; }
        const TexelInfo& GetTexelInfo() const{ return ::IMCodec::GetTexelInfo(GetTexelFormat());}


    private:
        ImageDescriptor* fImageDescriptor;
    };


    /// <summary>
    /// A tree structure of Images.
    /// </summary>
	class Image : public ImageDesc
	{
	public:
        Image(ImageItemSharedPtr imageItem, ImageItemType subImageType) : ImageDesc(&imageItem->descriptor), fSubItemsGroupType(subImageType), fImageItem(imageItem)  {}
        const std::byte* GetBufferAt(int32_t x, int32_t y) const { return fImageItem->data.data()+ (y * GetRowPitchInBytes() + x * GetBitsPerTexel() / CHAR_BIT); }
        const std::byte* GetBuffer() const { return fImageItem->data.data(); }
        uint32_t GetNumSubImages() const { return fSubImages.size(); }

        const ImageDescriptor& GetDescriptor() { return fImageItem->descriptor; }
        const ItemRuntimeData& GetRuntimeData() const { return fImageItem->runtimeData; }
        const AnimationData& GetAnimationData() const { return fImageItem->animationData; }
        const ItemMetaData& GetMetaData() const { return fImageItem->metaData; }
        const ImageItemSharedPtr& GetImageItem() const { return fImageItem; }
        ImageItemType GetItemType() const { return fImageItem->itemType; }
        
        bool IsValidSelf() const
        {
            return
                (fSubItemsGroupType != ImageItemType::Unknown || (fImageItem->data != nullptr && fImageItem->descriptor.IsValid())) // is a type of container 
                ||
                (fSubItemsGroupType == ImageItemType::Image
                    && fImageItem->descriptor.IsValid()
                    && fImageItem->data != nullptr);
        }

        bool IsValid() const
        {
            if (IsValidSelf() == false)
                return false;
            
            for (const auto& subImage : fSubImages)
            {
                if (subImage->IsValid() == false)
                    return false;
            }
            
            return true;
        }

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

        void SetNumSubImages(uint16_t size)
        {
            fSubImages.resize(size);
        }

        ImageItemType GetSubImageGroupType() const { return fSubItemsGroupType;}

    private:
        /// <summary>
        /// the type of subitems in case all subitems shares a common type.
        /// </summary>
        ImageItemType fSubItemsGroupType{};
        std::vector<ImageSharedPtr> fSubImages;
        ImageItemSharedPtr fImageItem;
	};

	class ImageClient
	{

	};


	class ImageGPU
	{

	};

	
}