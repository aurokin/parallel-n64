# ADR-0018: Replaced-draw composition follows the HLE pack contract (cutout kill, alpha kill, view-keyed serving)

- Status: Accepted
- Date: 2026-06-13

## Context

The sewer/stage attract artifacts (#34: dark boxed silhouettes around
characters, large black ovals/blob on walls, persistent white boxes around
characters) were originally classified as a pack-content collision and
queued for curation (ADR-0015 lane). Re-verification ordered by the user
overturned that story end to end:

- The pack entries are faithful repaints, not foreign collisions. Rendering
  the native RDRAM windows (I4 views) proved d7f736aa is the slate panels
  and 18b5e3be's repaint is a white field with a hard transparent-black
  disc.
- GlideN64 with the same .hts serves all five effect-family checksums
  (txdump miss-set proof, ADR-0012 rig) and composes the same scene clean.
- The artifacts reproduce at 1x (not an upscale mapping bug) and are
  insensitive to texel alpha policy (flatten vs verbatim A/B) and to texel
  RGB (tracer-pack recolors) at the affected pixels.
- Combine forensics (the per-draw "Hi-res draw combine" telemetry added for
  this hunt) decoded two distinct mechanisms:

**Dark boxes / black ovals.** Wall-redraw quads: 2-cycle
`RGB = T0 * T1 * SHADE`, final blend `P*A_in + M*A_mem` with
**force_blend off**, AA on. With force_blend off the faithful blender
writes interior (full-coverage) pixels **unblended** — pixel alpha is
irrelevant there. These quads are invisible natively only because their
texel products color-match the background. HLE renderers (GlideN64, Rice)
have no coverage pipeline and apply GL src-alpha blending to every pixel,
so a pack texel with alpha == 0 simply vanishes there. Packs are authored
and tested against that behavior: alpha == 0 means "nothing here", full
stop. Serving such texels verbatim into a faithful blender materializes
them as opaque black (packs store black RGB under transparent texels).
Substituting the native texel per-pixel (tried first) repairs
background-redraw quads but cannot repair quads whose product must match a
background composed from *different* pack texels — falsified by the
then-persisting white box.

**White boxes.** The character flash-stencil draws (2-cycle, tlut=1,
T0 = the CI sprite replacement, T1 = an effect-mask buffer, combiner alpha
= **T1 alpha only**, CVG_TIMES_ALPHA) read T1 as a 32x56 view of a window
whose checksum key covers only 16x32 (512 bytes). The "texture" is a
dynamic effect-compositing buffer; the rows past the keyed window are
other live buffers, and the stencil alpha is per-frame game content. We
served the static 128x128 repaint stretched over the 56-row view,
replacing the dynamic stencil alpha with the repaint's opaque white field
— materializing the flash quad as a persistent white box around each
character (94-frame paired review: white boxes in 36 parallel frames, zero
in 97 glide frames, including a programmatic bright-cluster scan).
GlideN64 keys its draw-time lookup over the rendering view, so its CRC
over the 56-row window is a different key, misses the pack, and serves
native — that is the reference pipeline's clean mechanism, and the same
keying-convention family as ADR-0013/0014.

## Decision

Three class-level rules, all hi-res-gated (feature-off provably inert,
ADR-0002), no per-scene or per-descriptor overrides (ADR-0005):

1. **Cutout kill** (shaders/texture.h + shading.h): if a sampled
   replacement texel has filtered alpha == 0, the pixel is not written at
   all (`hires_replacement_cutout` -> shade_pixel returns false). The
   alpha-weighted RGB filter (ADR-0010 rule 2) guarantees filtered alpha
   is 0 iff every contributing tap is transparent, so the kill region is
   exactly the authored cutout, with clean filtered edges outside it.
   Copy-pipe transport keeps raw alpha-bit semantics and is exempt.
   This fixed the dark-box/black-oval/blob family.

2. **HLE alpha kill** (RASTERIZATION_HIRES_ALPHA_KILL_BIT): draws that
   sample a replaced texture and whose final blend cycle is a
   src-alpha-over-memory closure (`1b == A_in`, `2a == MEM`,
   `2b == A_mem | 1-A_in`) kill pixels whose combined alpha is < 8
   (threshold absorbs alpha dither; an HLE renderer leaves such pixels
   under ~3% contribution). The eligibility bit is computed CPU-side at
   draw enqueue — where replacement hit state and depth-blend state meet —
   and set on the normalized static raster state (triangle lane only).
   No firing case exists in the PM64 attract family (its draws route
   alpha through coverage), so this rule is a contract-completion guard
   for alpha-choreographed replaced content; it survives the full gate
   and probe battery without visible effect.

3. **View-keyed serving** (rdp_renderer draw-time rebase): a rendering
   tile view whose RDRAM byte extent exceeds the bound replacement's
   keyed window is served native for that draw
   ("Hi-res view exceeds keyed window" telemetry). The reference pipeline
   could never serve it (its view-keyed CRC is a different key), and the
   replacement provably does not cover the view. Draw-time compat hits
   key over the view itself and always pass; in-window sub-views
   (atlases) keep serving; the binding re-applies on the next in-window
   view. This fixed the white-box family.

## Consequences

- The attract dark-box/black-oval/blob and white-box families are fixed
  (94-frame paired attract-segment review against GlideN64, iflat-probe
  A-J evidence chain in artifacts/experiments/iflat-probe-202533).
- ADR-0010 rule 7's closing claim — that content like "the sewer slate"
  collision is pack-curation class — is **corrected**: there was no
  collision and nothing to curate. Verbatim serving (rule 7) stands; what
  was missing was composition semantics and view-keyed serving, recorded
  here.
- Dynamic effect-compositing buffers (windows that the game rewrites
  per frame and views across keyed-window boundaries) are now structurally
  excluded from mis-serving by rule 3 without any content denylist.
- Checksum-shaped evidence got a worked counterexample: the "dump set =
  miss set" oracle proved glide *served* the entries, but every inference
  about *why* glide looked clean required combine-level forensics; the
  final mechanism (draw-time view keying) was invisible to checksum
  reasoning alone.
