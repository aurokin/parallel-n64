# ADR-0014: Replacement orig dims follow GlideN64's display view

- Status: Accepted
- Date: 2026-06-12

## Context

The "star-shape family" (battle damage-star upper half rendering as an opaque
slab, squeezed HD numerals, kmr_03 sky seam) was first framed as a copy-path
geometry bug. Falsification overturned that: the damage star is thirty-six 2-cycle
*triangles* per frame sampling a 16x32 I4 atlas, not texrects. The actual defect:
the upload lane stored the LOAD-window dimensions as the replacement's original
dims, while hi-res packs are authored against GlideN64's texture-cache view —
the RENDERING tile's SetTileSize extent clamped by `mask_s`/`mask_t` (also the
window its Rice CRC hashes). For reshaped/mirrored loads (8x16 load vs 16x16
display, 16x64 vs 32x64, 16x32 vs 32x32) the GPU ratio map skewed: numerals
squeezed, the star's upper half sampled from interior rows into an opaque slab.
Pack asset dimensions scale to the glide dims (80x80 = 5x of 16x16; 307x614 ≈ 9.6x
of 32x64; 256x256 = 8x of 32x32), never to the load window.

## Decision

Replacement original dimensions follow GlideN64's display view. Implemented
(76892a69) as:

- `compute_hires_gliden64_display_dims` — SetTileSize extent clamped by the
  coordinate masks — used for the compat lane's stored dims, and
- a draw-time rebase in `draw_shaded_primitive` rewriting `orig_w/orig_h` of every
  bound texel tile from the rendering tile's registers before binding.

Upload-lane key/identity is untouched (this is a dims fix, not an identity
change); copy strips are identical by construction (render window == load window).

## Consequences

- Fixed the damage star (5-point silhouette + HD numeral matching the GlideN64
  reference), the Bowser shell beat, and the gray-blob beat without touching
  identity.
- Accepted: sub-texel resample shifts on previously-working scenes (kmr_03 0.5%
  pixel delta, content-equivalent; toad-town sub-texel shifts only).
- Remaining family member: the 1px vertical strip-junction seam in vignette skies
  (present before and after the fix, absent OFF) — shader-level, queued behind
  regen verification (ADR-0011 consequences).
- SM64 copy-pipe flagged for a breadth re-check after the rebase.

## Evidence

- Commit 76892a69; gameplay-campaign-011041/star-fix/verify (battle-damage-fixed,
  star-fix-3way, kmr03-on-vs-glide) and user-beats-dimsfix bundles; gates 43/43
  emu-required + 2/2 runtime-conformance post-fix.
