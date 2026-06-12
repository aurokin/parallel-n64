# Reboot Plan (2026-06-10)

This is the controlling plan for the parallel-ish effort after the 2026-06-10 reboot.
It replaces the Attempt B plan stack now archived under [docs/history/](/home/auro/code/parallel-n64/docs/history).
Durable decisions made under this plan are recorded in
[docs/adr/](/home/auro/code/parallel-n64/docs/adr/README.md); the running narrative is
[PROJECT_NOTES.md](/home/auro/code/parallel-n64/PROJECT_NOTES.md).

## Goal

Add GlideN64-style hi-res texture pack support and good scaling to the ParaLLEl-RDP
Vulkan renderer in this libretro core, stable enough for real gameplay, validated by
what is on screen rather than by metadata accounting.

## Protected Property

With hi-res and scaling features OFF, the core must remain upstream-grade stable.

- Feature-off parity and savestate remint authority verification are the only
  places where screenshot digest equality is a valid gate.
- Hi-res-ON validation must NEVER use pixel digests or exact metadata counts.
  It uses class-level semantics (entry/draw-hit presence, source-mode class, explicit
  fallback reasons) plus human visual review.

See [ADR-0002](/home/auro/code/parallel-n64/docs/adr/0002-protected-feature-off-property.md)
and [ADR-0004](/home/auro/code/parallel-n64/docs/adr/0004-hires-on-validation-methodology.md).

## Identity Decision

- The GlideN64-compat Rice-CRC lane is the PRIMARY identity path. It is proven
  cross-game: SM64, OoT (43K entries, 8.9GB streaming), and Paper Mario (11,463
  compat entries on the title fixture with the current zero-config package).
- The native-sampled-identity/enrichment program from Attempt B is FROZEN: the code
  stays dormant, no gates depend on it, and no further converter-review work happens.
- Runtime pack format is `.phrb` only; legacy `.hts` caches are offline `hts2phrb`
  conversion sources (exact-variant-set promotion per
  [ADR-0007](/home/auro/code/parallel-n64/docs/adr/0007-pack-pipeline-phrb-only.md)).

See [ADR-0006](/home/auro/code/parallel-n64/docs/adr/0006-replacement-identity.md).

## GlideN64 Guardrails

GlideN64 (via mupen64plus-next) may be used only as:

1. A source-level semantic oracle — read its code to understand intended behavior.
2. A hash-coverage oracle — txDump Rice-named dumps set-intersected with pack entries
   and parallel hit/miss logs. NOTE: the dump set is GlideN64's MISS set
   ([ADR-0012](/home/auro/code/parallel-n64/docs/adr/0012-gliden64-reference-rig-and-txdump.md)).
3. Side-by-side screenshots for CONTENT judgment ("is the right art displayed?") as
   disposable evidence-bundle context.

Hard bans (these killed Attempt A):

- No numeric image-similarity metric against GlideN64 output anywhere in tests or
  pass/fail logic.
- No renderer commit justified by "match GlideN64".
- No per-scene or per-descriptor renderer overrides.

See [ADR-0005](/home/auro/code/parallel-n64/docs/adr/0005-gliden64-oracle-policy.md).

## Work Order (with status)

1. **Remint the savestate ladder** (title screen -> file select -> `kmr_03 ENTRY_5`)
   on this machine and verify the live control stack.
   **DONE 2026-06-10.** Ladder reminted and promoted, deterministic across
   independent sessions, canonical feature-off digests recorded
   (PROJECT_NOTES "Control stack live" entry;
   [ADR-0008](/home/auro/code/parallel-n64/docs/adr/0008-fixture-and-evidence-authority.md)).
2. **Verify shader regeneration**: `tools/regen-parallel-rdp-shaders.sh` must
   regenerate `slangmosh.hpp` reproducibly. This gates all shader work.
   **PARTIAL.** The toolchain is proven: modern slangmosh emits an incompatible
   interface, so a 2020-vintage slangmosh built from the fork's pinned parallel-rdp
   upstream snapshot is used, with the Granite include path rewritten at run time;
   regens already shipped the copy-pipe replacement-sampling fix (1f38a85e).
   Full reproducible-regeneration verification is still open and gates new shader
   probes (the strip-junction seam).
3. **1x-vs-4x sampler falsification experiment** before any shader edit.
   **DONE 2026-06-10** (artifacts/experiments/sampler-falsification-062900): the
   suspected `st_fp5 /= SCALING_FACTOR` wrong-region bug was FALSIFIED (ST is
   scaled-space; the division is correct); the sub-texel fraction discard was
   CONFIRMED and fixed in cb46b7d2.
4. **Sampler fix as dictated by step 3**, plus mipmaps, wiring the
   `hirestex-filter` libretro option, and exempting hires-replaced TEX_RECTs from
   `native_resolution_tex_rect`.
   **DONE 2026-06-10/11.** fp5 preservation (cb46b7d2); direct sampling + CPU mips
   + `hirestex-filter` wired (e647e357), defaulting to trilinear (9c0d6ae3);
   texrect exemption (4f79390b) extended to replaced copy rects (8dd25b02).
   See [ADR-0010](/home/auro/code/parallel-n64/docs/adr/0010-replacement-sampling-semantics.md)
   and [ADR-0011](/home/auro/code/parallel-n64/docs/adr/0011-texrect-rasterization-policy.md).
5. **Bounded GlideN64 reference rig** within the guardrails above.
   **DONE 2026-06-10** (1eac0dbd rig + guardrail gate; 5f7ea3cc env-gated txDump
   patch); in live oracle use since 2026-06-12.
6. **Interactive agent-play adapter mode**: per-bundle stdin FIFO session,
   no daemons, no sockets.
   **DONE 2026-06-10** (a7fbf4ad); in production use for all beat/campaign work
   ([ADR-0009](/home/auro/code/parallel-n64/docs/adr/0009-interactive-adapter-and-deterministic-play.md)).
7. **Breadth**: SM64/OoT/MK64/MM packs.
   **ACTIVE.** All packs acquired, converted to `.phrb`, and boot-validated
   (83d0dfad, e0f35000, be119ac7; verdicts in
   [assets/TEXTURE_PACKS.md](/home/auro/code/parallel-n64/assets/TEXTURE_PACKS.md)).
   The step is now ongoing breadth validation: compat-CRC hit traffic plus visual
   review on each game's entry scenes, and breadth re-checks after renderer changes
   (SM64 copy-pipe is flagged for one after the orig-dims rebase).

## Current Frontier (as of 2026-06-12)

The work order above is substantially complete; active work is correctness
refinement on the compat lane plus curation triage:

- GlideN64-compat keying conformance: TLUT base shadow + bank-0 palette candidate
  ([ADR-0013](/home/auro/code/parallel-n64/docs/adr/0013-compat-keying-refinements.md)),
  orig-dims display-view rebase
  ([ADR-0014](/home/auro/code/parallel-n64/docs/adr/0014-orig-dims-display-view-rebase.md)).
- Glide-vs-parallel beat comparison as the standing validation methodology: a
  "beat" is a user-flagged in-game moment pinned in a savestate slot at sight
  during a live run; each beat is captured on both renderers and judged via
  paired captures and adversarial panels.
- Pack-curation lane for content reclassified out of renderer scope
  ([ADR-0015](/home/auro/code/parallel-n64/docs/adr/0015-pack-content-curation-boundary.md)).
- Open items: the 1px vertical strip-junction seam (behind step 2's regen gate);
  the I-format tlut=1 recolor sampler-semantics check; the debug-flood session
  wedge forensics (soft session hangs under `PARALLEL_RDP_HIRES_DEBUG` log flood —
  see the PROJECT_NOTES 2026-06-12 star-family entry's session tooling notes).

## Success Criteria Style

- **Visual review rubric** for hi-res-ON acceptance: paired before/after captures and
  an explicit judgment per scene — yes / no / wrong-texture / wrong-region.
- **Class-level semantics** for runtime gates: `entry_count > 0`, `draw_hits > 0`,
  expected source-mode class, explicit fallback reasons. Never exact counts.
- **Named scenes approved at 4x**: a milestone is done when its named scenes pass the
  rubric at 4x internal scale, not when a counter reaches a number.

## Validation Scope

Paper Mario is the strict validation title until the first major milestone is stable.
SM64/OoT/MK64/MM are compat-path breadth checks. The test surface is described in
[docs/EMU_TESTING.md](/home/auro/code/parallel-n64/docs/EMU_TESTING.md); local paths in
[docs/WORKSPACE_PATHS.md](/home/auro/code/parallel-n64/docs/WORKSPACE_PATHS.md).
