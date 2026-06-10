# Scenario Runners

This directory is for deterministic end-to-end workflows.

The shared runtime contract is documented in [MODEL.md](/home/auro/code/parallel-n64/tools/scenarios/MODEL.md).

Use it for:

- launching RetroArch/core/content with known settings
- loading savestates or replay checkpoints
- capturing frames, screenshots, and logs
- comparing outputs against expected results
- producing small reproducible reports

Do not use it for:

- storing large assets
- game-specific source instrumentation
- permanent research notes

Scenario runners should consume fixture manifests from [`tools/fixtures/`](/home/auro/code/parallel-n64/tools/fixtures) and write generated output to [`artifacts/`](/home/auro/code/parallel-n64/artifacts).

Current tracked scenario seeds:

- [`paper-mario-title-screen.sh`](/home/auro/code/parallel-n64/tools/scenarios/paper-mario-title-screen.sh)
- [`paper-mario-title-screen.runtime.env`](/home/auro/code/parallel-n64/tools/scenarios/paper-mario-title-screen.runtime.env)
- [`paper-mario-file-select.sh`](/home/auro/code/parallel-n64/tools/scenarios/paper-mario-file-select.sh)
- [`paper-mario-file-select.runtime.env`](/home/auro/code/parallel-n64/tools/scenarios/paper-mario-file-select.runtime.env)
- [`paper-mario-kmr-03-entry-5.sh`](/home/auro/code/parallel-n64/tools/scenarios/paper-mario-kmr-03-entry-5.sh)
- [`paper-mario-kmr-03-entry-5.runtime.env`](/home/auro/code/parallel-n64/tools/scenarios/paper-mario-kmr-03-entry-5.runtime.env)
- [`paper-mario-file-select-input-probe.sh`](/home/auro/code/parallel-n64/tools/scenarios/paper-mario-file-select-input-probe.sh)
- [`paper-mario-title-timeout-probe.sh`](/home/auro/code/parallel-n64/tools/scenarios/paper-mario-title-timeout-probe.sh)
- [`paper-mario-phrb-authority-validation.sh`](/home/auro/code/parallel-n64/tools/scenarios/paper-mario-phrb-authority-validation.sh)
- [`paper-mario-full-cache-phrb-authority-validation.sh`](/home/auro/code/parallel-n64/tools/scenarios/paper-mario-full-cache-phrb-authority-validation.sh)
- [`cross-game-hires-boot-validation.sh`](/home/auro/code/parallel-n64/tools/scenarios/cross-game-hires-boot-validation.sh)
- [`cross-game-hires-savestate-fixture-validation.sh`](/home/auro/code/parallel-n64/tools/scenarios/cross-game-hires-savestate-fixture-validation.sh)
- [`remint-cross-game-boot-state.sh`](/home/auro/code/parallel-n64/tools/scenarios/remint-cross-game-boot-state.sh)
- [`paper-mario-savefile-start.runtime.env`](/home/auro/code/parallel-n64/tools/scenarios/paper-mario-savefile-start.runtime.env)
- [`paper-mario-hos-05-entry-3.sh`](/home/auro/code/parallel-n64/tools/scenarios/paper-mario-hos-05-entry-3.sh)
- [`paper-mario-hos-05-entry-3.runtime.env`](/home/auro/code/parallel-n64/tools/scenarios/paper-mario-hos-05-entry-3.runtime.env)
- [`remint-paper-mario-title-screen-authority.sh`](/home/auro/code/parallel-n64/tools/scenarios/remint-paper-mario-title-screen-authority.sh)
- [`remint-paper-mario-file-select-authority.sh`](/home/auro/code/parallel-n64/tools/scenarios/remint-paper-mario-file-select-authority.sh)
- [`remint-paper-mario-kmr-03-entry-5-authority.sh`](/home/auro/code/parallel-n64/tools/scenarios/remint-paper-mario-kmr-03-entry-5-authority.sh)
- [`remint-paper-mario-hos-05-entry-3-authority.sh`](/home/auro/code/parallel-n64/tools/scenarios/remint-paper-mario-hos-05-entry-3-authority.sh)
- [`stage-paper-mario-savefile.sh`](/home/auro/code/parallel-n64/tools/scenarios/stage-paper-mario-savefile.sh)
- [`gliden64-reference-capture.sh`](/home/auro/code/parallel-n64/tools/scenarios/gliden64-reference-capture.sh)
- [`compose-reference-pair.py`](/home/auro/code/parallel-n64/tools/scenarios/compose-reference-pair.py)

## GlideN64 Reference Rig

- [`gliden64-reference-capture.sh`](/home/auro/code/parallel-n64/tools/scenarios/gliden64-reference-capture.sh) captures a scene on the mupen64plus-next/GLideN64 vehicle with the legacy `.hts` pack loaded, reaching scenes by boot + timed input (paraLLEl savestates do not transfer). [`compose-reference-pair.py`](/home/auro/code/parallel-n64/tools/scenarios/compose-reference-pair.py) stacks a reference/paraLLEl pair with labels for review.
- These captures are a visual CONTENT oracle only ("does the pack content appear, on the right surfaces, at hi-res detail?"). `emu.support.gliden64_reference_guardrails` fails the required gate if image-similarity metrics or GlideN64 capture digest gating appear in the gate surface.

## Active Paper Mario Lanes

- [`paper-mario-title-screen.sh`](/home/auro/code/parallel-n64/tools/scenarios/paper-mario-title-screen.sh), [`paper-mario-file-select.sh`](/home/auro/code/parallel-n64/tools/scenarios/paper-mario-file-select.sh), and [`paper-mario-kmr-03-entry-5.sh`](/home/auro/code/parallel-n64/tools/scenarios/paper-mario-kmr-03-entry-5.sh) are the repo-default authority fixtures. Their normal `on` runs now require a promoted enriched full-cache `PHRB` and fail closed if that runtime artifact is missing.
- [`paper-mario-phrb-authority-validation.sh`](/home/auro/code/parallel-n64/tools/scenarios/paper-mario-phrb-authority-validation.sh) is the shared Paper Mario `PHRB` authority runner. It executes title screen, file select, and `kmr_03 ENTRY_5` against a supplied `.phrb`, records one validation summary with class-level semantics (entries loaded, draw hits, source-mode class, explicit fallback reasons), and rejects legacy runtime inputs.
- [`paper-mario-full-cache-phrb-authority-validation.sh`](/home/auro/code/parallel-n64/tools/scenarios/paper-mario-full-cache-phrb-authority-validation.sh) is the full-cache `PHRB` wrapper used by the `emu.conformance.paper_mario_full_cache_phrb_authorities` runtime lane.

## Active Cross-Game Lanes

- [`cross-game-hires-boot-validation.sh`](/home/auro/code/parallel-n64/tools/scenarios/cross-game-hires-boot-validation.sh) is the shared SM64/OoT boot proof runner. It requires a `.phrb`, runs with hi-res enabled, and writes final capture, logs, extracted hi-res evidence, and `validation-summary.{json,md}`.
- [`cross-game-hires-savestate-fixture-validation.sh`](/home/auro/code/parallel-n64/tools/scenarios/cross-game-hires-savestate-fixture-validation.sh) is the fixed-state SM64/OoT fixture runner. It verifies savestate identity, staged-state load acknowledgement, post-load paused frame, and semantic hi-res evidence. Capture digests are artifact identity only, not hi-res correctness gates.
- [`remint-cross-game-boot-state.sh`](/home/auro/code/parallel-n64/tools/scenarios/remint-cross-game-boot-state.sh) mints baseline-off boot savestates for cross-game fixtures and requires `--expected-verify-capture-sha256` for authority remints; `--allow-unverified` writes review evidence only and does not promote to the final output path.

## Common Experimental Overrides

- Use `RUNTIME_ENV_OVERRIDE=/abs/path/to/runtime.env` for temporary debug-only runs.
- Screenshot verification is reserved for feature-off baseline parity and remint authority checks. Do not add hi-res-on capture digest gates; use semantic traces and hi-res evidence instead.
- Runtime env overrides are auto-exported while sourcing, so `PARALLEL_RDP_*` toggles reach the RetroArch/core process.
- Prefer the real `PARALLEL_RDP_HIRES_FILTER_*` variable names. The older `HIRES_FILTER_*` names remain compatibility fallbacks only.

## Supplemental Research

- Attempt B probe history and deferred seam evidence are archived in [docs/history/PAPER_MARIO_RUNTIME_RESEARCH.md](/home/auro/code/parallel-n64/docs/history/PAPER_MARIO_RUNTIME_RESEARCH.md) (frozen, reference only).
- The controlling docs are:
  - [Reboot Plan](/home/auro/code/parallel-n64/docs/REBOOT_PLAN.md)
  - [EMU_TESTING.md](/home/auro/code/parallel-n64/docs/EMU_TESTING.md)
