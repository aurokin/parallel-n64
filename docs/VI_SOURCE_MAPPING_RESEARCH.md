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

### Concrete hardware details from the docs

The most useful details from `/home/auro/code/n64_docs/n64brew_Video_Interface.html` and the official manuals were:

- `VI_X_SCALE` and `VI_Y_SCALE` are both 2.10 fixed-point inverse scale-up factors.
- The VI keeps an internal accumulated vertical offset register:
  - it is initialized from `Y_OFFSET`
  - `Y_SCALE` is added every scanline
  - the integer part of that running value selects the framebuffer Y position
- Mid-frame `VI_Y_SCALE` changes do not reset that accumulated vertical register.
- Mid-frame `VI_ORIGIN` changes also do not reset the internally accumulated vertical offset.
- Serration is only one part of interlacing:
  - the other part is an odd number of half-scanlines
  - to display the odd field correctly, software typically needs to offset `Y_OFFSET` by `Y_SCALE / 2`
  - if that offset overflows the fractional domain, the integer part must be expressed via `VI_ORIGIN`
- `VI_WIDTH` and `VI_ORIGIN` can both participate in odd/even field stepping for interlaced output.
- The official manual describes low-resolution interlaced vertical reconstruction as alternating 75/25 and 25/75 line weighting between fields.

These points matter because they strongly support a model where:

- the effective source Y mapping is stateful across scanlines
- field-relative source positioning is not just a global constant
- some of the empirical phase/band fixes we found may be approximating missing half-line / accumulated-offset behavior

### One especially relevant doc clue

The n64brew VI page calls out a very common horizontal bug:

- with `X_SCALE <= 0x200` and low `H_START`, the VI can generate invalid output
- a common workaround for a nominal `0x200` case is to use `0x201`

That does not directly explain our current best `x_add -= 17` correction, but it is an important reminder that VI coordinate behavior is not perfectly "ideal math." There are hardware quirks around exact scale values, start values, and filtering modes.

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

### What the current texrect lane clarified

- The Paper Mario file-select scene is not just a VI-only problem.
- The center backdrop is composed from repeated texrect strips, and current tracing also shows non-copy texrect composition in the same scene.
- A copy-cycle-only native-resolution experiment was inert.
- Broad native texrect handling in the experimental upscaled path is what materially improves the stable `right/top/bottom` regions.

That means the remaining seam is now best treated as a mixed problem:

- source-coordinate / VI reconstruction still matters
- but texrect composition is also a first-class contributor

## Current Interpretation

The current best reading is:

- The empirical phase/band corrections are compensating for a real deficiency in the local source-coordinate model.
- The defect is not one uniform seam. It is split by both:
  - scanline phase
  - vertical band / region
- The right long-term fix is probably not "more constants."
- The right long-term fix is a clearer derived source-coordinate rule, or a more explicit line-aware scanout model, informed by VI semantics.

More specifically after reading the docs:

- the current local path is probably missing some combination of:
  - accumulated vertical offset semantics
  - field-relative half-line positioning
  - piecewise source rebasing between scanline bands / fields
- the current empirical upper-band and lower-band phase-Y corrections are plausible approximations of those missing behaviors
- the first cleanup steps are now validated locally:
  - three of those phase-Y adjustments can be derived directly from raw `Y_SCALE` without changing the Paper Mario oracle result
  - a fourth lower-band phase-3 adjustment also improved the current oracle when derived from raw `Y_SCALE / 4`
- the remaining right/bottom residual also responds to a lower-band-only phase-3 X correction; current best local rule is `phase3_x += 128` in the lower band only
- that phase-3 X correction can now also be expressed directly from raw `Y_SCALE`:
  - `lower-band phase3_x += raw_y_add / 8`
- the structural `y_start / line-base` split is also now validated in one limited form:
  - an upper-band-only line-base term improves `top` and `right` without moving the bottom region
  - a lower-band-only line-base term then improves `bottom/right`
  - those band terms can now be expressed directly from raw `Y_SCALE`:
    - `upper-band y_line_base -= 3 * raw_y_add / 4`
    - `lower-band y_line_base += raw_y_add / 4`
- the remaining source-Y base bias can also now be expressed directly from raw `Y_SCALE`:
  - `y_base += 23 * raw_y_add / 32`
- the remaining source-step biases can now also be expressed directly from raw scale:
  - `x_add -= raw_x_add / 32 + raw_x_add / 512`
  - `y_add -= raw_y_add / 32 - raw_y_add / 512`
- source X base can now also be expressed directly from raw `X_SCALE`:
  - `x_base += raw_x_add / 16`
- the upper/lower band boundary can now also be expressed directly from raw `Y_SCALE`:
  - `upper_band_limit = 5 * raw_y_add / 8`
- using that derived band split consistently in both the outer source-coordinate path and the inner `sample_divot_output()` path is a real visual improvement; leaving a stale hardcoded `640` in the inner path was part of the remaining mismatch
- a principled replacement should be derived from VI register semantics first, not from more blind sweeps

## Practical Next Steps

1. Keep using the current experimental baseline as the best local result.
2. Derive a clearer rule for source-coordinate rebasing from VI semantics instead of continuing with blind sweeps.
3. Focus first on:
   - accumulated `Y_SCALE` semantics
   - field-relative `Y_OFFSET` / `Y_SCALE / 2` behavior
   - whether our current phase-1 / phase-3 split matches a half-line or fractional-field offset model
4. Prefer a lightweight derived piecewise rule first.
5. Only revisit a larger per-line scanout port if the derived rule cannot explain the remaining mismatch.
6. Keep texrect composition in scope while doing that; the current best local result now depends on broad native texrect handling in the experimental path.

## Non-Conclusions

This research does not prove that upstream paraLLEl-RDP output is correct for this scene.

It only shows that:

- upstream uses a richer coordinate model than the local simplified path
- our current wins are consistent with fixing a real source-mapping error
- GLide's cleaner image is achieved through a substantially different architecture

## Current Code Policy

The repo no longer ships the older heuristic VI reconstruction lane.

Current committed policy is narrower:

- keep texrect handling as the practical fix for the Paper Mario stripe bug
- keep VI changes opt-in only
- only keep VI behavior that can be explained as a documented accuracy improvement, primarily field-relative source-Y handling around `Y_OFFSET` / `Y_SCALE / 2`
