# ADR-0008: Fixture And Evidence Authority

- Status: Accepted
- Date: 2026-06-10

## Context

Live frontend state, global save directories, temporary scripts, and
machine-local assumptions do not produce reproducible renderer evidence.

## Decision

Each tracked fixture defines:

- ROM identity;
- state identity and authority lineage;
- configuration and pack identity;
- settle and capture points;
- required semantic and renderer evidence.

The steady-state path is authoritative state → settle three frames → capture.
Controller scripts bootstrap or remint authorities; they do not replace them.

Each run writes a bundle containing the resolved inputs, final capture, logs,
hit/fallback evidence, and available semantic traces.

## Consequences

- Fixture YAML and the authority graph are the machine-readable authority.
- Large inputs and outputs stay outside Git.
- Remints promote only after the feature-off authority check succeeds.

## Evidence

- [fixture manifests](../../tools/fixtures/);
- [scenario contract](../../tools/scenarios/);
- fixture verification support tests.
