# ADR-0013: Compat keying refinements: TLUT base shadow, bank-0 palette candidate

- Status: Accepted
- Date: 2026-06-12

## Context

After the txDump semantics correction (ADR-0012), beat forensics showed palette-
keyed families missing that GlideN64 served from the same pack. Both root causes
were conformance gaps to the pack-authoring pipeline's conventions, found by
source-level oracle reading plus byte-level RDRAM proofs — exactly the ADR-0006
playbook.

## Decision

1. **TLUT keying shadow reads from the texture image base, bug-for-bug** (7688c8d1):
   the `tlut_shadow` copy used for pack keying reads RDRAM at the texture image
   BASE address, matching GlideN64's `gDPLoadTLUT` keying memcpy which ignores
   load-tile uls/ult offsets. The TMEM mirror keeps the offset address (real
   LoadTlut semantics). A probe falsified this as the Paper Mario root cause
   (every montage TLUT load has addr == base), but the convention is kept as
   protective parity for titles that do offset TLUT loads. The keying convention
   now distinguishes "what the hardware did" (TMEM mirror) from "what the
   pack-authoring pipeline hashed" (keying shadow).
2. **Bank-0 palette candidate for CI4 tiles** (979cdc98):
   `compute_gliden64_compat_checksum64` emits a second key candidate for CI4 tiles
   with `pal != 0`, keyed against the bank-0 palette window; the rescue lane
   retries it after the primary misses. Packs authored from HLE dumps key CI
   sprites against the TLUT base (HLE regenerates tile state with palette index 0).
   Proven byte-level from captured RDRAM: pack pcrc `74d1211b` == RiceCRC(bank-0
   palette); our miss `d7cb65b9` == RiceCRC(bank-1 filler). Class-level counter
   `compat_draw_bank0_hits` added to the keying summary.

A bounded, convention-derived second candidate is not a heuristic fallback — the
strict-identity rule (ADR-0006) stands. Draw-time compat misses now log attempted
keys so censuses run on the authoritative lane (ADR-0015).

## Consequences

- Tinted-sprite beats render hi-res (`compat_draw_bank0_hits=78` on the recaptured
  states, matching GlideN64 side-by-sides).
- Verified non-regressive: feature-off digest bit-exact (the sanctioned digest
  use — ADR-0002). The nine hi-res-ON beat captures were additionally
  sha256-identical across pre/post cores, consistent with the probe's finding
  that the TLUT-shadow change is functionally inert for this title — an
  inertness observation, not a digest gate (hi-res-ON validation never uses
  pixel digests, ADR-0004).

## Evidence

- Commits 7688c8d1, 979cdc98; gameplay-campaign-011041/user-beats-parallel
  (keying-probe-findings.md), user-beats-fixA/before-after.png.
