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
- The current best experimental path combines the existing non-linear row-phase-aware VI reconstruction with source-domain X/Y step biases in `scale_stage()`.
- That path now has two meaningful parts:
  - `vi_scale.frag` keeps the tuned `0/2/7/18` row-phase schedule, the upward-skewed `upper 8/16`, `lower 7/16` 4-tap footprint, the localized `y_frac` remap for phases `1/2` when the source row is in the upper band, and now splits phase-1 source-Y handling between the upper and lower source bands.
  - `scale_stage()` now also applies tested experimental source-domain biases through `vi_scale_sampling_policy`, currently `x_add -= 17`, `y_add -= 30`, `y_base += 736`, a phase-1 source Y bias of `+384` in the upper band, a phase-1 lower-band source Y bias of `-512`, and a phase-3-only source Y bias of `+512` in the upper band for the validated 4x path.
  - local experimentation can override the global source-domain values at runtime with `PARALLEL_VI_SOURCE_{X,Y}_{ADD,BASE}_BIAS`, and can probe the per-phase seam controls with `PARALLEL_VI_PHASE1_Y_BIAS`, `PARALLEL_VI_PHASE1_LOWER_Y_BIAS`, `PARALLEL_VI_PHASE3_Y_BIAS`, and `PARALLEL_VI_PHASE3_X_BIAS`.
  - the current committed baseline from those sweeps is `x_add -= 17`, `y_add -= 30`, `y_base += 736`, `phase1_upper_y += 384`, `phase1_lower_y -= 512`, and `phase3_y += 512`, which improved the saved Paper Mario oracle to a clean `full 18.5840 / left 19.3148 / right 30.2321 / top 17.9058 / bottom 21.0256 / file2_new 2.9931`.
- That row-phase adjustment materially reduces the remaining 4x cadence artifact in the `scale` dump:
  - previous committed experimental path: `mod4 spread 5.7583`, `mod8 spread 6.5015`, `mod12 spread 6.4761`
  - prior committed row-phase path: `mod4 0.5964`, `mod8 1.3461`, `mod12 1.9907`
  - prior best row-phase-only path: `mod4 0.4867`, `mod8 1.1960`, `mod12 1.7706`
  - current combined path: `mod4 0.5443`, `mod8 1.2770`, `mod12 1.8259`
- Latest Paper Mario oracle comparison for the current experimental path:
  - clean committed run: `full 18.5840`, `left 19.3148`, `right 30.2321`, `top 17.9058`, `bottom 21.0256`, `file2_new 2.9931`
  - repeated same-code captures still show a small left-side variance, which moves the `left` crop and `full` metric slightly while leaving `right`, `top`, `bottom`, and `file2_new` unchanged; this remains the main source of full-frame metric wobble
- Practical read on the latest improvement: source-domain mapping remains the dominant lane, and the current best result now depends on phase-specific source Y biases in both the upper and lower source bands. The upper-band phase-1/phase-3 controls shrink the top seam, while the lower-band phase-1 control tightens the right/bottom residual without giving back the top gain.
- Practical read: the current combined path is a better visual/oracle baseline even though the raw cadence metric is slightly worse than the prior row-phase-only variant. Keep both facts in mind before optimizing only against the row spread numbers.
- The plugin-level `interlacing` toggle was previously not threaded into `ScanoutOptions` at all. That is now fixed: it maps to weave/persistence mode (`blend_previous_frame = true`, `upscale_deinterlacing = false`) when enabled.
- On the current Paper Mario save-state scene, forcing that interlacing path changes only a narrow left-side region and does not materially move the main oracle regions, so it is not the dominant source of the horizontal split artifact here.
- There is still more room to improve this VI path, but it did not eliminate the main horizontal seam / banding issue on the file-select scene.
- Treat VI sample-phase tuning as a proven secondary lever, not the primary remaining blocker.
- When work resumes, preserve the current experimental path as a better baseline, but focus new effort on the horizontal-line artifact before spending many more cycles on small VI kernel refinements.
- Use `./run-paper-mario-scaling-capture.sh --tag <tag> --dump-vi-stages aa,divot,scale,final` to localize scanout-stage regressions.
- The VI dump trigger is aligned to screenshot time, so state-mode dumps represent the loaded Paper Mario save-state scene rather than the launch frame.
- On the current Paper Mario 4x state workflow, `scale` and `final` are byte-identical, so the remaining horizontal-line artifact is already present by the end of `scale_stage()` in this path.
- Accurate vs experimental state dumps now diverge almost entirely at `scale/final`, while `aa/divot` remain effectively unchanged, which keeps the main investigation centered on `scale_stage()`.

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
