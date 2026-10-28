#pragma once
#include <cstdint>
#include <tiffio.h>
#include <tiffio.hxx>
#include <string>
#include <algorithm>
#include <cstring>

namespace IMCodec
{

    struct tiff_bufferInfo
    {
        const uint8_t* buffer;
        tsize_t size;
        tsize_t currentPos;
    };

    tsize_t tiff_read(thandle_t handle, tdata_t data, tsize_t size)
    {
        auto& info = *static_cast<tiff_bufferInfo*>(handle);
        if (size <= 0 || info.currentPos < 0 || info.currentPos > info.size)
            return 0;
        const auto readable = std::min(size, info.size - info.currentPos);
        std::memcpy(data, info.buffer + info.currentPos, static_cast<size_t>(readable));
        info.currentPos += readable;
        return readable;
    }

    tsize_t tiff_write([[maybe_unused]] thandle_t handle, [[maybe_unused]] tdata_t data, [[maybe_unused]] tsize_t size)
    {
        return 0;
    }
    toff_t tiff_seek(thandle_t handle, toff_t offset, int whence)
    {
        auto& info      = *static_cast<tiff_bufferInfo*>(handle);
        const auto size = static_cast<toff_t>(info.size);
        toff_t position = offset;
        if (whence == SEEK_CUR || whence == SEEK_END)
        {
            const auto base = whence == SEEK_CUR ? static_cast<toff_t>(info.currentPos) : size;
            if (offset <= size - base)
                position = base + offset;
            else
            {
                // TIFF represents backward relative seeks as unsigned two's-complement offsets.
                const auto backwards = toff_t{0} - offset;
                if (backwards > base)
                    return static_cast<toff_t>(-1);
                position = base - backwards;
            }
        }
        else if (whence != SEEK_SET)
            return static_cast<toff_t>(-1);
        if (position > size)
            return static_cast<toff_t>(-1);
        info.currentPos = static_cast<tsize_t>(position);
        return position;
    }
    int tiff_close(thandle_t)
    {
        return 0;
    }
    toff_t tiff_size(thandle_t handle)
    {
        tiff_bufferInfo* bufferInfo = reinterpret_cast<tiff_bufferInfo*>(handle);
        return bufferInfo->size;
    }
    int tiff_map(thandle_t, tdata_t*, toff_t*)
    {
        return 0;
    }
    void tiff_unmap(thandle_t, tdata_t, toff_t) {}

    TIFF* ReadTiffFromMemory(tiff_bufferInfo& bufferInfo)
    {
        TIFF* tif = TIFFClientOpen("TifChannel", "r", reinterpret_cast<thandle_t>(&bufferInfo), tiff_read, tiff_write,
                                   tiff_seek, tiff_close, tiff_size, tiff_map, tiff_unmap);

        return tif;
    }
}  // namespace IMCodec