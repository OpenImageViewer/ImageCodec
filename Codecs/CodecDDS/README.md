# DDS decoding

CodecDDS uses libdds CPU decoders and exposes the first DDS surface and its mip
chain. YUV surfaces use a temporary limited-range BT.601 conversion to RGBA8,
which loses higher-bit-depth precision and may not match the source colorimetry.

Legacy BGRX, B5G5R5A1 and B4G4R4A4 conversion writes directly into final storage using
libdds's caller-owned Convert overload. The CPU conversion formulas, filter flags,
threshold and color semantics are unchanged. Legacy 24-bit DDS layout expansion
still uses the owning DDS loader before final image storage.
