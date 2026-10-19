# ADR-0019: Deterministic emulation-state gates for regression scenarios

- Status: Accepted, amended
- Date: 2026-10-16

## Context

Recorded gameplay — frame-stepped agent and TAS sessions with hashed start and end
savestates — is the cheapest source of broad regression coverage: playing games is
hard for agents, replaying their recordings is not. Repeated programs from power-on
have produced byte-identical savestates in every observed group, including across
hosts, and an immediate re-save reproduces the loaded state exactly. ADR-0002 limits
pixel-digest gates to feature-off parity and remints; it does not address emulation
state.

Replaying 90 recorded eval segments on their recording core reproduced all 9
power-on segments and 69 of 81 savestate-start segments byte for byte. A second
replay of the 12 that differed reproduced the first replay exactly, so the
difference is between the recording and any fresh load, not run-to-run noise.
Replaying three of them as one continuous program from power-on, without the
load, reproduced the recording each time: the savestate omits some emulator
state that affects later timing (see the issue log).

Running the 153 nightly scenarios (Paper Mario, Mario Kart 64 and Super Mario 64)
with and without hi-res replacement left every savestate byte outside RDRAM and
every checkpoint RAM cue identical. RDRAM differed in the frame and depth
buffers, plus two small Paper Mario regions whose role is unconfirmed: a
651-word block in one segment and a single word in one scene.

## Decision

1. A regression scenario is a start (power-on or a savestate identified by the
   sha256 of its decompressed body), a frame program of input and step commands,
   and checkpoints ([`tools/regression/SCENARIO_SCHEMA.md`](../../tools/regression/SCENARIO_SCHEMA.md)).
2. With hi-res replacement disabled, the savestate body hash at the end of a
   scenario and the named RAM cues at its checkpoints are exact gates for a
   runtime class. This extends feature-off parity from pixels to emulation state.
3. Expectations are recorded by an explicit runner pass, store the core identity
   they were recorded with, and change only with a justification naming the change
   that moved them.
4. With hi-res replacement enabled, RAM cues and every savestate byte outside RDRAM
   must equal the feature-off run of the same scenario (`compare_modes.py`).
   RDRAM differences are reported for review: replacement legitimately changes the
   framebuffer and depth contents the renderer writes back. End-state hashes stay
   report-only in that mode, and captures remain review-only (ADR-0004).
5. A scenario built from a recording is qualified before it gates: replaying the
   recorded program on the recording core must reproduce the recorded end state.
   A segment that fails qualification is excluded from gating and kept as a
   finding with its cause; it is not re-recorded into a golden.
6. Gates are fresh-process per scenario. A scenario that is not reproducible
   across two runs on the same core and runtime class is a determinism defect,
   not a flaky test.

Amendment (2026-10-18): with [ADR-0020](0020-replacement-stays-out-of-rdram.md)
replacement no longer reaches RDRAM when upscaling. The on-mode end-state hash
now gates like the off-mode one, and `compare_modes.py --strict-rdram` requires
the on-mode end state to equal the off-mode one at upscaled runtime classes.

Amendment (2026-10-19): savestates now carry the RDP noise seed and saving
resumes like a load (issue log, "Savestate load does not reproduce the saving
run"), so recordings made by such cores reproduce from their saves. Decision 5
still applies: recordings made before the fix keep their qualification
outcome on their recording core.

## Consequences

- Recorded agent play becomes a durable regression corpus; future play follows the
  recording contract so it qualifies.
- A changed end-state hash fails until explained, even when pictures look the same.
- Runtime classes must stay explicit; headless and headed baselines remain separate.

## Evidence

Recorded-segment qualification and continuous-chain replays of October 15–16,
2026 ([issue log](../ISSUE_LOG.md#savestate-load-does-not-reproduce-the-saving-run));
the savestate bridge round-trip and immediate re-save identity checks; the
off/on comparison in [`tools/regression/README.md`](../../tools/regression/README.md#hi-res-presentation-only-check).
