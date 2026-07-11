# ADR-0004: Validate Hi-Res-On Semantically And Visually

- Status: Accepted
- Date: 2026-06-10; reviewer-hint amendment 2026-06-12

## Context

Hi-res-on output intentionally differs from baseline. Pixel digests and exact
metadata counts therefore create false confidence.

## Decision

Use:

1. class-level semantics such as entry presence, draw-hit presence, expected
   source mode, and explicit fallback reasons;
2. paired captures with an explicit verdict: correct, missing, wrong texture,
   wrong region, or corruption;
3. falsification experiments and targeted probes before changing renderer or
   shader behavior.

Reviewer context may identify the scene but must not describe the suspected
artifact as expected content. Independent review is useful for consequential
visual claims, but no fixed agent count is part of the contract.

Corruption always fails. An explicit fallback with a valid reason is acceptable
when the scenario permits it.

## Consequences

- No hi-res-on digest or exact-count gate.
- Every runtime claim leaves a bundle with identities, logs, semantic evidence,
  and the capture used for judgment.
- Shader probes are diagnostic evidence, not production policy.

## Evidence

The required gate enforces the no-metric policy; runtime authority runners emit
the class-level evidence consumed by reviewers.
