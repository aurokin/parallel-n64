# ADR-0007: Pack pipeline: PHRB-only runtime, offline conversion, exact-variant sets

- Status: Accepted
- Date: format established in Attempt B; ratified 2026-06-10; exact-variant sets 2026-06-11

## Context

Legacy GLideNHQ caches (`.hts`/`.htc`) come in heterogeneous layouts; parsing them
at runtime is fragile, and pack policy (variant promotion, deferral, budgets) needs
review artifacts. Pack sources in the wild also include non-GLideNHQ formats that
carry the wrong identities for our lane.

## Decision

1. **The runtime loads `.phrb` packages exclusively.** Legacy `.hts` caches are
   conversion *sources*, processed offline by `hts2phrb` (both old- and new-version
   `.hts` layouts parsed by `hires_pack_common`).
2. **Acceptable pack sources are GlideN64/GLideNHQ caches only** — they carry the
   Rice-CRC identities the compat lane keys against (ADR-0006). Rejected with
   recorded evidence: sm64nx `.pak` ports, Citra packs, SoH `.o2r`.
3. **Multi-variant exact families are promoted as exact-variant sets**
   (2026-06-11, commit e0d59c63): the converter emits them as exact-only authority
   instead of parking them `runtime_ready=false`. Family-level low32 serving stays
   env-gated off; exact 64-bit keys disambiguate by construction.

## Consequences

- Pack-policy fixes are converter changes plus reconversion with review artifacts —
  not runtime changes.
- Every title needs a conversion step; validated for all five titles (PM, SM64,
  OoT, MK64, MM) with boot-validated zero-config packages; the 9.5GB OoT and 3.9GB
  MM packs double as streaming-load stress tests.
- The exact-variant-set promotion unblocked 649 parked Paper Mario families:
  file-select textured-draw misses fell 114 → 1, kmr_03 57 → 13; the canonical PM
  package is `artifacts/hts2phrb-review/local-pm64-exact-variant-set/package.phrb`.
- Tracking lives in [assets/TEXTURE_PACKS.md](/home/auro/code/parallel-n64/assets/TEXTURE_PACKS.md).

## Evidence

- `tools/hires_pack_migrate.py` (hts2phrb); commits e0d59c63, 83d0dfad, e0f35000,
  be119ac7; PROJECT_NOTES.md 2026-06-10 breadth entries and 2026-06-11
  exact-variant-set entry.
