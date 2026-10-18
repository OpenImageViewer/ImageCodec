#include <CodecDDS.h>
#include <array>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
using namespace DirectX;
int main(int argc, char** argv)
{
    try
    {
#ifdef _WIN32
        if (const auto* cpu = std::getenv("DDS_BENCH_CPU"))
            if (!SetThreadAffinityMask(GetCurrentThread(), DWORD_PTR(1) << std::stoul(cpu)))
                return 2;
#endif
        const bool alternating = argc > 1 && std::string(argv[1]) == "alternate";
        const size_t count     = argc > 2 ? std::stoull(argv[2]) : 256;
        std::array<Blob, 2> inputs;
        for (size_t i = 0; i < 2; ++i)
        {
            ScratchImage source;
            const size_t width = i ? 1792 : 1536;
            if (FAILED(source.Initialize2D(DXGI_FORMAT_B8G8R8A8_UNORM, width, width, 1, 0)))
                return 3;
            std::memset(source.GetPixels(), i ? 41 : 17, source.GetPixelsSize());
            if (FAILED(SaveToDDSMemory(source.GetImages(), source.GetImageCount(), source.GetMetadata(),
                                       DDS_FLAGS_FORCE_DX10_EXT, inputs[i])))
                return 4;
        }
        IMCodec::CodecDDS codec(16 * 1024 * 1024);
        auto decode = [&](size_t index, bool verify)
        {
            IMCodec::ImageSharedPtr image;
            auto& input = inputs[index];
            if (codec.Decode(reinterpret_cast<const std::byte*>(input.GetBufferPointer()), input.GetBufferSize(),
                             IMCodec::ImageLoadFlags::None, {}, image) != IMCodec::ImageResult::Success)
                throw std::runtime_error("decode");
            if (verify)
            {
                for (uint16_t mip = 0; mip <= image->GetNumSubImages(); ++mip)
                {
                    auto level = mip ? image->GetSubImage(mip - 1) : image;
                    if (!std::all_of(level->GetBuffer(), level->GetBuffer() + level->GetSizeInMemory(),
                                     [index](std::byte b) { return b == std::byte(index ? 41 : 17); }))
                        throw std::runtime_error("pixels");
                }
            }
        };
        for (size_t i = 0; i < 16; ++i)
            decode(0, i == 0);
        auto start = std::chrono::steady_clock::now();
        for (size_t i = 0; i < count; ++i)
            decode(alternating ? (i + 1) % 2 : 1, false);
        double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count() / count;
        decode(0, true);
        decode(1, true);
        std::cout << "{\"mode\":\"" << (alternating ? "alternate" : "switch") << "\",\"ms\":" << ms << "}\n";
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what();
        return 1;
    }
}
