# ADR-0014: Replacement Original Dimensions Follow The Display View

- Status: Accepted
- Date: 2026-06-12

## Context

Replacement assets are authored against the rendering tile's displayed view,
not necessarily the upload window. Using upload dimensions squeezed or
misaddressed reshaped and mirrored content.

## Decision

Compute replacement original dimensions from the rendering
`SetTileSize` extent, clamped by coordinate masks. Rebase bound
replacement dimensions from the draw-time rendering tile before binding.

Upload identity remains unchanged. This is a mapping correction, not another
key candidate.

## Consequences

The rule fixes the damage-star, numeral, shell, and related display-view
families without scene overrides. The separate copy-strip junction seam was
subsequently fixed by the inclusive coverage rule in
[ADR-0011](0011-texrect-rasterization-policy.md); it is not open work.

## Evidence

- commit `76892a69`;
- `compute_hires_gliden64_display_dims`;
- draw-time dimension rebase tests and scene evidence.
