# ADR-0008: Savestate ladder authority and evidence bundles

- Status: Accepted
- Date: pre-reboot (contract); ladder reminted 2026-06-10

## Context

The failed attempt's automation depended on live frontend state, `/tmp`, tmux, and
local layout assumptions, and its tests locked script contracts instead of renderer
truth. A fresh-machine reboot also invalidated all old savestates.

## Decision

1. **Hermetic fixtures before scene choreography.** Each fixture is defined by
   manifest, ROM identity, savestate identity, config snapshot, and expected
   capture points. Steady-state path: authoritative savestate → settle 3 frames →
   capture.
2. **The reminted savestate ladder is the fixture authority**: title screen →
   file select → `kmr_03 ENTRY_5` (960-frame attract ladder), reminted on this
   machine, visually verified, proven deterministic across independent sessions,
   with canonical feature-off digests (ADR-0002). Remint authority verification is
   one of the two sanctioned digest uses.
3. **Every fixture/experiment run produces a named evidence bundle** under
   `artifacts/experiments/<name>-<timestamp>/`: captures, identity/config
   snapshots, ROM/savestate/pack hashes, logs, hit/miss reporting, class-level
   telemetry (`hires-evidence.json`). Notebook entries cite bundles as the proof
   trail.

## Consequences

- Decisions are auditable; bundles are the unit users and review panels judge.
- Known caveat: `kmr_03 ENTRY_5` sits inside a scripted sequence (not free-roam)
  and the staged `.srm` carries no save — menu scenes need a post-prologue save
  (new-game recipes recorded 2026-06-12).
- Savestates are the authority once available; debug warps and scripted entry are
  acceptable only earlier in the ladder.

## Evidence

- `tools/fixtures/paper-mario-authority-graph.yaml`; PROJECT_NOTES.md 2026-06-10
  "Control stack live" entry; the artifacts/experiments/ tree
  (sampler-falsification-062900 through gameplay-campaign-011041 and its
  user-beats-* sub-bundles).
