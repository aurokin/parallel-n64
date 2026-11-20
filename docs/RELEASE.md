# Release Checklist

ParaLLEish binaries are built with `RELEASE=1` (for example
`make platform=win64 RELEASE=1`).

`HAVE_PARALLEL_RSP=1` adds paraLLEl-RSP, which bundles GNU Lightning (LGPLv3 or
later). The Mupen64Plus sources in this tree are licensed "GPLv2 or any later
version", so such a binary is a GPLv3 combination: publish it under GPLv3, and
add Lightning's `COPYING` and `COPYING.LESSER` and the paraLLEl-RSP license to
its `licenses/` directory. A binary built without it stays GPLv2.

Every published core archive contains, next to the core binary:

- `LICENSE` (GPLv2, the combined core; GPLv3 text as well when paraLLEl-RSP is
  included) and `LICENSES.md`;
- a `licenses/` directory with the license text of every component compiled
  into that binary: paraLLEl-RDP (MIT), the Vulkan headers (Apache-2.0,
  with their notices), volk (MIT), xxHash (BSD-2-Clause), CXD4 RSP
  (CC0), the Mupen64Plus HLE RSP (GPLv2), libretro-common, and zlib when it is
  linked statically;
- a pointer to the exact source revision (GPLv2 section 3).

The hi-res renderer and the pack converter are also published separately under
MIT ([MIT components](MIT_COMPONENTS.md), [converter](../tools/README.md#license)).
