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

The fork lineage is
[libretro/parallel-n64](https://github.com/libretro/parallel-n64) followed by
the [Parallel Launcher edition](https://gitlab.com/parallel-launcher/parallel-n64)
and this renderer-focused fork.

This table is a navigation aid, not a complete software-bill-of-materials.
Files may carry narrower or different headers than their containing directory.
Resolve per-file provenance before making a tree-wide licensing claim or
redistributing a binary.

## Retained build payloads

`libretro/msvc/msvc-2013/nasm.exe` is an inherited Windows assembler executable.
Its exact upstream release and redistribution notice have not been established
by the September 28 source audit; do not infer that its license follows the
surrounding emulator code. Resolve its provenance before distributing a new
binary bundle containing it.

`mupen64plus-core/tools/m64p_helper_scripts.tar.gz` contains six shell helpers
for fetching, building, installing, testing, updating and uninstalling the
upstream core. It is source tooling, not a game-asset archive. Its scripts still
require their own provenance review before a complete redistribution claim.

Generated shader source is tracked in `slangmosh.hpp`; its size alone does not
make it a game asset. ROMs, texture packs, save states and runtime captures are
operator-supplied inputs and are not part of the source-only build.
