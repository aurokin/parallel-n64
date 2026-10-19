# ADR-0007: PHRB-Only Runtime Pack Pipeline

- Status: Accepted, amended
- Date: ratified 2026-06-10; exact-variant sets 2026-06-11; Jabo MD5 sources 2026-10-19

## Context

Legacy GlideN64 caches vary in layout, while runtime parsing and implicit family
fallback make replacement behavior hard to review.

## Decision

1. Runtime loads `.phrb` packages only.
2. `.hts` and `.htc` are offline conversion sources.
3. Accept sources carrying GlideN64/Rice identities; unrelated port or
   console-specific pack formats are not compatible.
4. Multi-variant families may be emitted as exact-only sets. Family low-32
   serving remains opt-in.

Pack-policy changes happen in conversion and curation, not as scene-specific
runtime rules.

*Amendment (2026-10-19):* decision 3 also accepts Project64/Jabo packs, whose
textures are named by the MD5 of the draw view as Jabo uploaded it to
Direct3D. The key conversion was derived offline from ROM data against real
pack names and is reproduced bug-for-bug, under the strict-identity rule of
[ADR-0006](0006-replacement-identity.md). PHRB version 8 adds a key kind and
a 16-byte key to each record; converters write it only for packages holding
Jabo records, so Rice-only packages stay byte-identical version 7 and older
cores reject Jabo packages by version. The draw-time compat lane computes the
Jabo key only after its Rice key misses and only when Jabo records are loaded.

## Consequences

Every title requires a conversion step. Evidence records the exact source and
generated package identity. Current source provenance and observed consumer
defaults are in [Texture Pack Sources](../../assets/TEXTURE_PACKS.md).

## Evidence

- `tools/hts2phrb.py` and converter contracts;
- exact-variant-set and cross-game conversion commits;
- runtime `phrb-only` assertions.
