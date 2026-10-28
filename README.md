# ParaLLEish

ParaLLEish is a libretro N64 core based on Mupen64Plus and paraLLEl-RDP, with
GlideN64-compatible high-resolution texture replacement, replacement-aware
scaling, deterministic fixture tooling, and renderer-focused conformance tests.

The renderer is the correctness authority. External orchestration may supply
paths and launch parameters, but this repository remains buildable and testable
without a fleet controller or private workspace.

The shipped core uses Mupen64Plus with Vulkan paraLLEl-RDP. Angrylion, Rice,
Glide64/Glitch64 and gln64 renderers are not included. CXD4, HLE and optional
paraLLEl-RSP remain available; Vulkan graphics uses an LLE RSP.

## Current Capabilities

- GlideN64/Rice-compatible draw-time texture identity.
- Offline `.hts`/`.htc` to `.phrb` conversion; runtime loading is `.phrb` only.
- Replacement-aware texrect, sampling, mip, alpha, and view-key behavior.
- Deterministic RetroArch control through the maintained `agent-control`
  frontend branch; [`tools/retroarch-patches/`](tools/retroarch-patches/)
  reconstructs the older stdin-control series.
- Paper Mario fixture authorities plus opt-in SM64 and OoT compatibility lanes;
  MK64 and MM are manual breadth checks.
- Class-level runtime evidence and explicit fallback reporting.

Paper Mario is the strict renderer-validation title. The other games exercise
compatibility breadth and do not define renderer policy.

## Build And Test

No package manager is used.

```sh
./run-build.sh
./run-tests.sh --profile emu-required
```

The required profile is display-free. The runtime profile launches RetroArch,
occupies a display or headless Vulkan device, and requires locally staged
copyrighted inputs:

```sh
./run-tests.sh --profile emu-runtime-conformance
```

Run runtime tests serially. See [Emulator Testing](docs/EMU_TESTING.md) for
profiles, prerequisites, and skip policy.

## Existing Saves And States

The core now reports `ParaLLEish` and builds as `paralleish_libretro.<ext>`.
With RetroArch closed, copy existing files from `ParaLLEl N64/` to
`ParaLLEish/` under your savefile and savestate directories. Copy per-core
configuration/override folders in the same way, and update any explicit
core paths from `parallel_n64_libretro.<ext>`. Keep backups of the old
folders. Savestate bytes and core-option keys (`parallel-n64-*`) are unchanged;
texture packs still use `system/parallel-n64/hires/`.

If you build from an existing checkout, run `make clean` once after updating:
the removed renderers' headers are still listed in old dependency files.

## Runtime Inputs

ROMs, texture packs, savestates, binaries, and generated evidence are not
tracked. Scenario runners accept repository-relative inputs and existing
environment overrides; the primary pack override is
`PARALLEL_RDP_HIRES_CACHE_PATH`. Without that override, packs load only from
`<system>/parallel-n64/hires/<ROM name>.phrb` and/or the matching
`<ROM name>/` directory. Use the ROM header's internal name (trailing spaces
and NULs removed). See [pack setup](assets/TEXTURE_PACKS.md#format-contract).
Do not add machine names or personal
checkout roots as product defaults.

RetroArch is discovered on `PATH` unless explicitly selected. Deterministic
scenarios require `RETROARCH_BASE_CONFIG`; they do not use a personal desktop
configuration. See [scenario configuration](tools/scenarios/README.md#portable-inputs).

Pack source and conversion provenance lives in
[Texture Pack Tracking](assets/TEXTURE_PACKS.md). Generated output belongs under
`artifacts/`.

## Repository Map

- [ParaLLEl video implementation](mupen64plus-video-paraLLEl/)
- [Libretro integration](libretro/libretro.c)
- [Renderer behavior tests](tests/emulator_behavior/)
- [Fixture manifests](tools/fixtures/)
- [Scenario runners](tools/scenarios/)
- [RetroArch adapters](tools/adapters/)
- [Architecture decisions](docs/adr/README.md)
- [Open technical issues](docs/ISSUE_LOG.md)

The [documentation index](docs/README.md) is the shortest route to deeper
material. Agent-specific commands and invariants are in [AGENTS.md](AGENTS.md).

## Project Boundaries

- This repository owns renderer/core behavior, fixture contracts, portable
  adapters, and renderer evidence.
- RetroArch owns generic frontend control, capture, state, and replay transport.
- Eval scoring, private fleet scheduling, and gameplay/TAS research belong to
  their respective external systems.
- Nothing here may require a private orchestration repository to build or run.

## Upstream

This repository descends from
[libretro/parallel-n64](https://github.com/libretro/parallel-n64) through the
[Parallel Launcher edition](https://gitlab.com/parallel-launcher/parallel-n64).
That edition credits Matt Pharoah, Wiseguy, Aglab2, and devwizard64 for its
fork-specific work. Additional component authors and licenses remain recorded
beside their source.

## License

The combined ParaLLEish libretro core is distributed under **GPLv2** because
it includes the Mupen64Plus core. See [LICENSE](LICENSE).

The hi-res renderer (paraLLEl-RDP and our replacement additions) and the pack
converter, including the PHRB format, are **MIT** and can be used separately
from the GPL core. Third-party dependencies retain their own licenses. See
[LICENSES.md](LICENSES.md), the [MIT file inventory](docs/MIT_COMPONENTS.md),
and the [converter license and file list](tools/README.md#license).
