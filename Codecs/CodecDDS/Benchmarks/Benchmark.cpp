#include <CodecDDS.h>
#include <LLUtils/FileMapping.h>
#include <filesystem>
#include <chrono>
#include <atomic>
#include <barrier>
#include <sstream>
#include <thread>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace DirectX;
namespace
{
    void Check(bool condition)
    {
        if (!condition)
            throw std::runtime_error("Benchmark validation failed");
    }
    uint64_t Hash(const std::byte* bytes, size_t size)
    {
        uint64_t hash = 14695981039346656037ull;
        for (size_t i = 0; i < size; ++i)
            hash = (hash ^ std::to_integer<uint8_t>(bytes[i])) * 1099511628211ull;
        return hash;
    }
}  // namespace
int main(int argc, char** argv)
{
    try
    {
#ifdef _WIN32
        if (const auto* allowed = std::getenv("DDS_BENCH_ALLOWED_CPUS"))
        {
            DWORD_PTR mask = 0;
            std::stringstream names(allowed);
            for (std::string name; std::getline(names, name, ',');)
            {
                const auto index = std::stoul(name);
                Check(index < sizeof(DWORD_PTR) * 8);
                mask |= DWORD_PTR(1) << index;
            }
            Check(SetProcessAffinityMask(GetCurrentProcess(), mask) != 0);
        }
#endif
        // Opt-in only: the benchmark runner never changes the system power policy.
        if (const auto* cpu = std::getenv("DDS_BENCH_CPU"))
        {
#ifdef _WIN32
            const auto index = std::stoul(cpu);
            Check(index < sizeof(DWORD_PTR) * 8);
            Check(SetThreadAffinityMask(GetCurrentThread(), DWORD_PTR(1) << index) != 0);
#else
            throw std::runtime_error("DDS_BENCH_CPU currently supports Windows only");
#endif
        }
        const std::string formatName = argc > 1 ? argv[1] : "bgra";
        const size_t width           = argc > 2 ? std::stoull(argv[2]) : 64;
        const size_t iterations      = argc > 3 ? std::stoull(argv[3]) : 100;
        const std::string mode       = argc > 4 ? argv[4] : "raw";
        const bool consume           = argc > 5 && std::string(argv[5]) == "consume";
        const std::string filePath   = argc > 6 ? argv[6] : "";
        if (mode == "owned")
            throw std::runtime_error("Owned-input decoding is disabled");
        const DXGI_FORMAT format = formatName == "bgra"   ? DXGI_FORMAT_B8G8R8A8_UNORM
                                   : formatName == "rgba" ? DXGI_FORMAT_R8G8B8A8_UNORM
                                   : formatName == "bgrx" ? DXGI_FORMAT_B8G8R8X8_UNORM
                                   : formatName == "5551" ? DXGI_FORMAT_B5G5R5A1_UNORM
                                   : formatName == "4444" ? DXGI_FORMAT_B4G4R4A4_UNORM
                                   : formatName == "bc1"  ? DXGI_FORMAT_BC1_UNORM
                                   : formatName == "bc3"  ? DXGI_FORMAT_BC3_UNORM
                                   : formatName == "bc4"  ? DXGI_FORMAT_BC4_UNORM
                                                          : DXGI_FORMAT_BC5_UNORM;
        ScratchImage source;
        Check(SUCCEEDED(source.Initialize2D(format, width, width, 1, 0)));
        size_t expectedMips = 1;
        for (size_t n = width; n > 1; n >>= 1)
            ++expectedMips;
        Check(source.GetImageCount() == expectedMips);
        const std::string pattern = std::getenv("DDS_BENCH_PATTERN") ? std::getenv("DDS_BENCH_PATTERN") : "random";
        Check(pattern == "random" || pattern == "constant");
        const bool constantPattern = pattern == "constant";
        uint32_t random            = 0x12345678;
        for (size_t i = 0; i < source.GetPixelsSize(); ++i)
        {
            random ^= random << 13;
            random ^= random >> 17;
            random ^= random << 5;
            source.GetPixels()[i] = constantPattern ? 0 : static_cast<uint8_t>(random);
        }
        Blob dds;
        Check(SUCCEEDED(SaveToDDSMemory(source.GetImages(), source.GetImageCount(), source.GetMetadata(),
                                        DDS_FLAGS_FORCE_DX10_EXT, dds)));
        auto input = std::make_shared<LLUtils::Buffer>(reinterpret_cast<const std::byte*>(dds.GetBufferPointer()),
                                                       dds.GetBufferSize());
        const size_t inputBytes = input->size();
        dds.Release();
        source.Release();
        if (!filePath.empty())
        {
            std::ofstream file(filePath, std::ios::binary);
            Check(static_cast<bool>(file.write(reinterpret_cast<const char*>(input->data()), input->size())));
        }
#ifdef DDS_BASELINE
        IMCodec::CodecDDS codec;
#else
        IMCodec::CodecDDS codec(mode == "direct" ? 0 : 512ull * 1024 * 1024);
#endif
        auto decode = [&](const LLUtils::Buffer* overrideInput = nullptr)
        {
            IMCodec::ImageSharedPtr image;
            IMCodec::ImageResult result;
            if (mode == "file" || mode == "mapped")
            {
                const auto path = std::filesystem::path(filePath);
                LLUtils::FileMapping mapping(path.native());
                const auto* bytes = static_cast<const std::byte*>(mapping.GetBuffer());
                result            = codec.Decode(bytes, mapping.GetSize(), IMCodec::ImageLoadFlags::None, {}, image);
            }
            else
                result = codec.Decode(overrideInput ? overrideInput->data() : input->data(),
                                      overrideInput ? overrideInput->size() : input->size(),
                                      IMCodec::ImageLoadFlags::None, {}, image);
            Check(result == IMCodec::ImageResult::Success);
            return image;
        };
        const auto coldBegin = std::chrono::steady_clock::now();
        auto initial         = decode();
        const double firstMs =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - coldBegin).count();
        Check(initial->GetNumSubImages() + 1 == expectedMips);
        size_t outputBytes = 0;
        uint64_t digest    = 0;
        for (size_t mip = 0; mip < expectedMips; ++mip)
        {
            auto image = mip ? initial->GetSubImage(static_cast<uint16_t>(mip - 1)) : initial;
            Check(image->GetWidth() == std::max<size_t>(1, width >> mip));
            outputBytes += image->GetImageItem()->data.size();
            digest ^= Hash(image->GetBuffer(), image->GetImageItem()->data.size()) + mip;
        }
        initial.reset();
        if (mode == "parallel")
        {
            Check(!consume);
            const bool independentInputs = std::getenv("DDS_BENCH_INDEPENDENT") != nullptr;
            std::vector<size_t> cpus;
            std::stringstream text(std::getenv("DDS_BENCH_CPUS") ? std::getenv("DDS_BENCH_CPUS") : "");
            for (std::string value; std::getline(text, value, ',');)
                cpus.push_back(std::stoul(value));
            Check(!cpus.empty() && iterations >= cpus.size() && iterations % cpus.size() == 0);
            std::chrono::steady_clock::time_point parallelBegin, parallelEnd;
            bool finishing = false;
            std::barrier rendezvous(static_cast<std::ptrdiff_t>(cpus.size() + 1),
                                    [&]() noexcept
                                    {
                                        if (finishing)
                                            parallelEnd = std::chrono::steady_clock::now();
                                        else
                                        {
                                            parallelBegin = std::chrono::steady_clock::now();
                                            finishing     = true;
                                        }
                                    });
            std::atomic<bool> valid{true};
            std::vector<std::jthread> workers;
            workers.reserve(cpus.size());
            try
            {
                for (const auto cpu : cpus)
                    workers.emplace_back(
                        [&, cpu]
                        {
                            IMCodec::ImageSharedPtr last;
                            std::unique_ptr<LLUtils::Buffer> privateInput;
                            try
                            {
#ifdef _WIN32
                                Check(cpu < sizeof(DWORD_PTR) * 8);
                                Check(SetThreadAffinityMask(GetCurrentThread(), DWORD_PTR(1) << cpu) != 0);
                                Check(GetCurrentProcessorNumber() == cpu);
#else
                                throw std::runtime_error("Pinned concurrency currently supports Windows only");
#endif
                                if (independentInputs)
                                    privateInput = std::make_unique<LLUtils::Buffer>(input->data(), input->size());
                                const auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(250);
                                do
                                {
                                    last = decode(privateInput.get());
                                    last.reset();
                                } while (std::chrono::steady_clock::now() < until);
                            }
                            catch (...)
                            {
                                valid = false;
                            }
                            rendezvous.arrive_and_wait();
                            try
                            {
                                for (size_t i = 0; i < iterations / cpus.size(); ++i)
                                {
                                    last.reset();
                                    last = decode(privateInput.get());
                                }
                            }
                            catch (...)
                            {
                                valid = false;
                            }
                            rendezvous.arrive_and_wait();
                            try
                            {
                                Check(last != nullptr);
                                uint64_t actual = 0;
                                for (size_t mip = 0; mip < expectedMips; ++mip)
                                {
                                    auto level = mip ? last->GetSubImage(static_cast<uint16_t>(mip - 1)) : last;
                                    actual ^= Hash(level->GetBuffer(), level->GetImageItem()->data.size()) + mip;
                                }
                                Check(actual == digest);
                            }
                            catch (...)
                            {
                                valid = false;
                            }
                        });
            }
            catch (...)
            {
                valid = false;
                for (size_t missing = workers.size(); missing < cpus.size(); ++missing)
                    rendezvous.arrive_and_drop();
            }
            // Setup and warmup finish before the main thread releases the start barrier.
            rendezvous.arrive_and_wait();
            rendezvous.arrive_and_wait();
            const auto elapsed = std::chrono::duration<double, std::milli>(parallelEnd - parallelBegin).count();
            workers.clear();
            Check(valid);
            std::cout << "{\"format\":\"" << formatName << "\",\"width\":" << width << ",\"mips\":" << expectedMips
                      << ",\"independent_inputs\":" << independentInputs << ",\"pattern\":\"" << pattern
                      << "\",\"workers\":" << cpus.size() << ",\"iterations\":" << iterations << ",\"digest\":\""
                      << digest << "\",\"elapsed_ms\":" << elapsed << ",\"ms\":" << elapsed / iterations << "}\n";
            return 0;
        }
        LLUtils::Buffer upload;
        if (consume)
            upload.Allocate(outputBytes);
        if (consume)
            std::memset(upload.data(), 0, upload.size());
        // Keep two prior results alive in this mode, as during browsing/prefetch.
        std::vector<IMCodec::ImageSharedPtr> live(2);
        size_t liveSlot = 0;
        auto operation  = [&]()
        {
            auto image = decode();
            if (consume)
            {
                size_t offset = 0;
                for (size_t mip = 0; mip < expectedMips; ++mip)
                {
                    auto level       = mip ? image->GetSubImage(static_cast<uint16_t>(mip - 1)) : image;
                    const auto bytes = level->GetImageItem()->data.size();
                    std::memcpy(upload.data() + offset, level->GetBuffer(), bytes);
                    offset += bytes;
                }
            }
            if (mode == "live")
            {
                live[liveSlot] = std::move(image);
                liveSlot       = (liveSlot + 1) % live.size();
            }
        };
        const auto warmUntil = std::chrono::steady_clock::now() + std::chrono::milliseconds(250);
        size_t warmups       = 0;
        do
        {
            operation();
            ++warmups;
        } while (std::chrono::steady_clock::now() < warmUntil);
        const auto begin = std::chrono::steady_clock::now();
        for (size_t i = 0; i < iterations; ++i)
            operation();
        const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - begin).count() /
                          iterations;
        std::cout << "{\"format\":\"" << formatName << "\",\"width\":" << width << ",\"mips\":" << expectedMips
                  << ",\"pattern\":\"" << pattern << "\",\"seed\":305419896" << ",\"iterations\":" << iterations
                  << ",\"mode\":\"" << mode << "\",\"consume\":" << consume << ",\"warmup_iterations\":" << warmups
                  << ",\"input_bytes\":" << inputBytes << ",\"output_bytes\":" << outputBytes << ",\"digest\":\""
                  << digest << "\",\"upload_digest\":\"" << (consume ? Hash(upload.data(), upload.size()) : 0)
                  << "\",\"first_ms\":" << firstMs << ",\"ms\":" << ms << "}\n";
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
