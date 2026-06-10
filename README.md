# parallel-n64 Agent Workspace

## Mission

This repo is the planning and implementation home for a stable hi-res texture replacement and scaling program for the ParaLLEl video core.

The project is being run as an agent-first workflow:

- the docs should let a new agent understand the mission quickly
- the plans should make phase, scope, and exit criteria explicit
- the tooling should make debugging reproducible without UI guesswork

## Current Status

The project rebooted on 2026-06-10. The controlling plan is
[REBOOT_PLAN.md](/home/auro/code/parallel-n64/docs/REBOOT_PLAN.md); the prior
Attempt B plan stack is archived under
[docs/history/](/home/auro/code/parallel-n64/docs/history).

Paper Mario remains the strict validation title.
The active fixtures are title screen, file select, and `kmr_03 ENTRY_5`; SM64/OoT are cross-game compat-path breadth checks.
The GlideN64-compat Rice-CRC lane is the primary identity path; the native-sampled-identity program is frozen.
Runtime hi-res loading is `.phrb` only. Legacy `.hts` / `.htc` packs are manual `hts2phrb` conversion inputs, not supported runtime inputs.

## Start Here

- [AGENTS.md](/home/auro/code/parallel-n64/AGENTS.md)
- [Reboot Plan](/home/auro/code/parallel-n64/docs/REBOOT_PLAN.md)
- [Workspace Paths](/home/auro/code/parallel-n64/docs/WORKSPACE_PATHS.md)
- [Project Notebook](/home/auro/code/parallel-n64/PROJECT_NOTES.md)
- [Docs Index](/home/auro/code/parallel-n64/docs/README.md)

## Key Working Rules

- `feature off` must preserve baseline behavior unless a change clearly brings the renderer closer to N64 parity
- `feature on` prioritizes correctness and diagnosability over apparent early coverage
- fixture runs require evidence bundles
- fallback and exclusion behavior must be explicit and logged
- image digest equality is only a valid correctness gate for feature-off baseline parity and remint authority verification; hi-res-on validation must use semantic/runtime evidence instead
- savestates are the authority once available; debug warps and scripted entry are acceptable earlier in the ladder
- emulator-facing runtime tests should run at `4x` internal scale and one at a time
- tracked RetroArch runtime scenarios should use fullscreen windows and should not start while another `retroarch` process is active

## Active Repos In Scope

- [parallel-n64](/home/auro/code/parallel-n64)
- [RetroArch](/home/auro/code/RetroArch) (`agent-control` branch)
- [papermario](/home/auro/code/papermario) (upstream decomp, debug-only reference)

Use [Workspace Paths](/home/auro/code/parallel-n64/docs/WORKSPACE_PATHS.md) for the canonical local layout on this machine.

## Key Repo Areas

- [mupen64plus-video-paraLLEl](/home/auro/code/parallel-n64/mupen64plus-video-paraLLEl): active video-core implementation target
- [libretro/libretro.c](/home/auro/code/parallel-n64/libretro/libretro.c): frontend/core option seam in this repo
- [tests/emulator_behavior](/home/auro/code/parallel-n64/tests/emulator_behavior): current emulator behavior test surface
- [tools/fixtures](/home/auro/code/parallel-n64/tools/fixtures): versioned fixture metadata
- [tools/scenarios](/home/auro/code/parallel-n64/tools/scenarios): deterministic scenario runners
- [tools/adapters](/home/auro/code/parallel-n64/tools/adapters): cross-repo wrapper glue
- [artifacts](/home/auro/code/parallel-n64/artifacts): generated workflow output

## Local Commands

- `./run-build.sh`
- `./run-tests.sh`
- `./run-tests.sh --profile emu-required`
- `./run-tests.sh --profile emu-runtime-conformance`

See [EMU_TESTING.md](/home/auro/code/parallel-n64/docs/EMU_TESTING.md) for the current test tiers.

Current runtime-conformance note:

- `emu-runtime-conformance` runs the lavapipe smoke check plus the single Paper Mario lane `emu.conformance.paper_mario_full_cache_phrb_authorities` and sets the `EMU_ENABLE_RUNTIME_CONFORMANCE=1` opt-in automatically
- the SM64/OoT boot and title-fixture lanes are registered for on-demand `ctest -R` runs only
- once opted in, missing `PHRB`, ROM, savestate, or RetroArch prerequisites fail loudly with staging instructions
