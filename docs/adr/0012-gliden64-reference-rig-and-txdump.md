# ADR-0012: GlideN64 reference rig; txDump output is the MISS set

- Status: Accepted
- Date: 2026-06-10 (rig); 2026-06-12 (txDump semantics correction)

## Context

Content judgment (ADR-0005 use 3) needs side-by-side captures, and coverage
analysis (use 2) needs GlideN64's per-scene identities — without reintroducing
Attempt A's pixel-diff failure mode or invasively forking the reference core.

## Decision

1. **Bounded reference rig on the mupen64plus-next vehicle** (1eac0dbd): GlideN64
   reference captures boot the `.hts` pack on the mupen64plus-next core, driven by
   the same adapter (generic `--core-options-template` / `--extra-append-config`);
   `compose-reference-pair.py` builds labeled review pairs; the guardrail gate
   bans image metrics from the test surface.
   Cross-core savestate loading is impossible (mupen64plus-next segfaults on our
   m64p states), so glide-side scenes are reached by natural boot/play — an
   accepted timeline-drift cost in beat matching.
2. **txDump oracle as an env-gated one-line patch** (5f7ea3cc): `GLN64_TXDUMP=1`
   in the local mupen64plus-libretro-nx clone dumps Rice-CRC-named PNGs; patch and
   rebuild recipe in `tools/gliden64-txdump-patch/`.
3. **Binding interpretation rule (2026-06-12 correction): the txDump dump set is
   GlideN64's MISS set, not its drawn-identity set.** The hires replacement
   attempt returns before the dump call site, so "GlideN64 dumped the same
   identity" means GlideN64 *also missed it* and says nothing about what it
   served. Corollaries:
   - With-pack dump = GlideN64's miss census.
   - Pack-less dump = computed-key ground truth (every cached load dumped with
     GlideN64's (tcrc, pcrc)).
   - Pack-gap claims require the flipped join or side-by-side confirmation.
   An earlier three-way "pack gap" verdict built on the opposite reading was
   retracted; the user's observation that the same beats rendered replaced on
   GlideN64 exposed the misreading, confirmed by a flipped-join proof.

## Consequences

- Side-by-side review pairs are standard evidence.
- The corrected txDump semantics unlocked the real keying root causes (TLUT base
  convention, bank-0 palette — ADR-0013; orig-dims — ADR-0014).
- All coverage analysis scripts must state which dump mode they consumed.

## Evidence

- Commits 1eac0dbd, 5f7ea3cc; PROJECT_NOTES.md 2026-06-12 CORRECTION entry;
  gameplay-campaign-011041/gliden64-montage-txdump and glide-vignette-txdump.
