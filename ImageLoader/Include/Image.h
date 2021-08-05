#pragma once
#include <memory> // for shared_ptr
#include "imageproperties.h"

namespace IMCodec
{
    class Image final
    {

    public:
        Image(const ImageDescriptor& properties)
            : fDescriptor(properties)

        {

        }
        
        //Image(const Image& rhs) = delete;
        Image operator= (const Image& rhs) = delete;



        //Internal methods
        const ImageDescriptor& GetDescriptor() const { return fDescriptor; }

        // Query methods
        const std::byte* GetBufferAt(int32_t x, int32_t y) const { return fDescriptor.fData.data() + (y * GetRowPitchInBytes() + x * GetBitsPerTexel() / CHAR_BIT); }
        const std::byte* GetBuffer() const { return fDescriptor.fData.data(); }
        uint32_t GetNumSubImages() const {return fDescriptor.fProperties.NumSubImages;}
        uint32_t GetWidth() const { return fDescriptor.fProperties.Width; }
        uint32_t GetHeight() const { return fDescriptor.fProperties.Height; }
        uint32_t GetRowPitchInBytes() const { return fDescriptor.fProperties.RowPitchInBytes; }
        uint32_t GetBitsPerTexel() const { return GetTexelFormatSize(fDescriptor.fProperties.TexelFormatDecompressed); }
        uint32_t GetBytesPerRowOfPixels() const { return GetWidth() * GetBitsPerTexel() / CHAR_BIT ; }
        uint32_t GetRowPitchInTexels() const { return GetRowPitchInBytes() * CHAR_BIT / GetBitsPerTexel() ; }
        uint32_t GetSlicePitchInBytes() const { return GetRowPitchInBytes() * GetHeight(); }
        uint32_t GetSlicePitchInTexels() const { return GetRowPitchInTexels() * GetHeight(); }
        uint32_t GetTotalPixels() const { return GetWidth() * GetHeight(); }
        uint32_t GetTotalSizeOfImageTexels() const { return GetTotalPixels() * GetBitsPerTexel() / CHAR_BIT; }
        uint32_t GetBytesPerTexel() const { return GetBitsPerTexel() / CHAR_BIT; }
        uint32_t GetSizeInMemory() const { return GetRowPitchInBytes() * GetHeight(); }

        bool GetIsRowPitchNormalized() const { return GetRowPitchInBytes() == GetBytesPerRowOfPixels(); }
        bool GetIsByteAligned() const { return GetBitsPerTexel() % CHAR_BIT == 0; }
        
        TexelFormat GetImageType() const { return fDescriptor.fProperties.TexelFormatDecompressed; }
        TexelFormat GetOriginalTexelFormat() const { return fDescriptor.fProperties.TexelFormatStorage; }

    private:
        const ImageDescriptor fDescriptor;
    };

    using ImageSharedPtr = std::shared_ptr<Image>;
    using VecImageSharedPtr = std::vector< ImageSharedPtr>;
 
}
