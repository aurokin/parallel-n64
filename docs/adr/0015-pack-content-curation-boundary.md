# ADR-0015: Pack-content defects are curation; three-class miss taxonomy

- Status: Accepted
- Date: 2026-06-11 (taxonomy); 2026-06-12 (equivalent-to-glide verdict)

## Context

Hi-res-ON divergences kept arriving mixed: some were renderer bugs, some were the
pack's own art or its internal key collisions, and some were noise from the
non-authoritative lookup lane. Undifferentiated miss counts misled (the cross-scene census over
title/file-select/kmr_03 logs: 15 family-variant keys + 59 absent Nx1 strips +
1 zero-key fill), and
dims-based "wrong art" guards were found impossible — legitimate pack sprites have
arbitrary non-integer scales.

## Decision

1. **Replacement misses are classified three ways:**
   - *family-variant miss* — the pack has the family, not the variant: keying or
     converter work;
   - *true pack gap* — e.g. pcrc=0 Nx1 block-strip loads: explicit fallback is
     correct behavior, pack-side work if ever;
   - *upload-lane noise* — split views (CI8-upload/CI4-draw) where the draw-time
     compat lane is the authoritative resolver and upload-lane miss lines are
     expected. Draw-time compat misses log attempted keys so censuses run on the
     authoritative lane.
2. **When GlideN64 serves the same wrong/divergent art from the same pack entries,
   the defect is pack-content (curation), out of renderer scope** — and
   "equivalent-to-glide" is a valid closing verdict. Established on: the sewer
   white-quad (a 65x65 slate entry Rice-colliding with the sewer beam inside the
   pack's own old-version keying) and the vignette sky-backdrop clouds (pack
   strips structurally diverge from native art; GlideN64 shows identical clouds).
3. **Remedies for pack-content defects** are the
   `PARALLEL_RDP_HIRES_FILTER_SIGNATURES` opt-out (proven on the sewer scene) and
   pack-side curation — never renderer special-casing (per-scene override ban,
   ADR-0005).

## Consequences

- "Absent key with explicit fallback" is a pass, not a bug; upload-lane noise is
  not chased.
- Open follow-up (tracked, not decided): I-format `tlut=1` draws natively recolor
  through the TLUT, which baked-RGBA serving bypasses — a sampler-semantics check
  is queued before any tlut-gated serve rule.

## Evidence

- PROJECT_NOTES.md 2026-06-11 missing-texture forensics; 2026-06-12 bank-0 entry
  (sewer resolution) and star-family entry (backdrop reclassification);
  commit 7688c8d1 (attempted-key miss logging).
