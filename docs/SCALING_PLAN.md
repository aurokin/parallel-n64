# Scaling Plan

## Goal

Improve native 4x HIRES-off scaling quality in `parallel` while preserving the existing default path when the experimental override is off.

This plan is for active work only. Completed exploration history lives in git and supporting notes such as [VI_SOURCE_MAPPING_RESEARCH.md](/home/auro/code/parallel-n64/docs/VI_SOURCE_MAPPING_RESEARCH.md).

Current high-value finding:

- the Paper Mario file-select backdrop is not behaving like ordinary 3D geometry; it is assembled from repeated `copy=1` horizontal strips, so texrect/copy behavior must now be treated as a first-class part of the scaling problem
- the file-select scene is a mix of copy and non-copy texrect composition, not just the `copy=1` strips; broad native texrect behavior is what helps here
- the texrect lane is now separated from the VI/source-mapping lane
- plain `scaling-mode=experimental` now means `texrect on, VI off`
- the VI/source-mapping lane is opt-in through `parallel-n64-parallel-rdp-experimental-vi=enabled`
- the texrect lane is switchable through `parallel-n64-parallel-rdp-experimental-texrect=auto|enabled|disabled`

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

The current best combined path is still the best oracle-scoring local result for the Paper Mario file-select scene, but the original texrect bug fix has been separated from the VI/source-mapping lane.

Files:

- [video_interface.cpp](/home/auro/code/parallel-n64/mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp/video_interface.cpp)
- [vi_scale.frag](/home/auro/code/parallel-n64/mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp/shaders/vi_scale.frag)
- [vi_scale_sampling_policy.hpp](/home/auro/code/parallel-n64/mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp/vi_scale_sampling_policy.hpp)

Current opt-in VI baseline:

- the heuristic reconstruction lane has been removed
- the remaining VI path is intentionally narrow and documentation-driven
- when explicitly enabled, it preserves the baseline VI sampling kernel and only applies field-relative source-Y handling for serrated/interlaced scanout, matching the documented `Y_OFFSET + Y_SCALE / 2` style behavior

Current expectation for Paper Mario file-select:

- this scene is progressive and texrect-heavy
- the primary bug fix is the texrect lane
- the trimmed VI accuracy-improvement path is expected to have little or no visible effect here

## What We Believe Now

Based on the current experiments and [VI_SOURCE_MAPPING_RESEARCH.md](/home/auro/code/parallel-n64/docs/VI_SOURCE_MAPPING_RESEARCH.md):

- The texrect fix solved the original Paper Mario stripe bug.
- The old VI lane mixed real VI/source-mapping ideas with scene-tuned reconstruction heuristics.
- Only part of that lane was supported by docs.
- The docs support a much narrower explanation:
  - `Y_SCALE` is accumulated every scanline
  - source Y is stateful across the frame
  - field-relative half-line behavior matters
  - `Y_OFFSET` and sometimes `ORIGIN` participate in field-relative positioning

So the VI lane is now treated as an accuracy-improvement track only, and only the doc-backed field/source-Y behavior remains committed.

The practical split now is:

- original bug fix: texrect lane
- follow-up quality work: VI/source-mapping lane

## Active Plan

### Add Experiments

These are the things we should experimentally add next.

- Add a field-relative source-Y offset experiment based on the documented `Y_SCALE / 2` interlace-style behavior, but keep it behind the experimental mode.
- Add tests or validation scenes that actually exercise serrated/interlaced output before expanding the VI accuracy-improvement path again.
- Add targeted instrumentation only when needed to verify the live values fed into `vi_scale.frag`.
- Add one or two additional validation scenes after each meaningful improvement:
  - another Paper Mario scene
  - one non-Paper-Mario 2D-heavy scene

### Try To Remove

These are the things we should try to remove from the current approximation as we replace them with clearer rules.

- Remove any future VI tweaks that cannot be tied back to documented VI semantics or tests.
- Remove hidden reconstruction heuristics if they start creeping back into `vi_scale.frag`.

### Avoid For Now

- Do not spend many more cycles on generic kernel/tap-layout churn unless a new source-mapping rule stalls.
- Do not reintroduce hidden policy overrides that force texrect behavior off in experimental mode.
- Do not narrow the texrect fix back down to copy-cycle-only handling unless a broader scene set proves it safe.
- Do not copy upstream paraLLEl-RDP or GLideN64 structure wholesale.

## Immediate Next Steps

1. Derive a candidate source-Y rule from the documented accumulated `Y_SCALE` behavior.
2. Extend that rule beyond the phase-Y cleanup and the first upper-band `y_line_base` split, and test whether the remaining band logic can also be reduced.
3. If the rule helps, remove one constant at a time and revalidate.
4. If the rule does not help enough, revisit a more explicit line-aware scanout model.

Near-term texrect lane:

1. Treat the file-select center backdrop as a copy/texrect strip-composition testcase.
2. Treat texrect-heavy composition as the current intrusive lane: the working improvement comes from broad texrect native-resolution handling, not copy-cycle-only handling.
3. Compare bad strip-composed textures against clean textures in the same scene to determine whether the remaining seam is in copy/texrect composition before VI.

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
