# Pinned paraLLEl-RDP Shader Toolchain

Regenerating
`mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp/shaders/slangmosh.hpp`
needs the `slangmosh` that produced the vendored paraLLEl-RDP `1cecd042`
shader bank. Other Granite revisions emit a different interface or different
SPIR-V for the same sources.

Build `slangmosh` from Granite `b8cc8335` with its pinned `third_party`
submodules and the SPIR-V optimizer compiled out
(`GRANITE_SHADER_COMPILER_OPTIMIZE=OFF`). Upstream's 1cecd042 bank has these
properties, and this tool reproduces it:

- the pre-October-2023 header layout (one `slangmosh.hpp`, no separate
  interface header); Granite `bf145d05` and later emit the new layout;
- reflection blobs of `ResourceLayout` version 4 (Granite `46478c77` and
  later); the 2022 tool writes version 3, which the 2024 backend rejects;
- SPIR-V from the glslang/SPIRV-Tools/shaderc pins Granite carried from
  `9bb56788` through `b8cc8335`;
- unoptimized SPIR-V: upstream's command passes `-O`, but the bank is what
  a build without the optimizer emits. A build with the optimizer produces
  different, smaller SPIR-V.

`b8cc8335` is the Granite revision paraLLEl-RDP itself pinned in July 2023
(`87fba42e`), the last pin before the header-layout change. The Granite that
the `1cecd042` export's Vulkan backend tracks (`cf71dee7`) does not
reproduce the bank: it emits the new header layout and has no `--vk11`.

```sh
git -C "$GRANITE" checkout b8cc8335
git -C "$GRANITE" submodule update --init \
  third_party/spirv-cross third_party/shaderc third_party/glslang \
  third_party/spirv-tools third_party/spirv-headers third_party/rapidjson \
  third_party/stb/stb third_party/meshoptimizer third_party/muFFT \
  third_party/khronos/vulkan-headers third_party/volk third_party/fsr2
cmake -S "$GRANITE" -B "$GRANITE/build" -DCMAKE_BUILD_TYPE=Release \
  -DGRANITE_PLATFORM=libretro -DGRANITE_VULKAN_FOSSILIZE=OFF \
  -DGRANITE_RENDERDOC_CAPTURE=OFF -DGRANITE_AUDIO=OFF -DGRANITE_FFMPEG=OFF \
  -DGRANITE_SHADER_COMPILER_OPTIMIZE=OFF -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake --build "$GRANITE/build" --target slangmosh
SLANGMOSH="$GRANITE/build/tools/slangmosh" \
GRANITE_DEFAULT_ASSET_DIRECTORY="$GRANITE/assets" \
  tools/regen-parallel-rdp-shaders.sh --force
```

The script runs, in the shader directory, upstream's command
`slangmosh --strip -O --namespace RDP slangmosh.json --output slangmosh.hpp`
(with the Granite include path rewritten to `$GRANITE/assets`).

Check the tool before using it: run the same `slangmosh` command on the
pristine `1cecd042` shader directory; the output must match its
`slangmosh.hpp` byte for byte. Shader changes require the required gate plus
the relevant runtime validation.
