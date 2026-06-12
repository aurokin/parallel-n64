# ADR-0002: Protected property: feature-off stays upstream-grade

- Status: Accepted
- Date: pre-reboot (foundational); reaffirmed 2026-06-10

## Context

The program's original non-negotiable constraint: adding hi-res replacement and
scaling must not destabilize the renderer people already rely on. The failed
attempts demonstrated how easily enhancement work bleeds into baseline behavior.

## Decision

With hi-res and scaling features OFF, the core must behave as close as possible to
the upstream stable renderer path. Any divergence must be deliberate, observable,
and attributable. Every renderer change re-proves feature-off bit-exactness.

Feature-off screenshot **digest equality is a valid gate here — and for savestate
remint authority verification — and nowhere else** (the hi-res-ON side is governed
by ADR-0004). Canonical feature-off digests are maintained per fixture (title
`351cf979…`, file-select `4b517fba…`, `kmr_03` `35213195…`) and re-anchored when the
machine changes.

## Consequences

- Every landed renderer change in the notebook records a feature-off digest
  re-verification.
- Implementation is constrained toward early explicit gates so the OFF path is
  untouched by construction (e.g. the bank-0 keying lane requires
  `replacement_provider`; the texrect exemptions require a bound replacement).
- "Checksum-shaped" runtime evidence stays suspect by default everywhere else:
  hashes identify artifacts (ROMs, packs, savestates), they do not prove runtime
  correctness.

## Evidence

- PROJECT_NOTES.md "Non-Negotiable Constraints"; re-verified in every entry through
  2026-06-12 (e.g. commits cb46b7d2, e647e357, 7688c8d1, 979cdc98, 76892a69 all
  record feature-off digest checks).
