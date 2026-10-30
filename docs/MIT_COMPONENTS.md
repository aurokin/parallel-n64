# MIT component scope

The renderer's [MIT license](../mupen64plus-video-paraLLEl/parallel-rdp/LICENSE)
covers paraLLEl-RDP and ParaLLEish's additions. The combined libretro core
remains GPLv2; see [LICENSES.md](../LICENSES.md). The GPL video integration
outside `mupen64plus-video-paraLLEl/parallel-rdp/` is outside this MIT scope.

## Renderer headers added by ParaLLEish

Paths below are relative to `mupen64plus-video-paraLLEl/parallel-rdp/`.
Each file was introduced by the listed local commit and is absent both from
the upstream tree and from all fetched upstream path history at paraLLEl-RDP
commit `7a3e561e89d35f7e221770d21e7efa970b496a1e` (the vendored `COMMIT`).
This is the ownership basis for adding `SPDX-License-Identifier: MIT` to
these files. Existing upstream files keep their original headers, including
files that contain local modifications.

| File with a new MIT SPDX header | Local addition commit |
| --- | --- |
| `parallel-rdp/command_ring_policy.hpp` | `5aaa5e19` |
| `parallel-rdp/rdp_device_capability_policy.hpp` | `c09ded58` |
| `parallel-rdp/rdp_hires_capability_policy.hpp` | `444d8264` |
| `parallel-rdp/rdp_hires_ci_binding_policy.hpp` | `9e866127` |
| `parallel-rdp/rdp_hires_ci_palette_policy.hpp` | `3317ec66` |
| `parallel-rdp/rdp_hires_compat_view.hpp` | `9f134ee2` |
| `parallel-rdp/rdp_hires_jabo_key.hpp` | `6820a4b1` |
| `parallel-rdp/rdp_hires_key_state_policy.hpp` | `0a18f945` |
| `parallel-rdp/rdp_hires_load_origin_policy.hpp` | `9e866127` |
| `parallel-rdp/rdp_hires_lookup_policy.hpp` | `e3d15e45` |
| `parallel-rdp/rdp_hires_pass_policy.hpp` | `553e4a27` |
| `parallel-rdp/rdp_hires_registry_policy.hpp` | `2aece304` |
| `parallel-rdp/rdp_hires_runtime_config.hpp` | `c2522872` |
| `parallel-rdp/rdp_hires_runtime_policy.hpp` | `94dd3e86` |
| `parallel-rdp/rdp_hires_state_policy.hpp` | `582872a4` |
| `parallel-rdp/rdp_hires_strong_hash.hpp` | `65341153` |
| `parallel-rdp/rdp_hires_summary_interval.hpp` | `1764dd53` |
| `parallel-rdp/rdp_hires_texture_layout.hpp` | `c2522872` |
| `parallel-rdp/rdp_memory_path_policy.hpp` | `31ff74e2` |
| `parallel-rdp/rdp_other_modes_policy.hpp` | `ab6975a3` |
| `parallel-rdp/rdp_rect_setup_policy.hpp` | `71324b1f` |
| `parallel-rdp/rdp_scanout_coherency_policy.hpp` | `d30b5568` |
| `parallel-rdp/rdp_scissor_policy.hpp` | `2d4ad38a` |
| `parallel-rdp/rdp_tex_rect_policy.hpp` | `d850a45d` |
| `parallel-rdp/rdp_triangle_setup_policy.hpp` | `cc320f8a` |
| `parallel-rdp/shaders/hires_addressing.h` | `c6833754` |
| `parallel-rdp/shaders/hires_cutout_policy.h` | `e0d44cbd` |
| `parallel-rdp/shaders/hires_mip_filter.h` | `051ec699` |
| `parallel-rdp/texture_keying.hpp` | `7b1e276f` |
| `parallel-rdp/texture_replacement.cpp` | `8d5f2d40` |
| `parallel-rdp/texture_replacement.hpp` | `8d5f2d40` |
| `parallel-rdp/vi_scale_policy.hpp` | `fd00563e` |
| `parallel-rdp/vi_scanout_policy.hpp` | `7f0e2b91` |

The generated `parallel-rdp/shaders/slangmosh.hpp` is not an entirely local
source file; it embeds upstream and local shaders. It receives no new SPDX
header. Neither do imported Granite, SPIRV-Cross, Vulkan-Headers, volk or
BSD-2-Clause `parallel-rdp/xxhash.h`. Their original notices remain intact.
The GPL core also includes this BSD-2-Clause xxHash header for TLB code-page
fingerprints; the renderer does not depend on core headers.
Full Apache-2.0 license copies accompany SPIRV-Cross and Vulkan-Headers.

## Dependency boundary

A static scan of the vendored renderer's C/C++ and shader includes found no
includes of GPL repository headers, including Mupen64Plus headers. The optional
`HAVE_LIBRETRO` logging path in `util/logging.hpp` includes the MIT
`libretro-common/include/libretro.h` API header. Standalone consumers may omit
that integration define and provide their own frontend.

The renderer uses zlib for PHRB decompression. It does not use a PNG decoder;
PNG/JPEG/BMP input decoding is performed offline by the converter. The retained
zlib and libpng sources outside the renderer keep their own notices. Optional
Granite/SDK includes and caller-configured volk headers are dependencies of
the configurations that enable them; this source scan does not validate every
external build configuration.

The [converter file list and dependency boundary](../tools/README.md#license)
cover the separately usable Python tooling and PHRB format. This inventory
records licensing scope, not a standalone renderer build or a binary
compatibility certification.
