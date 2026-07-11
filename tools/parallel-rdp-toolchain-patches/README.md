# Pinned paraLLEl-RDP Shader Toolchain

The vendored renderer requires a 2020-era `slangmosh` interface to
regenerate
`mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp/shaders/slangmosh.hpp`.
Modern `slangmosh` emits an incompatible interface.

Use a standalone paraLLEl-RDP checkout at commit `7a3e561e` with
Granite at `fc326088`. Set `PARALLEL_RDP_ROOT` to that
checkout. The local patch supplies the modern-GCC include fix and an
astc-encoder stub for a submodule pin that is no longer available.

```sh
PATCH="$PWD/tools/parallel-rdp-toolchain-patches/0001-granite-2020-modern-gcc.patch"
git -C "$PARALLEL_RDP_ROOT/Granite" apply \
  "$PATCH"
SLANGMOSH="$PARALLEL_RDP_ROOT/Granite/tools/slangmosh/slangmosh" \
  tools/regen-parallel-rdp-shaders.sh --force
```

Check the patch first with `git apply --check` and use
`--reverse --check` to detect an already-patched checkout. Shader
changes require the required gate plus the relevant runtime validation.
