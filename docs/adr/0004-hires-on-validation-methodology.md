# ADR-0004: Hi-res-ON validation: class semantics, visual rubric, falsification first

- Status: Accepted
- Date: 2026-06-10; amended 2026-06-12 (reviewer-hint rule)

## Context

Attempt A chased pixel-diff scores; Attempt B gated on metadata counters. Both
produced false confidence. Hi-res-ON output legitimately diverges from baseline by
design, so neither digests nor exact counts can define correctness there.

## Decision

Hi-res-ON validation never uses pixel digests or exact metadata counts. It uses:

1. **Class-level semantics** for runtime gates: `entry_count > 0`, `draw_hits > 0`,
   expected source-mode class, explicit fallback reasons. Never exact counts.
2. **The visual review rubric**: paired before/after captures with an explicit
   judgment per scene — yes / no / wrong-texture / wrong-region. A milestone is done
   when its named scenes pass the rubric at 4x, not when a counter reaches a number.
3. **Adversarial multi-agent review panels** for visual claims at scale: one
   reviewer plus one adversarial skeptic per scene, an arbiter only on disagreement,
   rubric verdicts only. Used for the trilinear default (10 agents), the gameplay
   campaign (28), and the beat comparison (28).
   - *Amendment (2026-06-12):* reviewer hints must describe the scene, never the
     suspected artifact — a panel once scored a corrupted scene "yes" because the
     hint described the corruption as expected content.
4. **Falsification before renderer/shader edits**: hypotheses are
   confirmed/falsified by controlled experiments first. The inaugural 6-condition
   sampler experiment falsified the suspected `/SCALING_FACTOR` wrong-region bug
   (the division is correct) and confirmed sub-texel fraction discard as the real
   bug — saving correct code from being "fixed".
5. **Probe-driven verification** when fixed-vs-broken looks similar at a glance:
   injected shader probes (magenta branch probe, grayscale identity probe, sub-row
   phase paint) plus offline provider dumps instead of eyeballing.

Corruption is always a fail; explicit fallback with a reported reason is acceptable
when the plan claims it.

## Consequences

- ON-path regressions are caught by panels and class counters; this accepts that
  ON-path validation needs judgment, not arithmetic.
- Every falsification/verification run leaves an evidence bundle (ADR-0008).
- Probes require shader rebuild access (see ADR-0010 on the pinned slangmosh).

## Evidence

- artifacts/experiments/sampler-falsification-062900, trilinear-default-204024,
  gameplay-campaign-011041 (panel-results.json, user-beats-* bundles); Reboot Plan
  "Success Criteria Style".
