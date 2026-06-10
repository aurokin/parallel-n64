# Agent Instructions

## Start Here
- [README.md](/home/auro/code/parallel-n64/README.md)
- [Reboot Plan](/home/auro/code/parallel-n64/docs/REBOOT_PLAN.md) — the controlling plan
- [Workspace Paths](/home/auro/code/parallel-n64/docs/WORKSPACE_PATHS.md)
- [Docs Index](/home/auro/code/parallel-n64/docs/README.md)
- [Project Notebook](/home/auro/code/parallel-n64/PROJECT_NOTES.md)

## Package Manager
- None. Use root shell scripts, `cmake`, `ctest`, and `make` as needed.

## File-Scoped Commands
| Task | Command |
|------|---------|
| Build target | `cmake --build build --target <target>` |
| Run one test | `ctest --test-dir build -R <test_name> --output-on-failure` |
| Required gate | `./run-tests.sh --profile emu-required` |
| Runtime gate | `./run-tests.sh --profile emu-runtime-conformance` |
| Full rebuild of test tree | `./run-tests.sh --clean --profile <profile>` |
| Rebuild live libretro core for ParaLLEl scenarios | `make -j4 -B HAVE_PARALLEL=1 parallel_n64_libretro.so` |

## Active Scope
Work order (details and per-step verification in the [Reboot Plan](/home/auro/code/parallel-n64/docs/REBOOT_PLAN.md)):
1. remint the savestate ladder (title -> file select -> `kmr_03 ENTRY_5`) and verify the live control stack
2. verify `tools/regen-parallel-rdp-shaders.sh` regenerates `slangmosh.hpp` (gates all shader work)
3. 1x-vs-4x sampler falsification experiment before any shader edit
4. sampler fix as dictated + mipmaps + wire the `hirestex-filter` option + exempt hires-replaced TEX_RECTs from `native_resolution_tex_rect`
5. bounded GlideN64 reference rig
6. interactive agent-play adapter mode (no daemons, no sockets)
7. breadth: SM64/OoT packs when acquired

Paper Mario is the strict validation title until the first major milestone is stable; SM64/OoT are compat-path breadth checks only.

## Key Paths
- Active renderer work: [mupen64plus-video-paraLLEl](/home/auro/code/parallel-n64/mupen64plus-video-paraLLEl)
- Libretro seam: [libretro/libretro.c](/home/auro/code/parallel-n64/libretro/libretro.c)
- Current tests: [tests/emulator_behavior](/home/auro/code/parallel-n64/tests/emulator_behavior)
- Fixtures: [tools/fixtures](/home/auro/code/parallel-n64/tools/fixtures)
- Scenarios: [tools/scenarios](/home/auro/code/parallel-n64/tools/scenarios)
- Adapters: [tools/adapters](/home/auro/code/parallel-n64/tools/adapters)
- RetroArch control patches: [tools/retroarch-patches](/home/auro/code/parallel-n64/tools/retroarch-patches)
- Texture pack tracking: [assets/TEXTURE_PACKS.md](/home/auro/code/parallel-n64/assets/TEXTURE_PACKS.md)

## Repo Boundaries
- parallel-n64: planning source of truth, fixture metadata, scenario runners, adapters, evidence conventions, video-core implementation
- RetroArch (`agent-control` branch): frontend/tooling patch target for deterministic control, capture, reporting
- papermario (upstream decomp): optional debug-only semantic reference, not a correctness authority
- Do not put emulator-specific renderer meaning into RetroArch
- Keep cross-project orchestration in parallel-n64

## Fixture And Evidence Contract
- Each fixture is defined by: manifest, ROM identity, savestate identity, config snapshot, expected capture points
- Steady-state fixture path: authoritative savestate -> settle 3 frames -> capture
- Evidence bundles are required for fixture runs and must include: final capture, fixture identity, config snapshot, ROM hash, savestate hash, hi-res pack hash, relevant logs, hit/miss reporting, semantic traces
- Corruption is always fail; explicit fallback with a reported reason is acceptable when the plan claims it

## Working Rules
- PROTECTED PROPERTY: with hi-res + scaling OFF, the core must stay upstream-grade stable. Feature-off screenshot digest equality is a valid gate there (and for remint authority verification) and nowhere else.
- Hi-res-ON validation must never use pixel digests or exact metadata counts. Use class-level semantics (entry/draw-hit presence, source-mode class, explicit fallback reasons) plus the visual review rubric: paired before/after captures with an explicit yes / no / wrong-texture / wrong-region judgment per scene.
- GlideN64 guardrails: use GlideN64 only as a source-level semantic oracle, a txDump hash-coverage oracle, or side-by-side screenshots for content judgment. Never add a numeric image-similarity metric against GlideN64 output to tests or pass/fail logic, never justify a renderer commit with "match GlideN64", never add per-scene/per-descriptor renderer overrides.
- Treat checksum-shaped runtime evidence as suspect by default. Preserve hashes for artifact identity/provenance, but do not promote checksum matches into runtime correctness policy.
- Prefer explicit classification: baseline issue, hi-res issue, scaling issue, or tooling/fixture issue
- Keep machine-specific path assumptions aligned with [WORKSPACE_PATHS.md](/home/auro/code/parallel-n64/docs/WORKSPACE_PATHS.md)
- Emulator-facing runtime tests run at `4x` internal scale, one at a time; they are heavy and occupy the display

## Commit Attribution
AI commits must include a `Co-Authored-By` line identifying the actual agent and provider, e.g.:
```text
Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>
```
