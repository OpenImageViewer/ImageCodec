# DDS storage benchmark

Build this standalone CMake target in Release with symbols. It exercises CodecDDS
with generated deterministic DDS files, complete mip chains through 1x1, and checks
a hash of every output byte against the comparison build. No assets are downloaded.

```powershell
cmake -S External/ImageCodec/Codecs/CodecDDS/Benchmarks -B build/dds-bench -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang-cl -DCMAKE_CXX_FLAGS_RELEASE="/O2 /DNDEBUG /Zi" -DCMAKE_EXE_LINKER_FLAGS=/DEBUG
cmake --build build/dds-bench
build/dds-bench/dds_storage_benchmark.exe bgra 8192 4 raw consume
```

Arguments are format, base width/height, iteration count, storage mode,
`decode` or `consume`, and an optional fixture output path. Formats are `bgra`,
`rgba`, `bgrx`, `5551`, `4444`, `bc1`, `bc3`, `bc4`, `bc5`.

- `direct`: caller-owned conversion with the output pool disabled.
- `raw`: current pointer API with direct conversion and pooled output.
- `owned`: rejected because input retention is disabled.
- `file`: file open/map, decode into independent output, and close. Supply a
  fixture path. This mirrors ImageLoader's public file-loading path.
  Plugin lookup and its stopwatch are excluded. Files are created before timing,
  so these are cached-file measurements, not cold-disk tests.
- `live`: raw decoding while retaining two previous results, to exercise buffers
  that cannot yet return to the pool.

`decode` includes image destruction/recycling. `consume` additionally copies every
mip into a preallocated upload buffer and hashes that buffer after timing. It models
reading all pixels, not GPU upload or presentation. `first_ms` records the first
call before pool warmup. `ms` is the warmed per-chain mean within that process.
A 250 ms operation warmup precedes the steady interval to settle startup effects.
Input generation, hashing and upload allocation are outside timing. Memory
input acquisition is outside timing. File mapping and close are inside timing.

The default matrix is 64x64 (7 mips), 2048x2048 (12 mips), and 8192x8192 (14 mips).
RGBA8 output sizes are 21.3 KiB, 21.3 MiB, and 341.3 MiB. Each operation loads the
entire chain, and verifies dimensions and mip count before timing.

For a pre-change build use `DDS_BASELINE=ON`, `LIBDDS_SOURCE_DIR`,
`CODECDDS_SOURCE_DIR`, and `IMAGECODEC_INCLUDE_DIR` to point at frozen pre-change
sources. Build the same benchmark source against both implementations.

```powershell
python External/ImageCodec/Codecs/CodecDDS/Benchmarks/run.py <baseline.exe> <candidate.exe> <results-folder>
# Include input I/O and separately retain live outputs:
python External/ImageCodec/Codecs/CodecDDS/Benchmarks/run.py <baseline.exe> <candidate.exe> <file-results> --file --formats bgra bgrx bc5
python External/ImageCodec/Codecs/CodecDDS/Benchmarks/run.py <baseline.exe> <candidate.exe> <live-results> --live --formats bgra bc5
```

The runner alternates order over three trials, extending to seven when any mode's
coefficient of variation exceeds 5%. It saves every sample, medians, min/max and
CV. Treat persistent high CV as unresolved noise. Do not infer small speedups from
those rows. Run without builds, profilers, viewers or other benchmark processes.
Use `--cpu <logical-CPU>` only when explicitly choosing Windows thread affinity.
Otherwise affinity is unchanged. Priority and system power policy are always unchanged.

## Concurrent loads and structured inputs

`--patterns random constant` selects deterministic xorshift32 bytes (seed
`0x12345678`) and all-zero encoded bytes. Input generation stays outside timing.
The random BC blocks exercise varied endpoints/indices. The constant pattern is
an additional check that allocation benefits do not rely on one block pattern.

`--parallel --sizes 1024 --formats bc5 --workers 1 2 8 24 --cpus <CPU indices>`
measures fixed total work of 384 full-chain loads. Supply logical processors from
actual topology, starting with the chosen single-thread core and then distinct
performance cores, followed by efficiency cores when testing the whole machine.
The viewer's ImageResidencyCache defaults to hardware_concurrency() workers, so
machine saturation represents its configured upper bound as well as a stress case.

Workers share immutable encoded input and one codec/pool, and own independent
outputs. Starts and finishes use a barrier. Worker creation, 250 ms warmup and
full output-hash checks are outside timing. Every worker's final mip chain is
checked. The final outputs remain live through the timed finish. The JSON reports
elapsed time and amortized time per image. The runner adds throughput, speedup
against the equivalent one-worker case, and parallel efficiency. Concurrent
consume/live/file combinations are intentionally outside this focused pool test.

`--consume no` selects decode-only runs, `--pair` selects just baseline and the
combined candidate, and `--iterations` overrides the per-case operation count.
CPU affinity applies to benchmark threads, and is verified in parallel workers.
Threads/processes are fresh and exit after each run. The system policy is unchanged.

Use `--independent` with `--parallel` to allocate a separate encoded buffer per
worker before warmup. This preserves the same seed/content while avoiding shared
input cache lines. Every worker still checks its complete output. Both shared and
independent storage cases are included in the recorded validation.

`DDS_BENCH_ALLOWED_CPUS` is an optional comma-separated Windows process CPU set for
testing decoders that enlist helpers. Give baseline and candidate the same set.
`DDS_BENCH_CPU` still pins their calling thread within it. The `mapped` mode is
an alias for `file` while input retention is disabled. The default matrix compares
baseline, direct, and pooled decoding. Retained-input measurements are unavailable.
`dds_pool_benchmark switch|alternate [iterations]` exercises a 16 MiB output pool
with full-mip 1536x1536/1792x1792 native images, testing adaptation and oscillating sizes.
These tools are comparison harnesses. They do not change production defaults.
