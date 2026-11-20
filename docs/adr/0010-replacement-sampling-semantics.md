# ADR-0010: Replacement sampling: sub-texel fp5, direct sampling, CPU mips, trilinear

- Status: Accepted, amended (latest 2026-11-02)
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
   The October 11, 2026 correction applies the same opacity weighting between
   mip levels: plain interpolation of their straight RGB reintroduced dark
   fringes when mip opacities differed. Interpolated alpha stays linear. The
   shared production helper is exercised by `emu.unit.hires_mip_filter`;
   feature-off rasterizer SPIR-V remains byte-identical after regeneration.
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

7. **Replacement texels are served verbatim for every format** (2026-06-12). The
   first-light-era I-format flatten (RGB collapsed to intensity, alpha
   overwritten with it) was removed after the #29 semantics check: the reference
   pipeline uploads replacements verbatim and never recolors them — for I-format
   draws with tlut=1 it palette-qualifies the Rice key but falls back to the
   texture-CRC-only key, which is the only key form packs can carry for non-CI
   art — so packs author I repaints expecting their RGBA (especially the alpha
   mask) to be honored. The same check settled that NO tlut-gated serve rule is
   wanted: serving baked art on I+tlut draws (bypassing the native TLUT recolor)
   is the reference pipeline's own behavior, and content that cannot survive a
   single baked look (per-phase effect recolors, Rice collisions like the sewer
   slate) is pack-curation class, not a renderer gate.

   *Correction (2026-06-12, ADR-0018):* the "sewer slate collision" example was
   wrong — re-verification proved the entries are faithful repaints with no
   collision, and the sewer artifacts were a composition-semantics gap on our
   side, fixed by the cutout/alpha kill rules in ADR-0018. Verbatim serving
   itself stands unchanged.

   *Jabo import qualification (2026-11-02):* direct Jabo MD5 packages lack
   native-format metadata at conversion time. On a native I draw only, a
   source image whose alpha bytes are all at least 254 derives opacity as the
   integer average of RGB, retaining RGB. Any alpha below 254 preserves the
   entire image; IA and all other formats retain authored alpha. This matches the
   format-aware Rice import convention without applying it to already
   imported Rice/cache art. The provider decodes this Jabo-only variant at
   draw time, and the resident key includes native format/size so one MD5
   cannot reuse another format's alpha interpretation. No reference code is
   incorporated.

8. **Addressing happens in replacement-texel space** (2026-10-15). The sampler
   takes the tile-relative coordinate before the N64 clamp and mask, scales it
   to the replacement, and wraps, mirrors or clamps each bilinear tap with the
   replacement-scaled period (`shaders/hires_addressing.h`, exercised by
   `emu.unit.hires_addressing`). Clamped tiles extend to the end of the last
   native texel. Native-space addressing had clamped taps at the image edge at
   every wrap (SM64 Hazy Maze Cave wall blocks and floor seams) and pinned
   clamped strips to the last texel's origin, freezing the replacement rows
   inside it (Paper Mario title-logo banding). Point-sampled draws map native
   texel i's footprint [i, i + 1) to its replacement block; bilinear draws keep
   the integer-centered convention. The copy pipe keeps its own mapping.
   Feature-off output is unchanged.

   *Correction to rule 1 (2026-10-15):* the retained `/SCALING_FACTOR`
   normalization was a preprocessor `#if` on a GLSL constant and never
   compiled; non-copy replacement ST has always been native-domain. The dead
   block was removed with no behavior change.

   *Point reconstruction phase (2026-11-02):* non-copy point draws evaluate
   replacement ST at the output pixel's center. Feeding the N64 upper-left
   sample directly into continuous filtering mixes opposite edges of wrapping
   replacement tiles (Big Mountain 2000's title logo). General triangles offset
   S, T and W by half their signed screen derivatives before perspective divide.

   Texture rectangles additionally rebase the authored, tile-relative origin
   to the selected native texel's low edge for positive traversal, or its high
   edge for negative traversal. With origin q0, signed native-pixel rate d and
   effective output scale N, reconstruction is q(j) - fract(q0) +
   (d < 0 ? 1 : 0) + d/(2N). This retains both integer and fractional flipped
   origins and handles positive half-texel starts. For a negative unit-rate
   rectangle starting at 31, the correction is +1/2 at 1x and +7/8 at 4x;
   a literal absolute half-step at 4x would still fetch across the opposite edge.
   Origin phase is removed after each sampled tile's shift and origin
   subtraction. Constant axes keep their phase. Swapped-axis rectangles retain
   their existing native-pixel snapping, using N=1 for the reconstruction center.

   This is continuous replacement reconstruction, not exact native floor
   sampling at every fractional texture rate; authored subtexel scrolling can
   change phase. General triangles retain signed centering and do not receive
   rectangle origin rebasing. Native point sampling, bilinear draws, copy draws,
   addressing periods and lookup remain unchanged. Only replacement rasterizer
   SPIR-V changes.

Replaced draws thereby diverge deliberately from N64 filter semantics — sanctioned,
because direct sampling is the point of replacement. Feature-off is provably inert
for every rule (digest checks, ADR-0002).

Supporting toolchain decision: shader regeneration uses the slangmosh that matches
the vendored parallel-rdp snapshot (other vintages emit an incompatible interface;
for `1cecd042` this is Granite `b8cc8335` built without the SPIR-V optimizer, for
`65f4f4da` it was Granite `194abc80`; see
`tools/parallel-rdp-toolchain-patches/`); `tools/regen-parallel-rdp-shaders.sh` rewrites the
Granite include path at run time. This is what makes shader-level fixes and probes
possible without modernizing the Vulkan backend.

## Consequences

- Minification quality now depends on the CPU mip chain.
- Later sampling work builds on replacement-texel space; the combine reduction is
  the template for future "replaced draws diverge deliberately" decisions.
- Forced shader regenerations are byte-identical to the committed header, and
  a mutation canary proved the pinned pipeline end-to-end.

## Evidence

- Commits cb46b7d2, 3da623f5, e647e357, 9c0d6ae3, a020212b;
  artifacts/experiments/sampler-falsification-062900, mips-trilinear-195816,
  trilinear-default-204024.
