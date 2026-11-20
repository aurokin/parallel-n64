# ADR-0011: Texrect policy: native snap by default, evidence-narrowed exemptions

- Status: Accepted
- Date: pre-reboot (default); exemptions 2026-06-10 and 2026-06-11; round-up
  amended 2026-11-20

## Context

Texrects are the highest-risk primitive for hi-res scaling: fixed-point coordinates
with non-trivial rounding, COPY/FILL vs 1-/2-cycle disagreement on lower-right edge
behavior, COPY-mode fetch-width step requirements. Historically, unsnapped copy
strips broke visibly — but with the default options (4x + native-texrect quirk),
replaced HUD/menu art was being snapped back to native resolution, defeating the
replacement.

## Decision

`native_resolution_tex_rect` (native snap) stays the **default for unreplaced
content** — a correctness-preserving strategy, not a hack. It is narrowed only by
falsification-backed exemptions for replaced content:

1. **Replaced non-copy, non-flip texrects** are exempted per-draw (4f79390b): the
   snap bit is GPU-only and patched on the queued setup after the draw-time CRC
   fallback resolves, so the OFF path is untouched by construction.
2. **Replaced COPY-mode texrects** are lifted too (2026-06-11, 8dd25b02), with:
   the sub-native S phase (dx remainder) recovered for the replacement coordinate;
   a **plain ratio map** (`st * scale`) instead of the centered remap, because copy
   walks a texel footprint from its sample point and the centered remap straddles
   texel boundaries at strip seams; and, in the lift arm only, a fractional `yl`
   rounded up to the next whole native line (`(yl+3) & ~3`) because Paper Mario
   ends strips at fractional `yl` and the span clip dropped the last upscaled
   sub-rows. (Amended November 20, 2026.) The round-up is a tile-byte bit that
   only upscaled span setup honors; the queued `yl` stays the game's, because the
   native pass reads the same setup and writes RDRAM (ADR-0020). It applies to
   rects that were snapped or are copy mode; an unsnapped 1- or 2-cycle rect keeps
   its partial last line. See `rdp_hires_tex_rect_policy.hpp`.
3. **Flip texrects and unreplaced copy strips keep the snap.**

## Consequences

- Default options show hi-res texrect content (PM title diorama, SM64 PRESS START
  at replacement resolution) while the fragile-path protection stays.
- The copy-path plain-ratio mapping was re-falsified during the 2026-06-12
  star-family investigation and stands; that family's root cause was orig-dims
  (ADR-0014).
- The 1px vertical strip-junction seam (S-axis analog of the yl round-up class)
  was fixed 2026-06-12: COPY coverage natively includes the whole pixel
  containing `xl`, but at upscale the inclusive `end_x` covered only that
  pixel's first sub-column; `span_setup.comp` now completes the native pixel
  for unsnapped copy rects, capped by the scissor.

## Evidence

- `rdp_tex_rect_policy.hpp`; commits 4f79390b, 1f38a85e, 8dd25b02;
  artifacts/experiments/texrect-exempt-135246, copy-lift-111824.
