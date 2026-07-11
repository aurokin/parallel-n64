# ADR-0005: GlideN64 oracle policy: three sanctioned uses, three hard bans

- Status: Accepted
- Date: 2026-06-10

## Context

Every available texture pack is authored against GlideN64/GLideNHQ, making GlideN64
indispensable as a reference — and Attempt A died treating its output as a numeric
target. The research-sweep era preference for per-game/per-scene fix metadata
(common in other emulators) was found to invite the same trap: scene-shaped
patches that nobody can justify from first principles.

## Decision

GlideN64 (via the mupen64plus-next vehicle) may be used **only** as:

1. A **source-level semantic oracle** — read its code to understand intended
   behavior and conventions.
2. A **txDump hash-coverage oracle** — Rice-named dumps set-intersected with pack
   entries and parallel hit/miss logs (interpretation rules in ADR-0012).
3. **Side-by-side screenshots for content judgment** ("is the right art
   displayed?") as disposable evidence-bundle context.

Hard bans:

- No numeric image-similarity metric against GlideN64 output in tests or pass/fail
  logic.
- No renderer commit justified by "match GlideN64".
- No per-scene or per-descriptor renderer overrides. (This supersedes the research
  sweep's recommendation of per-scene fix metadata: renderer fixes must be
  class-general; scene-specific divergence is resolved by classification — pack
  curation or explicit fallback, see ADR-0015.)

Matching GlideN64's *conventions at the source level* (Rice-CRC keying, display-view
dims, even bug-for-bug quirks) is sanctioned use 1 and is the basis of the identity
lane (ADR-0006, ADR-0013, ADR-0014). It is distinct from the banned image-matching.

## Consequences

- Enforced mechanically: the canary-tested gate `emu.support.gliden64_reference_guardrails`
  (commit 1eac0dbd) fails if image metrics or GlideN64 digest gating enter the test
  surface.
- "Equivalent-to-glide" became a valid *closing verdict* for content divergence
  (ADR-0015) — an oracle comparison, not a numeric target.

## Evidence

- commit `1eac0dbd`;
- `emu.support.gliden64_reference_guardrails`.
