# ADR-0016: Behavior-Backed Test Surface

- Status: Accepted
- Date: 2026-06-10

## Context

The earlier test registry overfit script shape and metadata counters, while
silent skips hid missing prerequisites.

## Decision

- `emu-required` is the display-free change gate: behavior-focused
  unit/support tests and static conformance.
- `emu-runtime-conformance` is the heavy authority lane and runs when
  runtime-facing behavior or release confidence requires it.
- Compatibility breadth remains explicit and opt-in.
- Once a gating runtime lane is enabled, missing prerequisites fail loudly.
- Runtime assertions use class-level semantics, never exact counts or
  hi-res-on capture digests.

## Consequences

Tests are added for observable behavior or a durable guardrail. Runtime work is
serial and separate from the ordinary required gate.

## Evidence

- `run-tests.sh` profiles;
- [Emulator Testing](../EMU_TESTING.md);
- support and conformance tests under `tests/emulator_behavior/`.
