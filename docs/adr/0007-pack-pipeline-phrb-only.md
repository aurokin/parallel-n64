# ADR-0007: PHRB-Only Runtime Pack Pipeline

- Status: Accepted
- Date: ratified 2026-06-10; exact-variant sets 2026-06-11

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

## Consequences

Every title requires a conversion step. Evidence records the exact source and
generated package identity. Current source provenance and observed consumer
defaults are in [Texture Pack Sources](../../assets/TEXTURE_PACKS.md).

## Evidence

- `tools/hts2phrb.py` and converter contracts;
- exact-variant-set and cross-game conversion commits;
- runtime `phrb-only` assertions.
