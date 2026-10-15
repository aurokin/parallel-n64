# Regression Scenario Runner

`run_scenarios.py` runs a matrix of portable scenarios
([format](SCENARIO_SCHEMA.md)) through
[`retroarch_interactive_session.sh`](../adapters/retroarch_interactive_session.sh)
and gates each run. It is emulator-facing: run it serially, at 4x, on a host
with no other RetroArch process. It needs only Python 3's standard library.

## Usage

```sh
tools/regression/run_scenarios.py \
  --scenarios "$CORPUS/scenarios" --blobs "$CORPUS/blobs" \
  --core "$CORE_PATH" --retroarch-bin "$RETROARCH_BIN" \
  --base-config "$RETROARCH_BASE_CONFIG" \
  --runtime-class linux-radv-headless-vk-4x --mode off \
  --expectations "$CORPUS/expectations" \
  --select tier=smoke --out artifacts/regression/smoke-off
```

- `--mode on` requires `--pack PATH` (a `.phrb` package).
- ROMs resolve from `--rom PATH` (one ROM, sha checked), `--rom-map JSON`
  (`{"<rom sha256>": "/path"}`) or `<blobs>/roms/<sha>.*`.
- `--select` takes an id glob or `tag=value[,value]`; repeated filters must
  all match. `--select tier=smoke --select mode=battle` selects smoke battle
  scenarios.
- `--step-chunk N` (default 120) bounds every `STEP_FRAME`; steps also split
  at checkpoint frames.
- `--ttl SEC` overrides the per-session TTL, which otherwise scales with the
  program length.
- `--resume` skips scenarios that already have a record for this condition in
  `--out`.
- `--record-expectations` writes observed values to
  `<out>/expectations/<id>.expect.json` for review; it never edits scenario
  files and skips runs that were not clean.
- `RETROARCH_VIDEO_CONTEXT_DRIVER` and other adapter environment variables
  pass through unchanged. Declare a distinct runtime class for each backend,
  presentation mode and host profile.

Output in `--out`:

- `results.jsonl` — one [result record](SCENARIO_SCHEMA.md#result-record) per run;
- `report.md` — summary, failures, review queue with capture paths, log flags
  and stepping FPS;
- `bundles/<id>__<mode>/` — the adapter bundle: logs, captures, states and
  `logs/regression.adapter.log` (every adapter call and its reply).

Exit status: 0 when no gate failed (review items allowed), 1 when any gate
failed, 2 for invalid scenarios or a busy host.

## Per-scenario sequence

1. `start --start-paused` in a fresh process, with the start state staged as
   `ParaLLEl N64/<content basename>.state1` and the `.srm` passed as the
   savefile source.
2. State starts: step one bootstrap frame, then `load-slot --paused`, which
   waits for `WAIT_LOAD_STATE DONE` and checks the load log
   ([IL-19](../../docs/ISSUE_LOG.md#il-19-complete-load-slot-with-wait_load_state)).
3. Send the program. Each `STEP_FRAME` is followed by `GET_STATUS` polling
   until the session is `PAUSED` at exactly the expected frame.
4. At checkpoints, read RAM cues with `READ_CORE_MEMORY` and capture.
5. Save slot 2 and hash the unwrapped M64+SAVE body
   (`tools/savestate_bridge.unwrap`).
6. `stop`, then find processes whose command line names the bundle. Survivors
   are terminated and recorded as `orphan-reaped`; if one cannot be reaped the
   run stops.

## Tiers

Tiers are scenario tags, selected with `--select tier=<name>`:

| Tier | Intended content | Typical use |
|---|---|---|
| `smoke` | short state loads and settles, off and on | per change |
| `nightly` | the full state matrix plus replay segments | nightly |
| `soak` | long attract loops and segments, repeated runs | weekly |

Run each mode as its own invocation and output directory. A soak repeat is a
second run into a new `--out`; compare their records.

## Gate policy

| Gate | off | on |
|---|---|---|
| `exit` — exit class is `clean` | fail | fail |
| `load` — paused load succeeded | fail | fail |
| `ram@F:name` — RAM cue equals expectation | fail | fail |
| `capture@F` — PNG sha equals expectation | fail ([ADR-0002](../../docs/adr/0002-protected-feature-off-property.md)) | review ([ADR-0004](../../docs/adr/0004-hires-on-validation-methodology.md)) |
| `end_state` — end-state body sha | fail | report-only |
| `hires.provider` — summary has `provider=on`, no cache load failure | — | fail |
| `hires.entries`, `hires.draw_hits` — at least one | — | fail |
| `hires.inert` — no `provider=on` summary | fail | — |

- Missing expectations make a gate `skip`, not `pass`.
- Capture digests are gates only feature-off (ADR-0002). A changed hi-res-on
  capture is nominated for visual review and never fails a run; presence gates
  are class-level semantics, not counts (ADR-0004,
  [ADR-0016](../../docs/adr/0016-test-surface-trim.md)).
- No image-similarity metric or GlideN64 output enters any gate
  ([ADR-0005](../../docs/adr/0005-gliden64-oracle-policy.md)).
- `draw_hits` uses the summary's `compat_draw_hits` (draw-time replacement,
  per [Emulator Testing](../../docs/EMU_TESTING.md)); summaries without that
  field fall back to lookup hits.
- The hi-res summary is read from the bundle's RetroArch log; the last
  `Hi-res keying summary` line wins. The runner sets
  `PARALLEL_RDP_HIRES_SUMMARY_INTERVAL_FRAMES` in on mode so a core that
  emits periodic summaries leaves evidence even when shutdown is forced; cores
  without it log only at shutdown.
- Sessions stay paused and advance only by explicit frame steps
  ([ADR-0009](../../docs/adr/0009-interactive-adapter-and-deterministic-play.md)).
  Each bundle records its inputs and logs
  ([ADR-0008](../../docs/adr/0008-fixture-and-evidence-authority.md)).
- Log flags (`hires-cache-load-failed`, `state-load-failed`,
  `uniform-black-capture`, `quit-timeout`, `ttl-expired`, `orphan-reaped`,
  `hires-summary-missing`) are reported for triage; gates decide the outcome.

Expectations are evidence for one runtime class. Re-record them only with a
justification, and keep bundles under the
[retention policy](../../docs/EVIDENCE_RETENTION.md).
