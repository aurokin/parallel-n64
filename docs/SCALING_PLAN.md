# Scaling Plan

## Goal

Improve native 4x scaling quality in `parallel` while preserving the current low-level emulation path as the default baseline.

This work is explicitly about the non-HIRES path first. HIRES compatibility must be preserved or deliberately adjusted as part of the design, but HIRES is not the primary oracle for this phase.

## Invariants

- The existing default path must remain unchanged when the new scaling override is off.
- HIRES-off behavior is the primary validation target for this phase.
- The current `upscaling` multiplier remains the source of truth for target resolution.
- The baseline LLE renderer remains intact. The new work should be an override at scanout / reconstruction time, not a replacement for the core renderer.

## Current Oracles

- GLideN64 4x HIRES-off oracle:
  - `/home/auro/code/parallel-n64-paper-mario-backups/20260306-hires-audit/scaling/oracle-gliden64-4x-hires-off-2`
  - validated button-path screenshot: `/home/auro/code/parallel-n64-paper-mario-backups/20260306-hires-audit/scaling/oracle-gliden64-4x-hires-off-2/Paper Mario (USA)-260306-212123.png`
- Matching `parallel` 4x HIRES-off reference:
  - `/home/auro/code/parallel-n64-paper-mario-backups/20260306-hires-audit/scaling/reference-parallel-4x-hires-off-1`
- Full comparison image:
  - `/home/auro/code/parallel-n64-paper-mario-backups/20260306-hires-audit/scaling/paper-mario-glide-vs-parallel-4x-hires-off-full.png`

Do not re-run GLideN64 unless a major method change requires refreshing the oracle.

For current `parallel` iteration, use:

- `./run-paper-mario-scaling-capture.sh --tag <tag>`
- `./run-paper-mario-scaling-compare.sh --tag <tag>`

This keeps scaling work on the same-core save-state path instead of the slower, less stable button-path capture.
Run captures sequentially; the helpers reuse RetroArch netcmd defaults and are not designed for concurrent launches.
The helper currently relies on an explicit temp `core-options.cfg` inside its isolated XDG root; do not switch it back to mixed per-core option mode unless RetroArch behavior changes and you re-validate that path.

## Current Findings

- The experimental VI reconstruction path is real and already improves the saved Paper Mario oracle over accurate mode.
- The current committed improvements came from subpixel reconstruction and X-axis sample-phase tuning in `vi_scale.frag`.
- There is still more room to improve this VI path, but it did not eliminate the main horizontal seam / banding issue on the file-select scene.
- Treat VI sample-phase tuning as a proven secondary lever, not the primary remaining blocker.
- When work resumes, preserve the current experimental path as a better baseline, but focus new effort on the horizontal-line artifact before spending many more cycles on small VI kernel refinements.

## Architectural Direction

The preferred design is:

- keep the current `parallel` path as the default
- add a dedicated scaling override mode
- reuse the existing upscale factor rather than introducing a second resolution configuration path
- improve final reconstruction with as few additional phases as possible

The first implementation target is the VI scanout / reconstruction path:

- `mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp/video_interface.cpp`

The current scale factor already flows through:

- `mupen64plus-video-paraLLEl/rdp.cpp`
- `mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp/rdp_device.cpp`
- `mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp/video_interface.cpp`

## Phase 0: Add Scaling Regression Tests

Before changing scaling behavior, add unit coverage for the current scanout policy so regressions are caught early.

Test targets:

- scale-factor plumbing from integration into scanout
- HIRES-off invariant behavior when the new scaling override is disabled
- downscale-step policy and final pass selection
- scanout option selection for VI AA / VI scale / divot / dither combinations
- any helper logic introduced for alternate scaling mode selection

Test style:

- prefer unit tests around policy / helper functions over screenshot-only validation
- keep image or oracle comparisons as higher-level validation, not the only test signal
- if current code is hard to test directly, refactor small pieces into helper policies first

Acceptance for Phase 0:

- new tests fail on a deliberate regression
- tests pass on the current baseline

## Phase 1: Baseline Measurement and Defect Classification

Use the saved Paper Mario HIRES-off oracle to classify the current visual differences.

Focus on:

- final scanout filtering
- downscale policy
- 2D / texrect-sensitive regions
- seam and banding artifacts

This phase should produce a short defect inventory tied to the current scanout stages, not just screenshots.

## Phase 2: Minimal Alternate Scanout Path

Add a new scaling override mode behind a dedicated core option, default off.

Constraints:

- use the existing upscale multiplier
- do not change default behavior when the override is off
- do not change RDP rasterization behavior in the first pass

Initial implementation target:

- replace or augment final reconstruction in `scale_stage()` and `downscale_stage()`
- avoid stacking many extra passes if a better single-pass or fewer-pass Vulkan approach is possible

Preferred exploration areas:

- explicit Vulkan sampling control for final reconstruction
- higher-quality resolve/downscale path than the current repeated blit-style halving
- modern Vulkan-friendly methods rather than copying GL-era GLide structure

## Phase 3: HIRES Compatibility Validation

After the alternate scaling path works for HIRES-off, validate HIRES-on behavior.

Important assumption:

- HIRES replacement is applied earlier in the pipeline, primarily in the rasterizer / texture replacement path
- the new scaling override should initially leave that path alone

Validation targets:

- replacements still load and bind correctly
- final scene remains stable with the new scanout mode
- no unexpected interaction between replacement sampling and the new reconstruction path

It is acceptable to modify HIRES compatibility behavior if it unlocks a better scaling method, but any such change must be explicit and tested.

## Phase 4: Decide Whether Texrect-Specific Handling Is Necessary

Do not start with texrect specialization.

Only introduce texrect-specific handling if oracle comparison shows that:

- final reconstruction improvements are insufficient, and
- the remaining gap is concentrated in 2D / texrect-driven content

If needed, use the existing texrect-related policy points as the foundation rather than creating unrelated control paths.

## Phase 5: Validation, Tooling, and Documentation

Once the method stabilizes:

- add oracle-driven regression workflow for scaling
- keep HIRES-off and HIRES-on validation paths documented separately
- document the new scaling override, its default-off behavior, and its relationship to the existing upscale multiplier

## Acceptance Criteria

The work is ready to land when all of the following are true:

- the new override materially improves the HIRES-off Paper Mario 4x comparison against the saved GLide oracle
- default-off behavior is unchanged
- unit tests cover the new scaling policy surface
- HIRES-on behavior is still functional under the new path

## Non-Goals

- replacing the baseline LLE path
- adding multiple parallel resolution configuration systems
- reworking HIRES replacement as the first step
- copying GLideN64 architecture wholesale
