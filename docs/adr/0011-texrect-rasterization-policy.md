# ADR-0011: Texrect policy: native snap by default, evidence-narrowed exemptions

- Status: Accepted
- Date: pre-reboot (default); exemptions 2026-06-10 and 2026-06-11

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
   texel boundaries at strip seams; and, in the lift arm only, queued `yl` rounded
   up to the next whole native line (`(yl+3) & ~3`) because Paper Mario ends strips
   at fractional `yl` and the span clip dropped the last upscaled sub-rows.
3. **Flip texrects and unreplaced copy strips keep the snap.**

## Consequences

- Default options show hi-res texrect content (PM title diorama, SM64 PRESS START
  at replacement resolution) while the fragile-path protection stays.
- The copy-path plain-ratio mapping was re-falsified during the 2026-06-12
  star-family investigation and stands; that family's root cause was orig-dims
  (ADR-0014).
- Known remaining family member: the 1px vertical strip-junction seam in vignette
  skies (S-axis analog of the yl round-up class), queued behind shader-regen
  verification.

## Evidence

- `rdp_tex_rect_policy.hpp`; commits 4f79390b, 1f38a85e, 8dd25b02;
  artifacts/experiments/texrect-exempt-135246, copy-lift-111824.
