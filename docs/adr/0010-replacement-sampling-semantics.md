# ADR-0010: Replacement sampling: sub-texel fp5, direct sampling, CPU mips, trilinear

- Status: Accepted
- Date: 2026-06-10 (sequence of commits, each falsification-backed)

## Context

With packs loading and binding correctly, replacement output still looked wrong:
detail decimated to the native texel grid, dark halos on cutout edges, stipple on
fences and speckle on stone paths, and shimmer under minification. Each symptom was
isolated by experiment before its fix (ADR-0004 methodology).

## Decision

Replacement sampling operates in replacement-texel space, with these rules:

1. **Preserve the fp5 sub-texel fraction** (commit cb46b7d2). The sampler
   falsification experiment showed the fraction was discarded before replacement
   sampling — the single real bug between "packs load" and "packs look hi-res".
   The suspected `/SCALING_FACTOR` wrong-region bug was falsified; that
   normalization is correct and retained.
2. **Alpha-weighted RGB filtering** (3da623f5): bilinear taps (and mip reduction)
   weight RGB by alpha because packs store black RGB under transparent texels
   (267/268 sampled cutout textures in the PM pack). Fixed cutout halos.
3. **Replaced tiles are truly direct-sampled** (e647e357): the N64 3-point combine
   is reduced to `t_base` on replaced draws (frac/sum_frac zeroed, TLUT draws
   included) so hi-res taps are not re-blended at native-texel spacing. This was
   the real cause of fence stipple and stone speckle — vindicating the early
   research warning that N64 3-point filtering must not be composed with modern
   sampling on enhanced paths.
4. **Full CPU-built mip chains** at replacement upload (alpha-weighted 2x2 reduce,
   VRAM-budgeted), with `hirestex-filter` (nearest/bilinear/trilinear) wired
   end-to-end; trilinear LOD from per-pixel ST screen derivatives on replaced
   draws only (e647e357).
5. **`hirestex-filter` defaults to trilinear** (9c0d6ae3) after a 5/5 adversarial
   panel across four breadth titles plus kmr_03 — bilinear-only speckle on
   minified content counted as evidence for the flip.
6. **Pack art renders as UNORM**; the dead `hirestex-srgb` option was deleted
   (a020212b) — sRGB-authored art rendered as UNORM matches GlideN64's behavior.

Replaced draws thereby diverge deliberately from N64 filter semantics — sanctioned,
because direct sampling is the point of replacement. Feature-off is provably inert
for every rule (digest checks, ADR-0002).

Supporting toolchain decision: shader regeneration uses a 2020-vintage slangmosh
built from the fork's pinned parallel-rdp upstream snapshot (modern slangmosh emits
an incompatible interface); `tools/regen-parallel-rdp-shaders.sh` rewrites the
Granite include path at run time. This is what makes shader-level fixes and probes
possible without modernizing the Vulkan backend.

## Consequences

- Minification quality now depends on the CPU mip chain.
- Later sampling work builds on replacement-texel space; the combine reduction is
  the template for future "replaced draws diverge deliberately" decisions.
- Full regen-script verification remains Reboot Plan step 2's residual gate for
  new shader probes (strip-seam work is queued behind it).

## Evidence

- Commits cb46b7d2, 3da623f5, e647e357, 9c0d6ae3, a020212b;
  artifacts/experiments/sampler-falsification-062900, mips-trilinear-195816,
  trilinear-default-204024.
