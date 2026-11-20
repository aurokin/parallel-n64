# Pinned paraLLEl-RDP Shader Toolchain

Regenerating
`mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp/shaders/slangmosh.hpp`
needs the `slangmosh` that produced the vendored paraLLEl-RDP `65f4f4da`
shader bank. Other Granite revisions emit a different interface or different
SPIR-V for the same sources.

Build `slangmosh` from Granite `194abc80`, with every `third_party`
submodule at the commit that revision pins. Its `tools/slangmosh.cpp` and
`compiler/` sources match Granite `9bb56788`, which the standalone
`65f4f4da` export's Vulkan backend tracks; its toolchain submodules are the
ones that emit SPIR-V generator version 10, as in the vendored bank. No local
patch is needed with a current GCC.

```sh
git -C "$GRANITE" checkout 194abc80
git -C "$GRANITE" submodule update --init --recursive
cmake -S "$GRANITE" -B "$GRANITE/build" -DCMAKE_BUILD_TYPE=Release \
  -DGRANITE_PLATFORM=libretro -DGRANITE_VULKAN_FOSSILIZE=OFF \
  -DGRANITE_RENDERDOC_CAPTURE=OFF -DGRANITE_AUDIO=OFF -DGRANITE_FFMPEG=OFF \
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake --build "$GRANITE/build" --target slangmosh
SLANGMOSH="$GRANITE/build/tools/slangmosh" \
GRANITE_DEFAULT_ASSET_DIRECTORY="$GRANITE/assets" \
  tools/regen-parallel-rdp-shaders.sh --force
```

Check the tool before using it: run the same `slangmosh` command on the
pristine `65f4f4da` shader directory; the output must match its
`slangmosh.hpp` byte for byte. Shader changes require the required gate plus
the relevant runtime validation.
