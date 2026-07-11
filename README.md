# parallel-n64

This fork extends the ParaLLEl video core in the libretro Parallel N64 emulator
with GlideN64-compatible high-resolution texture replacement, replacement-aware
scaling, deterministic fixture tooling, and renderer-focused conformance tests.

The renderer is the correctness authority. External orchestration may supply
paths and launch parameters, but this repository remains buildable and testable
without a fleet controller or private workspace.

## Current Capabilities

- GlideN64/Rice-compatible draw-time texture identity.
- Offline `.hts`/`.htc` to `.phrb` conversion; runtime loading is `.phrb` only.
- Replacement-aware texrect, sampling, mip, alpha, and view-key behavior.
- Deterministic RetroArch control through
  [`tools/retroarch-patches/`](tools/retroarch-patches/).
- Paper Mario fixture authorities plus opt-in compatibility lanes for SM64,
  OoT, MK64, and MM.
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

## Runtime Inputs

ROMs, texture packs, savestates, binaries, and generated evidence are not
tracked. Scenario runners accept repository-relative inputs and existing
environment overrides; the primary pack override is
`PARALLEL_RDP_HIRES_CACHE_PATH`. Do not add machine names or personal
checkout roots as product defaults.

Some legacy runners still contain overridable local fallbacks. Their portable
consumer seams are tracked separately; new code must not copy those values.

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

## Upstream And Licensing

This repository descends from
[libretro/parallel-n64](https://github.com/libretro/parallel-n64) through the
[Parallel Launcher edition](https://gitlab.com/parallel-launcher/parallel-n64).
That edition credits Matt Pharoah, Wiseguy, Aglab2, and devwizard64 for its
fork-specific work. Additional component authors and licenses remain recorded
beside their source.

The tree contains components under multiple existing licenses. No new
tree-wide license is asserted here; see [LICENSES.md](LICENSES.md) for the
factual component map.
