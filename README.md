# parallel-n64 Agent Workspace

## Mission

This repo is the planning and implementation home for a stable hi-res texture
replacement and scaling program for the ParaLLEl video core.

The project is run as an agent-first workflow:

- the docs should let a new agent understand the mission quickly
- the plans should make phase, scope, and exit criteria explicit
- the tooling should make debugging reproducible without UI guesswork

## Current Status

The project rebooted on 2026-06-10 under
[REBOOT_PLAN.md](/home/auro/code/parallel-n64/docs/REBOOT_PLAN.md); the prior Attempt
B plan stack is archived under
[docs/history/](/home/auro/code/parallel-n64/docs/history). As of 2026-06-12 the
reboot work order is substantially complete (per-step status in the plan): the
savestate ladder, sampler/mip/filter stack, texrect exemptions, GlideN64 reference
rig, and interactive agent-play adapter are all landed; active work is compat-keying
conformance, beat-comparison validation, and pack-curation triage.

The standing facts:

- Paper Mario is the strict validation title; SM64/OoT/MK64/MM are compat-path
  breadth checks. All five have converted, boot-validated packs.
- The GlideN64-compat Rice-CRC lane is the primary identity path; the
  native-sampled-identity program is frozen.
- Runtime hi-res loading is `.phrb` only; legacy `.hts`/`.htc` packs are offline
  `hts2phrb` conversion inputs.
- With hi-res + scaling OFF, the core must stay upstream-grade stable (the
  protected property).

## Reading Path

Progressively deeper, in order:

1. This README — mission, status, map.
2. [AGENTS.md](/home/auro/code/parallel-n64/AGENTS.md) — working rules, commands,
   active scope, boundaries.
3. [docs/REBOOT_PLAN.md](/home/auro/code/parallel-n64/docs/REBOOT_PLAN.md) — the
   controlling plan with per-step status and the current frontier.
4. [docs/adr/](/home/auro/code/parallel-n64/docs/adr/README.md) — decision records:
   why the architecture, validation methodology, identity lane, and tooling are the
   way they are.
5. [docs/README.md](/home/auro/code/parallel-n64/docs/README.md) — index of the
   reference docs (testing, paths, signal tables, surveys).
6. [PROJECT_NOTES.md](/home/auro/code/parallel-n64/PROJECT_NOTES.md) — the running
   narrative record, newest entries last.

## Key Repo Areas

- [mupen64plus-video-paraLLEl](/home/auro/code/parallel-n64/mupen64plus-video-paraLLEl): active video-core implementation target
- [libretro/libretro.c](/home/auro/code/parallel-n64/libretro/libretro.c): frontend/core option seam in this repo
- [tests/emulator_behavior](/home/auro/code/parallel-n64/tests/emulator_behavior): current emulator behavior test surface
- [tools/fixtures](/home/auro/code/parallel-n64/tools/fixtures): versioned fixture metadata
- [tools/scenarios](/home/auro/code/parallel-n64/tools/scenarios): deterministic scenario runners
- [tools/adapters](/home/auro/code/parallel-n64/tools/adapters): cross-repo wrapper glue
- [artifacts](/home/auro/code/parallel-n64/artifacts): generated workflow output (gitignored)

Related repos: [RetroArch](/home/auro/code/RetroArch) (`agent-control` branch),
[papermario](/home/auro/code/papermario) (debug-only reference), and the private
session-lab repo `parallel-n64-lab` (ADR-0017). Canonical local layout:
[docs/WORKSPACE_PATHS.md](/home/auro/code/parallel-n64/docs/WORKSPACE_PATHS.md).

## Local Commands

- `./run-build.sh`
- `./run-tests.sh --profile emu-required` — the required gate
- `./run-tests.sh --profile emu-runtime-conformance` — lavapipe smoke + the Paper
  Mario runtime authority lane (sets the runtime opt-in automatically; heavy,
  occupies the display)

See [EMU_TESTING.md](/home/auro/code/parallel-n64/docs/EMU_TESTING.md) for the test
tiers, skip-vs-fail policy, and the on-demand SM64/OoT breadth lanes.
