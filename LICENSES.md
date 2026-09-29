# Component Licenses And Provenance

This repository combines several inherited emulator components. This file maps
the license declarations already present in the tree; it does not select or
assert a new license for the repository as a whole.

| Component | Existing declaration |
|-----------|----------------------|
| Mupen64Plus core | [`mupen64plus-core/LICENSES`](mupen64plus-core/LICENSES): GNU GPL version 2, with separately identified bundled components |
| Mupen64Plus HLE RSP | [`mupen64plus-rsp-hle/LICENSES`](mupen64plus-rsp-hle/LICENSES): GNU GPL version 2 |
| gles2n64 imported tree | [`gles2n64/src/COPYING`](gles2n64/src/COPYING) records GNU LGPL version 3 for the port, while individual files may differ; for example, `glN64Config.c` declares GPL-2-or-later |
| CXD4 RSP | [`mupen64plus-rsp-cxd4/COPYING`](mupen64plus-rsp-cxd4/COPYING): CC0 1.0 Universal |
| ParaLLEl RSP | [`mupen64plus-rsp-paraLLEl/LICENSE`](mupen64plus-rsp-paraLLEl/LICENSE): dual MIT or LGPLv3 |
| GNU Lightning bundled with ParaLLEl RSP | [`lightning/COPYING`](mupen64plus-rsp-paraLLEl/lightning/COPYING) and [`lightning/COPYING.LESSER`](mupen64plus-rsp-paraLLEl/lightning/COPYING.LESSER): GPLv3 or LGPLv3 as described by the component |
| Vendored paraLLEl-RDP project-owned code | [`mupen64plus-video-paraLLEl/parallel-rdp/LICENSE`](mupen64plus-video-paraLLEl/parallel-rdp/LICENSE): MIT; imported headers and third-party code retain their own notices, including Apache-2.0 SPIRV-Cross/Vulkan material and MIT-licensed volk |
| Rice inherited code | Headed source such as [`Video.cpp`](gles2rice/src/Video.cpp) declares GPL-2-or-later; metadata, unheaded imports and contribution rights have separately recorded limits |
| Glide64/Glitch64 inherited renderer code | Headed sources declare GPL-2-or-later; the imported Glide API header has separate [3dfx terms](glide2gl/LICENSE.glide), not a blanket GNU GPL declaration |
| Angrylion imported rasterizer/Plus ancestry | Recovered [MAME terms](<mupen64plus-video-angrylion/MAME License.txt>) and [credits](mupen64plus-video-angrylion/CREDITS.txt), including noncommercial, complete-source and notice conditions; aggregate/build compatibility remains unresolved |

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
Notice restoration preserves existing upstream declarations; it does not
relicense the fork or clear all source/binary distribution requirements.

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
