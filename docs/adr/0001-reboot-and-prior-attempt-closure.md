# ADR-0001: Close Attempts A and B; reboot under the Reboot Plan

- Status: Accepted
- Date: 2026-06-10

## Context

Two prior runs at hi-res replacement failed in characteristic ways:

- **Attempt A** (`origin/hires/current-stack-2026-03-18`) died chasing GlideN64
  pixel-diff scores as a correctness target.
- **Attempt B** (master 2026-03-21..05-01; PHRB runtime plus native-sampled identity
  enrichment) drowned in a circular identity-enrichment program and
  metadata-counter gates.

An earlier review had already classified the pre-reboot hi-res code as a
prototype/spike ("lookup and metadata plumbing started", e.g. `ReplacementMeta.orig_w/orig_h`
never populated, decoders with no call sites) rather than a polishable base.

## Decision

Both attempts are closed outright. The project restarts under
[docs/REBOOT_PLAN.md](/home/auro/code/parallel-n64/docs/REBOOT_PLAN.md) as the
controlling plan with a fixed, verified work order. The Attempt B plan stack is
archived under [docs/history/](/home/auro/code/parallel-n64/docs/history). The PHRB
runtime is retained; the enrichment program is frozen (see ADR-0006).

Paper Mario is the strict validation title until the first major milestone is
stable; SM64/OoT (and later MK64/MM) are compat-path breadth checks only — the
Paper Mario decomp exposes named scene/state semantics no other target offers,
and Paper Mario is also the title that exposes identity weaknesses.

## Consequences

- All subsequent work is sequenced by the Reboot Plan work order; each step carries
  its own verification.
- Deep forensics (keying, beats, campaigns) run on Paper Mario only; breadth titles
  get serial intro watches and boot lanes.
- The failure modes of A and B are codified as standing policy: ADR-0005 (oracle
  bans) guards against A's relapse, ADR-0004 (no metadata-count gates) against B's.

## Evidence

- PROJECT_NOTES.md "2026-06-10 Reboot" entry; docs/history/ archive banners.
