# ADR-0006: Replacement identity: GlideN64-compat Rice-CRC lane primary, strict identity

- Status: Accepted
- Date: 2026-06-10

## Context

N64 replacement identity is hard: TMEM is 4KB and layout-sensitive, TLUT/palette
state changes the meaning of sampled data, and hash-only schemes from other
consoles do not transfer. The discarded native-sampled design tried to include
upload descriptors, TMEM windows, tile state, TLUT state, and sampled subrects
in one identity, then stalled on runtime enrichment. The failed branch also drifted into
permissive reinterpretation — mask/stride guesses, birth-family heuristics,
alternate CRC candidates — "increasingly clever ways to justify a match".

Meanwhile every available pack is authored against the GlideN64/GLideNHQ Rice-CRC
convention (texture CRC + palette CRC over the render-tile view).

## Decision

1. **The GlideN64-compat Rice-CRC lane is the primary replacement-identity path**,
   proven cross-game (SM64, OoT, Paper Mario). TLUT/palette state is part of
   identity (palette CRC; TLUT state shadowed). The compat key is computed at
   draw time from the rendering tile's descriptor — the same render-tile view
   GlideN64's Rice CRC hashes; a faster upload-time lookup lane also exists, but
   the draw-time compat lane is the authoritative resolver and upload-lane misses
   are expected noise on split views (ADR-0015).
2. **The native-sampled-identity/enrichment program is frozen**: code dormant
   (debug flag only), no gates depend on it, no further converter-review work.
3. **Strict identity over permissive reinterpretation.** Keying work is byte-level
   conformance to the reference convention — including reproducing its quirks
   bug-for-bug (ADR-0013, ADR-0014) — never heuristic fallback. The low32 family
   fallback exists but is env-gated off by default; a 2026-06-12 probe confirmed
   permissive serving picks wrong variants.

## Consequences

- All keying fixes since the reboot are conformance derivations, reproducible
  offline against captured RDRAM (bank-0 proof, display-dims proof).
- Palette-animated content multiplies variants (the tinted-sprite families) — an
  accepted cost handled by exact-variant sets (ADR-0007).
- SM64 resolves entirely through the draw-time compat CRC lane, making it the
  designated stress title if draw-time CRC cost ever matters.

## Evidence

- commits `7688c8d1`, `979cdc98`, and `76892a69`;
- the bounded low-32 fallback falsification experiment.
