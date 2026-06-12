# ADR-0016: Trimmed test surface: behavior-backed lanes, loud failures

- Status: Accepted
- Date: 2026-06-10

## Context

The pre-reboot CTest registry (125 tests) locked script contracts and metadata
counters rather than renderer truth, and silent exit-77 skips hid broken
prerequisites. Attempt B's dump gate gated on artifacts nobody reviewed.

## Decision

- The registry was trimmed 125 → 48 (49 as of 2026-06-12, after the ADR-0005
  guardrails canary was added), organized into two fast canonical profiles
  every change runs: `emu-required` (functional/unit/support contracts, 43 as of
  2026-06-12) and `emu-runtime-conformance` (lavapipe smoke + one class-level
  Paper Mario runtime authority lane). SM64/OoT breadth lanes are registered for
  on-demand runs.
- The dump gate is deleted; savestate fixtures + exact-shot recipes are the repro
  backbone instead of a committed dump/replay corpus.
- Once opted in (`EMU_ENABLE_RUNTIME_CONFORMANCE=1`), missing runtime
  prerequisites for the gating lanes fail loudly with staging instructions. The
  on-demand SM64/OoT breadth lanes still exit-77 skip with a printed reason when
  their assets are unstaged (they are opt-in breadth, not gates).
- Runtime lanes assert class-level semantics only (ADR-0004): entry/draw-hit
  presence, source-mode class, explicit fallback reasons. Emulator-facing runtime
  tests run at 4x internal scale, one at a time (they occupy the display).

## Consequences

- Two-command verification for every change:
  `./run-tests.sh --profile emu-required` and
  `./run-tests.sh --profile emu-runtime-conformance`.
- Test additions must be behavior-backed; gate-surface guardrails themselves are
  canary-tested (the GlideN64 no-metric gate, ADR-0005).

## Evidence

- Commits b6f54147..b6e0b767; run-tests.sh profiles;
  [docs/EMU_TESTING.md](/home/auro/code/parallel-n64/docs/EMU_TESTING.md).
