# parallel-rdp 2020 Toolchain Patches

Local patches for the Granite submodule of the upstream parallel-rdp checkout
(`~/code/mupen/parallel-rdp-upstream`, pinned to the fork's vendored commit
`7a3e561e`, Granite `fc326088`) so its 2020 sources build with a modern GCC.
Needed only to build the `slangmosh` shader packer that regenerates
`mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp/shaders/slangmosh.hpp`
(see `tools/regen-parallel-rdp-shaders.sh` for the full recipe).

- `0001-granite-2020-modern-gcc.patch`: missing `<stdexcept>` include in
  `network/tcp_listener.cpp`, plus the `third_party/astc-encoder/Source/CMakeLists.txt`
  stub (the 2020 astc-encoder submodule pin no longer exists upstream; nothing
  links it with `-DGRANITE_ASTC_ENCODER_COMPRESSION=OFF`).

Apply with: `git -C ~/code/mupen/parallel-rdp-upstream/Granite apply <patch>`
(the astc stub hunk requires the submodule dir to exist; create the empty file
first if the submodule was never initialized).

Known vintage note: the committed `slangmosh.hpp` predates this recipe and was
generated against a 2021-era `debug_channel.h` whose debug-SSBO decorations
(set 3 / binding 31) mismatch the fork's vendored runtime limits; regenerating
with this recipe yields set 7 / binding 15, which matches the runtime
(`vulkan/limits.hpp`, `command_buffer.cpp` debug-channel bind point). The delta
affects DEBUG_ENABLE shader variants only.
