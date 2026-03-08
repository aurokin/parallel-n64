# VI Source Mapping Research

## Purpose

Capture the current theory for the native 4x scaling bug, then update it with findings from the read-only N64 documentation set in `/home/auro/code/n64_docs`.

This note is meant to preserve reasoning across compaction and handoff. It is not a final design doc.

## Pre-Research Hypotheses

Before reading the N64 docs, the working assumptions were:

- The remaining Paper Mario HIRES-off 4x mismatch is primarily a VI source-coordinate problem, not a texture replacement problem and not mainly a shader-kernel problem.
- The current experimental wins came from correcting source-domain inputs into `scale_stage()` and `vi_scale.frag`, especially:
  - `x_add`
  - `y_add`
  - `y_base`
  - phase-specific source Y adjustments
- The residual error is cadence-locked to the 4x output grid.
- The residual error is not uniform:
  - upper-band error tends to behave differently from lower-band error
  - phase `3` has been a strong lever for upper-band seam reduction
  - phase `1` has been a strong lever for lower/right cleanup
- The current implementation is still an empirical approximation of a deeper rule. The exact constants are likely not the final answer.

## Research Inputs

Read-only references consulted:

- `/home/auro/code/n64_docs/n64brew_Video_Interface.html`
- `/home/auro/code/n64_docs/repeater64_Readme.md`
- `/home/auro/code/n64_docs/parallel-rdp_README.md`
- `/home/auro/code/n64_docs/official_manual/N64OnlineManuals51/n64man/os/osViSetMode.htm`
- `/home/auro/code/mupen/parallel-rdp-upstream/parallel-rdp/video_interface.cpp`
- `/home/auro/code/mupen/parallel-rdp-upstream/parallel-rdp/shaders/vi_scale.frag`
- `/home/auro/code/mupen/GLideN64-upstream/src/GraphicsDrawer.cpp`
- `/home/auro/code/mupen/GLideN64-upstream/src/VI.cpp`
- `/home/auro/code/mupen/GLideN64-upstream/src/TexrectDrawer.cpp`

## Post-Research Update

### What the docs reinforced

- VI behavior is more structured than the current local single-global-source model suggests.
- Interlace / non-interlace / serrate / deflicker behavior is field-sensitive and line-sensitive.
- Low-resolution non-interlaced and interlaced modes perform different vertical reconstruction behavior.
- The official manual explicitly describes line interpolation behavior in interlaced low-resolution modes.
- `repeater64` confirms that emulators often fail on line-by-line VI effects and show short horizontal artifacts when VI behavior is wrong.

### What upstream paraLLEl-RDP clarified

- Upstream paraLLEl-RDP does not rely on one global `x_start/x_add/y_start/y_add` pair at scanout time.
- It builds per-scanline horizontal information in `decode_vi_registers(HorizontalInfoLines *lines)`.
- The VI shader then samples with a more explicit model:
  - `x = coord.x * x_add + x_start`
  - `y = (coord.y - y_base) * y_add + y_start`
- The shader also indexes per-line scanout data via `uHorizontalInfo`.

This is important because our current experimental fixes are acting like a piecewise correction layered on top of a simpler global model. That strongly suggests the bug class is real source-coordinate mis-modeling, not arbitrary tuning noise.

### What GLideN64 clarified

- GLideN64 is not solving this at the same layer as paraLLEl-RDP.
- It leans on:
  - native-res framebuffers
  - `TexrectDrawer`
  - native-res texrect handling
  - presentation-side filtering / hybrid filtering
- That helps explain why GLide can look cleaner without telling us the exact VI rule we need to implement.

So GLide remains a visual oracle, but not a direct implementation oracle for this problem.

## Current Interpretation

The current best reading is:

- The empirical phase/band corrections are compensating for a real deficiency in the local source-coordinate model.
- The defect is not one uniform seam. It is split by both:
  - scanline phase
  - vertical band / region
- The right long-term fix is probably not "more constants."
- The right long-term fix is a clearer derived source-coordinate rule, or a more explicit line-aware scanout model, informed by VI semantics.

## Practical Next Steps

1. Keep using the current experimental baseline as the best local result.
2. Derive a clearer rule for source-coordinate rebasing from VI semantics instead of continuing with blind sweeps.
3. Prefer a lightweight derived piecewise rule first.
4. Only revisit a larger per-line scanout port if the derived rule cannot explain the remaining mismatch.

## Non-Conclusions

This research does not prove that upstream paraLLEl-RDP output is correct for this scene.

It only shows that:

- upstream uses a richer coordinate model than the local simplified path
- our current wins are consistent with fixing a real source-mapping error
- GLide's cleaner image is achieved through a substantially different architecture
