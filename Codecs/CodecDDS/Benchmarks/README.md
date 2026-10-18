# DDS storage pool probe

Build this directory with CMake and run `dds_pool_benchmark switch|alternate [iterations]`.
The probe uses a 16 MiB output pool with full-mip 1536x1536/1792x1792 native images.
It verifies decoded pixels while exercising changed and alternating allocation sizes.
