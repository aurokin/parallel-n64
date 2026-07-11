# ADR-0001: Close Failed Attempts And Restart From Explicit Invariants

- Status: Historical
- Date: 2026-06-10

## Context

Two earlier hi-res implementations failed in different ways:

- one treated GlideN64 pixel-difference scores as renderer correctness;
- one expanded native identity enrichment and metadata accounting without
  proving the visible runtime path.

Both accumulated scene-shaped policy and gave arithmetic more authority than
rendered behavior.

## Decision

Close both attempts. Retain the useful PHRB loader/provider seam, freeze native
identity enrichment, and rebuild around:

- feature-off stability;
- Rice-compatible draw-time identity;
- class-level runtime semantics;
- explicit visual judgment;
- falsification before renderer changes.

The reboot work order completed. This ADR explains the resulting architecture;
it does not sequence current work.

## Consequences

ADRs 0002–0018 are the current decision surface. Git history retains the
discarded plans and experiment chronology without presenting them as active
instructions.

## Evidence

- branch `origin/hires/current-stack-2026-03-18`;
- commits from the 2026-06-10 restart through the accepted ADR series.
