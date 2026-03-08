# Scaling Plan

## Goal

Improve native 4x HIRES-off scaling quality in `parallel` while preserving the existing default path when the experimental override is off.

This plan is for active work only. Completed exploration history lives in git and supporting notes such as [VI_SOURCE_MAPPING_RESEARCH.md](/home/auro/code/parallel-n64/docs/VI_SOURCE_MAPPING_RESEARCH.md).

## Invariants

- Default-off behavior must stay unchanged.
- HIRES-off remains the primary validation target.
- The existing upscale multiplier remains the source of truth for target resolution.
- The baseline LLE renderer stays intact; changes should stay in scanout / VI reconstruction unless clearly justified.

## Oracle

- GLideN64 4x HIRES-off oracle:
  - `/home/auro/code/parallel-n64-paper-mario-backups/20260306-hires-audit/scaling/oracle-gliden64-4x-hires-off-2/Paper Mario (USA)-260306-212123.png`

Do not refresh the GLide oracle unless a major method change makes the old comparison invalid.

For `parallel` iteration, use:

- `./run-paper-mario-scaling-capture.sh --tag <tag>`
- `./run-paper-mario-scaling-compare.sh --tag <tag>`

Run captures sequentially.

## Current Baseline

The current committed experimental path is the best known local result for the Paper Mario file-select scene.

Files:

- [video_interface.cpp](/home/auro/code/parallel-n64/mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp/video_interface.cpp)
- [vi_scale.frag](/home/auro/code/parallel-n64/mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp/shaders/vi_scale.frag)
- [vi_scale_sampling_policy.hpp](/home/auro/code/parallel-n64/mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp/vi_scale_sampling_policy.hpp)

Current experimental 4x source/reconstruction baseline:

- `x_add -= 17`
- `y_add -= 30`
- `y_base += 736`
- derived phase-Y adjustments from raw `Y_SCALE`:
  - upper-band `phase1_y += 3 * raw_y_add / 8`
  - lower-band `phase1_y -= raw_y_add / 2`
  - upper-band `phase3_y += raw_y_add / 2`
  - lower-band `phase3_y -= raw_y_add / 4`
- row-phase schedule `0/2/7/18`
- upward-skewed 4-tap footprint `upper 8/16`, `lower 7/16`
- localized `y_frac` remap for phases `1/2` in the upper source band

Current clean Paper Mario compare:

- `full 18.5087`
- `left 19.2639`
- `right 30.2013`
- `top 17.9058`
- `bottom 20.7660`
- `file2_new 2.9931`

Important caveat:

- repeated same-code captures still show left-side variance
- trust `right`, `top`, `bottom`, `file2_new`, and stage dumps more than the `left` crop alone

## What We Believe Now

Based on the current experiments and [VI_SOURCE_MAPPING_RESEARCH.md](/home/auro/code/parallel-n64/docs/VI_SOURCE_MAPPING_RESEARCH.md):

- The main bug class is source-coordinate modeling in the VI upscale path.
- The current constants are useful, but they are still an empirical approximation.
- Two cleanup steps have landed already:
  - the original phase-Y corrections are derived from raw `Y_SCALE` in the shader instead of being stored as three default constants in policy state
  - the remaining lower-band phase-3 residual also responds to a derived raw-`Y_SCALE` term instead of another free-standing policy constant
- The remaining mismatch is split by both:
  - scanline phase
  - vertical band
- The docs support a more principled explanation:
  - `Y_SCALE` is accumulated every scanline
  - source Y is stateful across the frame
  - field-relative half-line behavior matters
  - `Y_OFFSET` and sometimes `ORIGIN` participate in field-relative positioning

So the next goal is not to stack more arbitrary constants. It is to replace some of the current constants with a clearer rule derived from VI semantics.

## Active Plan

### Add Experiments

These are the things we should experimentally add next.

- Add a derived source-Y model based on accumulated `Y_SCALE` semantics instead of treating every correction as a flat bias.
- Add a field-relative source-Y offset experiment based on the documented `Y_SCALE / 2` interlace-style behavior, but keep it behind the experimental mode.
- Add a lightweight derived piecewise source mapping rule that depends on:
  - phase
  - source band
  - current scanout field semantics
- Add targeted instrumentation only when needed to verify the live values fed into `vi_scale.frag`.
- Add one or two additional validation scenes after each meaningful improvement:
  - another Paper Mario scene
  - one non-Paper-Mario 2D-heavy scene

### Try To Remove

These are the things we should try to remove from the current approximation as we replace them with clearer rules.

- Remove hardcoded phase-specific source-Y constants if a derived field/accumulation rule reproduces the same improvement.
- Remove band splits that are only acting as proxies for missing VI state.
- Remove any temporary env-driven sweep hooks once the corresponding behavior is either:
  - promoted to a principled default, or
  - proven dead
- Remove duplicated correction layers when a single earlier source-coordinate correction makes a later shader bias unnecessary.

### Avoid For Now

- Do not spend many more cycles on generic kernel/tap-layout churn unless a new source-mapping rule stalls.
- Do not revisit broad texrect-special handling yet.
- Do not copy upstream paraLLEl-RDP or GLideN64 structure wholesale.

## Immediate Next Steps

1. Derive a candidate source-Y rule from the documented accumulated `Y_SCALE` behavior.
2. Extend that rule beyond the phase-Y cleanup that is already landed, and test whether the remaining band logic can also be reduced.
3. If the rule helps, remove one constant at a time and revalidate.
4. If the rule does not help enough, revisit a more explicit line-aware scanout model.

## Validation Rules

- Always validate with the saved GLide oracle.
- Prefer state-mode `parallel` captures for current scaling work.
- Use `--dump-vi-stages aa,divot,scale,final` when needed.
- If `scale` and `final` match, keep the investigation in `scale_stage()` / `vi_scale.frag`.
- Do not commit a change that only improves one noisy region while regressing the stable regions.

## Acceptance

The current lane is successful when:

- the experimental path improves the stable oracle regions further
- at least one current hardcoded source-phase correction is replaced by a clearer derived rule
- default-off behavior remains unchanged
- unit coverage still protects the policy surface
