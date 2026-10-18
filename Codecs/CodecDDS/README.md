# DDS storage ownership

CodecDDS uses libdds CPU decoders and exposes the first DDS surface and its mip
chain. YUV surfaces use a temporary limited-range BT.601 conversion to RGBA8,
which loses higher-bit-depth precision and may not match the source colorimetry.

## Public decoding

`Decode(pointer, size, ...)` returns pixels independent of the input lifetime,
including native DDS surfaces and all returned mips. Callers may change or release
the encoded input after Decode returns. ImageLoader uses a file mapping only during
decoding, so returned pixels survive file replacement, truncation, and deletion.

ImageBuffer owns writable output storage. Copying an ImageBuffer or ImageItem
copies the pixels. ImageSharedPtr copies refer to the same mutable image, and
concurrent mutation requires caller synchronization. Source users passing storage
as an LLUtils::Buffer reference can use MutableBuffer(). Const reads can use
Image::GetBuffer() or a const ImageBuffer reference. LLUtils itself is unchanged.

## Disabled internal retention

No public API or load flag enables input retention. Owned-input dispatch and
file snapshot acquisition have been removed.

The dormant DDS backend can retain validated native mip views through a shared
encoded buffer. Each mip keeps that buffer alive independently, and writable
access first detaches into private storage. Retained storage may have DDS
alignment rather than LLUtils's 16-byte allocation alignment. Re-enabling this
backend requires an input ownership path and revisiting its lifetime,
immutability, and performance contracts.

## Output storage

The decoded-output pool remains enabled. It holds at most 512 MiB of idle payload
storage and 64 allocations. Only allocations of at least 64 KiB enter the pool,
and exact-size buffers are recycled after their final owner releases them.
Allocations larger than the budget are freed rather than cached. Live images are
outside the idle limit. This allowance is independent of the application's image
residency cache. A zero capacity in CodecDDS's constructor disables its output pool.

Legacy BGRX, B5G5R5A1, and B4G4R4A4 conversion writes directly into final storage
using libdds's caller-owned Convert overload. Legacy 24-bit layout expansion uses
the owning DDS loader before final image storage.

See [Benchmarks/README.md](Benchmarks/README.md) for the storage pool probe.

Large BC surfaces (approximately four million pixels or more) can use up to three
async helpers alongside the calling thread. Work is split at complete BC block-row
boundaries, including partial bottom/right blocks. Only one large surface at a time
may enlist helpers. Overlapping large decodes run serially. All tasks finish before
the result is exposed or its storage can return to the pool. Small images and native
images keep their existing paths. This policy bounds extra concurrency rather than
creating a separate persistent worker pool. Original libdds decoder code is unchanged.
