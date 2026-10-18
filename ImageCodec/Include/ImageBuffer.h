#pragma once
#include <LLUtils/Buffer.h>
#include <algorithm>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

namespace IMCodec
{
    // Only idle allocations count against this bound. Live images own their buffers
    // independently, and may outlive the codec that created the pool.
    class ImageBufferPool
    {
      public:

        static constexpr size_t DefaultCapacity    = 512ull * 1024 * 1024;
        static constexpr size_t MinimumPooledBytes = 64 * 1024;
        explicit ImageBufferPool(size_t capacity = DefaultCapacity) : fCapacity(capacity) {}

        static std::shared_ptr<ImageBufferPool> Shared()
        {
            static const auto pool = std::make_shared<ImageBufferPool>();
            return pool;
        }

        LLUtils::Buffer Acquire(size_t size)
        {
            if (!size)
                return {};
            {
                std::lock_guard lock(fMutex);
                const auto found = std::find_if(fIdle.begin(), fIdle.end(),
                                                [size](const auto& buffer) { return buffer.size() == size; });
                if (found != fIdle.end())
                {
                    LLUtils::Buffer result = std::move(*found);
                    fIdle.erase(found);
                    fBytes -= size;
                    return result;
                }
            }
            LLUtils::Buffer result(size);
            if (size && !result.data())
                throw std::bad_alloc();
            return result;
        }

        void Release(LLUtils::Buffer buffer) noexcept
        {
            if (!buffer.size() || buffer.size() > fCapacity)
                return;
            try
            {
                std::lock_guard lock(fMutex);
                // Bound metadata as well as bytes. Exact-size reuse avoids retaining
                // a huge allocation for a small mip, and needs no per-pixel locking.
                if (fIdle.size() < 64 && buffer.size() <= fCapacity - fBytes)
                {
                    const auto size = buffer.size();
                    fIdle.push_back(std::move(buffer));
                    fBytes += size;
                }
            }
            catch (...)
            {
            }  // Recycling must never make image destruction throw.
        }

      private:

        // Native DDS input becomes the output storage. Return it to the same
        // bounded pool only after the last retained mip releases the buffer object.
        static std::shared_ptr<LLUtils::Buffer> AcquireInput(size_t size)
        {
            if (!size)
                return std::make_shared<LLUtils::Buffer>();
            if (size < MinimumPooledBytes)
                return std::make_shared<LLUtils::Buffer>(size);
            auto pool = Shared();
            return {new LLUtils::Buffer(pool->Acquire(size)), [pool](LLUtils::Buffer* buffer)
                    {
                        pool->Release(std::move(*buffer));
                        delete buffer;
                    }};
        }

        const size_t fCapacity;
        size_t fBytes = 0;
        std::mutex fMutex;
        std::vector<LLUtils::Buffer> fIdle;
    };

    // Public decoders produce writable owned pixels. Internal retained storage
    // and detach-on-write support remain available for the disabled DDS path.
    // As with LLUtils::Buffer, callers synchronize concurrent mutation themselves.
    class ImageBuffer
    {
      public:

        ImageBuffer() = default;
        ImageBuffer(const ImageBuffer& other) : fOwned(other.Clone()) {}
        ImageBuffer(ImageBuffer&& other) noexcept { Swap(other); }
        ImageBuffer& operator=(const ImageBuffer& other)
        {
            if (this != &other)
                *this = other.Clone();
            return *this;
        }
        ImageBuffer& operator=(ImageBuffer&& other) noexcept
        {
            if (this != &other)
            {
                Free();
                Swap(other);
            }
            return *this;
        }
        ImageBuffer& operator=(LLUtils::Buffer buffer)
        {
            Free();
            fOwned = std::move(buffer);
            return *this;
        }
        ~ImageBuffer() { Free(); }

        const std::byte* data() const noexcept { return fOwner ? fPixels : fOwned.data(); }
        std::byte* data() { return MutableBuffer().data(); }
        size_t size() const noexcept { return fOwner ? fSize : fOwned.size(); }
        size_t Size() const noexcept { return size(); }
        LLUtils::Buffer Clone() const
        {
            LLUtils::Buffer copy;
            if (size())
            {
                copy.Allocate(size());
                if (!copy.data())
                    throw std::bad_alloc();
                std::memcpy(copy.data(), data(), size());
            }
            return copy;
        }
        void Read(std::byte* target, size_t offset, size_t count) const
        {
            if (offset > size() || count > size() - offset)
                throw std::out_of_range("Image buffer read overflow");
            if (count)
                std::memcpy(target, data() + offset, count);
        }
        void Write(const std::byte* source, size_t offset, size_t count)
        {
            if (offset > size() || count > size() - offset)
                throw std::out_of_range("Image buffer write overflow");
            if (count)
                std::memcpy(data() + offset, source, count);
        }
        void Allocate(size_t size, std::shared_ptr<ImageBufferPool> pool = {})
        {
            auto buffer = size ? (pool ? pool->Acquire(size) : LLUtils::Buffer(size)) : LLUtils::Buffer{};
            if (size && !buffer.data())
                throw std::bad_alloc();
            Free();
            fOwned = std::move(buffer);
            fPool  = std::move(pool);
        }
        void Free() noexcept
        {
            fOwner.reset();
            fPixels = nullptr;
            fSize   = 0;
            if (fPool)
                fPool->Release(std::move(fOwned));
            else
                fOwned.Free();
            fPool.reset();
        }
        LLUtils::Buffer& MutableBuffer()
        {
            if (fOwner)
            {
                // Allocate before releasing the owner, including on allocation failure.
                *this = Clone();
            }
            return fOwned;
        }

      private:

        friend class CodecDDS;

        // The owner must keep this range alive and immutable. The codec validates
        // that every surface lies inside the owner's encoded byte buffer.
        void Retain(std::shared_ptr<const LLUtils::Buffer> owner, const std::byte* pixels, size_t size)
        {
            if (!owner || !pixels)
                throw std::invalid_argument("Missing image buffer owner");
            const auto start   = reinterpret_cast<std::uintptr_t>(owner->data());
            const auto address = reinterpret_cast<std::uintptr_t>(pixels);
            if (address < start || address - start > owner->size() || size > owner->size() - (address - start))
                throw std::out_of_range("Image surface outside retained input");
            Free();
            fOwner  = std::move(owner);
            fPixels = pixels;
            fSize   = size;
        }
        void Swap(ImageBuffer& other) noexcept
        {
            std::swap(fOwned, other.fOwned);
            std::swap(fPool, other.fPool);
            std::swap(fOwner, other.fOwner);
            std::swap(fPixels, other.fPixels);
            std::swap(fSize, other.fSize);
        }
        LLUtils::Buffer fOwned;
        std::shared_ptr<ImageBufferPool> fPool;
        std::shared_ptr<const LLUtils::Buffer> fOwner;
        const std::byte* fPixels = nullptr;
        size_t fSize             = 0;
    };
}  // namespace IMCodec
