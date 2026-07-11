# ADR-0002: Feature-Off Remains Upstream-Grade

- Status: Accepted
- Date: reaffirmed 2026-06-10

## Context

Enhancement work can accidentally alter the stable renderer even when the
feature is disabled.

## Decision

With hi-res replacement and scaling disabled, preserve upstream-grade behavior.
Any divergence must be deliberate, attributable, and covered by a focused
test.

Screenshot digest equality is a valid gate only for:

- feature-off parity;
- authority remint verification.

Fixture manifests own the expected identities. Hi-res-on behavior follows
[ADR-0004](0004-hires-on-validation-methodology.md).

## Consequences

- Enhancement gates should make the disabled path inert by construction.
- Hashes identify ROMs, packs, states, and captures; outside the two cases above
  they do not prove runtime correctness.

## Evidence

The required and runtime test profiles continuously exercise the protected
path; renderer commits record their focused verification.
