# Component Licenses And Provenance

The combined ParaLLEish libretro core is distributed under GPLv2 because it
contains the Mupen64Plus core. The full GPLv2 text is in [LICENSE](LICENSE).

The hi-res renderer (paraLLEl-RDP plus our replacement additions) and the pack
converter, including the PHRB format, are MIT and may be used separately from
the GPL core. Their third-party dependencies retain their own licenses; see
the [MIT file inventory](docs/MIT_COMPONENTS.md) and
[converter scope](tools/README.md#license).

| Component | Existing declaration |
|-----------|----------------------|
| Mupen64Plus core | [`mupen64plus-core/LICENSES`](mupen64plus-core/LICENSES): GNU GPL version 2, with separately identified bundled components |
| Mupen64Plus HLE RSP | [`mupen64plus-rsp-hle/LICENSES`](mupen64plus-rsp-hle/LICENSES): GNU GPL version 2 |
| CXD4 RSP | [`mupen64plus-rsp-cxd4/COPYING`](mupen64plus-rsp-cxd4/COPYING): CC0 1.0 Universal |
| ParaLLEl RSP | [`mupen64plus-rsp-paraLLEl/LICENSE`](mupen64plus-rsp-paraLLEl/LICENSE): dual MIT or LGPLv3 |
| GNU Lightning bundled with ParaLLEl RSP | [`lightning/COPYING`](mupen64plus-rsp-paraLLEl/lightning/COPYING) and [`lightning/COPYING.LESSER`](mupen64plus-rsp-paraLLEl/lightning/COPYING.LESSER): GPLv3 or LGPLv3 as described by the component |
| Vendored paraLLEl-RDP project-owned code | [`mupen64plus-video-paraLLEl/parallel-rdp/LICENSE`](mupen64plus-video-paraLLEl/parallel-rdp/LICENSE): MIT; imported headers and third-party code retain their own notices, including Apache-2.0 SPIRV-Cross/Vulkan material and MIT-licensed volk |
| Hi-res pack converter and PHRB format | [`tools/LICENSE-hires-converter`](tools/LICENSE-hires-converter): MIT; covered files are listed in [`tools/README.md`](tools/README.md#license) |
| Granite runtime/util imports in paraLLEl-RDP | MIT; copyright and permission notices retained in [`util/`](mupen64plus-video-paraLLEl/parallel-rdp/util/) and [`vulkan/`](mupen64plus-video-paraLLEl/parallel-rdp/vulkan/) |
| xxHash 0.8.4 (renderer and core) | [`xxhash.LICENSE`](mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp/xxhash.LICENSE) and [`xxhash.h`](mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp/xxhash.h): BSD-2-Clause, Yann Collet |
| SPIRV-Cross | [`spirv-cross/LICENSE`](mupen64plus-video-paraLLEl/parallel-rdp/spirv-cross/LICENSE): Apache-2.0; generated Khronos enum headers retain their own permissive grants |
| Vulkan-Headers | [`vulkan-headers/LICENSE.txt`](mupen64plus-video-paraLLEl/parallel-rdp/vulkan-headers/LICENSE.txt): Apache-2.0 in these pinned headers, with per-file notices retained |
| volk | MIT, Arseny Kapoulkine; full notice in [`volk.h`](mupen64plus-video-paraLLEl/parallel-rdp/volk/volk.h) |
| libretro-common and libretro API | Primarily MIT, with per-file declarations in [`libretro-common/`](libretro-common/); the [`libretro.h`](libretro-common/include/libretro.h) API header is MIT |
| libco | Public domain, as declared in [`libco.c`](libretro-common/libco/libco.c) and its implementation files |
| zlib | zlib license; notices in [`custom/dependencies/libzlib/zlib.h`](custom/dependencies/libzlib/zlib.h) and [`libretro-common/include/compat/zlib.h`](libretro-common/include/compat/zlib.h); renderer compression requires zlib |
| libpng (retained source) | libpng license and contributing-author notices in [`custom/dependencies/libpng/png.h`](custom/dependencies/libpng/png.h); not used by the PHRB-only renderer or Python converter |
| Mupen64Plus bundled MD5 and CIC helpers | zlib-style and BSD-style terms respectively; see [`md5.c`](mupen64plus-core/src/main/md5.c) and [`n64_cic_nus_6105.c`](mupen64plus-core/src/si/n64_cic_nus_6105.c) |
| Python and Pillow (external converter dependencies) | Python/PSF terms and Pillow's MIT-CMU license respectively; not vendored. Redistributed interpreters, Pillow builds and their codecs retain their own notices |
| NASM build tool | BSD-2-Clause; [`LICENSE.nasm`](libretro/msvc/msvc-2013/LICENSE.nasm), with provenance below |

The fork lineage is
[libretro/parallel-n64](https://github.com/libretro/parallel-n64) followed by
the [Parallel Launcher edition](https://gitlab.com/parallel-launcher/parallel-n64)
and this renderer-focused fork.

This table is a navigation aid, not a complete software-bill-of-materials.
Files may carry narrower or different headers than their containing directory.
Resolve per-file provenance before making a tree-wide licensing claim or
redistributing a binary.

The [September 29 component provenance record](docs/COMPONENT_PROVENANCE.md)
identifies imports, removed notices, byte matches and unresolved exceptions.
Notice restoration preserves existing upstream declarations. The release split
does not replace those declarations or settle binary license compatibility for
every optional build, including Apache-2.0 material and GNU Lightning's LGPLv3
or GPLv3 terms.

## Retained build payloads

`libretro/msvc/msvc-2013/nasm.exe` is NASM 2.12.01 for Windows x64. Its
SHA-256 is `699fd438f0bacf284d47eadc87a2370e86159089283ed8f86b03041457dea61f`,
identical to `nasm-2.12.01/nasm.exe` in the
[official release archive](https://www.nasm.us/pub/nasm/releasebuilds/2.12.01/win64/nasm-2.12.01-win64.zip).
The archive SHA-256 is
`1c8f4d5aeb48f68f89f5f7dc7673862f78868787e89cf7688ec7ac0e1941dd42`.
Its upstream BSD-2-Clause notice is retained in
[`LICENSE.nasm`](libretro/msvc/msvc-2013/LICENSE.nasm), with only line endings
and trailing whitespace normalized. Provenance was checked September 28, 2026.

`mupen64plus-core/tools/m64p_helper_scripts.tar.gz` contains six shell helpers
for fetching, building, installing, testing, updating and uninstalling the
upstream core. Each declares Copyright 2009 Richard Goedeken and
GPL-2.0-or-later in its header. It is source tooling, not a game-asset archive;
the existing component license is at
[`mupen64plus-core/LICENSES`](mupen64plus-core/LICENSES).

Generated shader source is tracked in `slangmosh.hpp`; its size alone does not
make it a game asset. ROMs, texture packs, save states and runtime captures are
operator-supplied inputs and are not part of the source-only build.
