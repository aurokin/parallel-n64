# Release Checklist

ParaLLEish binaries are built with `RELEASE=1` (for example
`make platform=win64 RELEASE=1`). The build refuses `RELEASE=1` together with
`HAVE_PARALLEL_RSP=1`: paraLLEl-RSP bundles GNU Lightning (GPLv3/LGPLv3), which
cannot be combined with the GPLv2-only Mupen64Plus core in a distributed
binary. Development builds may still enable it.

Every published core archive contains, next to the core binary:

- `LICENSE` (GPLv2, the combined core) and `LICENSES.md`;
- a `licenses/` directory with the license text of every component compiled
  into that binary: paraLLEl-RDP (MIT), SPIRV-Cross and the Vulkan headers
  (Apache-2.0, with their notices), volk (MIT), xxHash (BSD-2-Clause), CXD4 RSP
  (CC0), the Mupen64Plus HLE RSP (GPLv2), libretro-common, and zlib when it is
  linked statically;
- a pointer to the exact source revision (GPLv2 section 3).

The hi-res renderer and the pack converter are also published separately under
MIT ([MIT components](MIT_COMPONENTS.md), [converter](../tools/README.md#license)).
