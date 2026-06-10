# Reboot Plan (2026-06-10)

This is the controlling plan for the parallel-ish effort after the 2026-06-10 reboot.
It replaces the Attempt B plan stack now archived under [docs/history/](/home/auro/code/parallel-n64/docs/history).

## Goal

Add GlideN64-style hi-res texture pack support and good scaling to the ParaLLEl-RDP
Vulkan renderer in this libretro core, stable enough for real gameplay, validated by
what is on screen rather than by metadata accounting.

## Protected Property

With hi-res and scaling features OFF, the core must remain upstream-grade stable.

- Feature-off parity is the one place where screenshot digest equality is a valid gate.
- Hi-res-ON validation must NEVER use pixel digests or exact metadata counts.
  It uses class-level semantics (entry/draw-hit presence, source-mode class, explicit
  fallback reasons) plus human visual review.

## Identity Decision

- The GlideN64-compat Rice-CRC lane is the PRIMARY identity path. It is proven
  cross-game: SM64 (2,530 entries), OoT (43K entries, 8.9GB streaming), Paper Mario
  (8,620 compat records).
- The native-sampled-identity/enrichment program from Attempt B is FROZEN: the code
  stays dormant, no gates depend on it, and no further converter-review work happens.

## GlideN64 Guardrails

GlideN64 (via mupen64plus-next) may be used only as:

1. A source-level semantic oracle — read its code to understand intended behavior.
2. A hash-coverage oracle — txDump Rice-named dumps set-intersected with pack entries
   and parallel hit/miss logs.
3. Side-by-side screenshots for CONTENT judgment ("is the right art displayed?") as
   disposable evidence-bundle context.

Hard bans (these killed Attempt A):

- No numeric image-similarity metric against GlideN64 output anywhere in tests or
  pass/fail logic.
- No renderer commit justified by "match GlideN64".
- No per-scene or per-descriptor renderer overrides.

## Work Order

1. **Remint the savestate ladder** (title screen -> file select -> `kmr_03 ENTRY_5`)
   on this machine and verify the live control stack.
   Verify: each remint scenario promotes an authoritative state; the three Paper Mario
   fixture scenarios run green from those states end to end.
2. **Verify shader regeneration**: `tools/regen-parallel-rdp-shaders.sh` must
   regenerate `slangmosh.hpp` reproducibly. This gates all shader work.
   Verify: regenerated output builds and feature-off behavior is unchanged.
3. **1x-vs-4x sampler falsification experiment.** Static analysis found two suspected
   bugs in `parallel-rdp/shaders/texture.h` (sub-texel fraction discarded before
   replacement sampling; `st_fp5 /= SCALING_FACTOR` possibly sampling a 4x-compressed
   region at 4x), but recorded history (hi-res-ON captures byte-identical to OFF)
   partially contradicts this. Run real pack art plus a labeled minipack
   (`tools/hires_minipack.py`) at 1x and 4x, texrect-native on and off, BEFORE any
   shader edit.
   Verify: the experiment confirms or falsifies each suspected bug with labeled
   visual evidence at both scales.
4. **Sampler fix as dictated by step 3**, plus mipmaps, wiring the dead
   `hirestex-filter` libretro option, and exempting hires-replaced TEX_RECTs from
   `native_resolution_tex_rect`.
   Verify: visual review rubric passes on the named Paper Mario scenes at 4x;
   feature-off parity digests unchanged.
5. **Bounded GlideN64 reference rig** (mupen64plus-next vehicle, txDump coverage
   oracle, side-by-side capture helper) within the guardrails above.
   Verify: produces hash-coverage intersections and paired screenshots; no
   similarity metrics anywhere.
6. **Interactive agent-play adapter mode**: extend the per-bundle stdin FIFO session
   for interactive play. No daemons, no sockets.
   Verify: an agent can pause/step/input/capture interactively within one bundle
   session and the session leaves a normal evidence bundle.
7. **Breadth**: SM64 and OoT packs once acquired (see
   [assets/TEXTURE_PACKS.md](/home/auro/code/parallel-n64/assets/TEXTURE_PACKS.md)).
   Verify: compat-CRC hit traffic plus visual review on each game's entry scenes.

## Success Criteria Style

- **Visual review rubric** for hi-res-ON acceptance: paired before/after captures and
  an explicit judgment per scene — yes / no / wrong-texture / wrong-region.
- **Class-level semantics** for runtime gates: `entry_count > 0`, `draw_hits > 0`,
  expected source-mode class, explicit fallback reasons. Never exact counts.
- **Named scenes approved at 4x**: a milestone is done when its named scenes pass the
  rubric at 4x internal scale, not when a counter reaches a number.

## Validation Scope

Paper Mario is the strict validation title until the first major milestone is stable.
SM64/OoT are compat-path breadth checks. The test surface is described in
[docs/EMU_TESTING.md](/home/auro/code/parallel-n64/docs/EMU_TESTING.md); local paths in
[docs/WORKSPACE_PATHS.md](/home/auro/code/parallel-n64/docs/WORKSPACE_PATHS.md).
