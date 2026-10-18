# Regression scenario format v1

The runner in this directory consumes scenarios; external corpus builders
produce them. Scenario files are portable JSON: no host paths, no ROM or state
bytes. Binary inputs are content-addressed blobs resolved from a caller-supplied
blob directory.

## Scenario file (`<id>.json`)

```json
{
  "schema": "n64-regression-scenario/1",
  "id": "pm-segment-0051",
  "game": {"rom_sha256": "<sha256 of the ROM file as supplied>", "title": "Paper Mario (USA)"},
  "start": {"kind": "power-on"}
         | {"kind": "state", "state_sha256": "<sha256 of decompressed M64+SAVE body>",
            "savefile_sha256": "<sha256 of .srm>" | null},
  "program": ["SET_INPUT_PORT 0 0x100", "STEP_FRAME 12", "CLEAR_INPUT_PORT 0", "STEP_FRAME 30"],
  "checkpoints": [
    {"after_frames": 42, "capture": true,
     "ram": [{"name": "pm.area_map_entry", "address": "0x800740A8", "length": 8}]}
  ],
  "end": {"save_state": true},
  "expect": {
    "<condition>": {
      "end_state_sha256": "<decompressed body sha>",
      "checkpoints": {"42": {"ram": {"pm.area_map_entry": "<hex bytes>"},
                             "capture_sha256": "<png sha, feature-off conditions only>"}}
    }
  },
  "source": {"kind": "eval-segment|lab-bundle|durable|fixture|attract|corpus-state",
             "ref": "<run id / bundle id / durable id>", "segment": 51,
             "recorded_core_sha256": "<core that produced the recording>",
             "recorded_end_state_sha256": "<as recorded>"},
  "tags": {"scene": "kkj_13/e0", "mode": "battle", "chapter": 0, "tier": ["smoke", "nightly"]},
  "modes": ["off"]
}
```

Rules:

- `id` matches `[A-Za-z0-9._-]+`; it names the bundle and expectation files.
- `modes` (optional, default `["off", "on"]`) lists the hi-res modes the
  scenario covers. A run in another mode lists the scenario as not run; use
  `["off"]` where the game's pack replaces nothing in that section.
- `program` uses only `SET_INPUT_PORT`, `CLEAR_INPUT_PORT` and `STEP_FRAME`,
  exactly as the RetroArch agent-control surface accepts them. Frames advance
  only through `STEP_FRAME`; the session stays paused otherwise. A power-on
  start begins paused at frame 0. A state start bootstraps one frame, loads
  paused (barrier and log check) and counts frames from the load.
- `after_frames` counts frames stepped since the start (load or power-on).
  A capture at `after_frames: 0` after a paused load precedes any presented
  frame and comes back black.
- RAM reads are word-aligned, multiples of 4 bytes and taken only while paused.
  Values are recorded as lowercase hex of the bytes the frontend returns
  (native RDRAM word order).
- `condition` is `"<mode>:<runtime-class>"`, for example
  `"off:linux-radv-headless-vk-4x"` or `"on:linux-radv-headless-vk-4x"`. The
  runtime class is caller-declared and recorded.
- Expectations are added only by an explicit golden-recording run, never edited
  by hand, and each carries the core sha it was recorded with. They may be
  embedded under `expect` or stored in a sibling `<id>.expect.json` (below).
- `capture_sha256` is asserted only for `off:*` conditions
  ([ADR-0002](../../docs/adr/0002-protected-feature-off-property.md)). `on:*`
  captures are stored and, when they change, queued for review, never gated
  ([ADR-0004](../../docs/adr/0004-hires-on-validation-methodology.md)).
  `end_state_sha256` gates both `off:*` and `on:*` conditions
  ([ADR-0020](../../docs/adr/0020-replacement-stays-out-of-rdram.md)); at
  upscaled runtime classes the on-mode value equals the off-mode one.

## Expectation file (`<id>.expect.json`)

`--record-expectations` writes one file per scenario under
`<out>/expectations/`. Review it, then pass its directory with
`--expectations`. A file entry for a condition replaces the scenario's
embedded `expect` entry for that condition.

```json
{
  "schema": "n64-regression-expectations/1",
  "scenario": "pm-segment-0051",
  "conditions": {
    "off:<runtime-class>": {
      "recorded": "2026-10-15T12:00:00Z",
      "core_sha256": "...", "frontend_sha256": "...", "rom_sha256": "...", "pack_sha256": null,
      "end_state_sha256": "...",
      "checkpoints": {"42": {"ram": {"pm.area_map_entry": "..."}, "capture_sha256": "..."}}
    },
    "on:<runtime-class>": {
      "checkpoints": {"42": {"ram": {"pm.area_map_entry": "..."},
                             "reference_capture_sha256": "..."}}
    }
  }
}
```

`reference_capture_sha256` identifies the last reviewed hi-res-on capture. A
different capture is nominated for review; it never fails a run.

## Blob store

- `<blobs>/states/<state_sha256>.state` — raw M64+SAVE body (RetroArch loads raw).
- `<blobs>/savefiles/<sha256>.srm`.
- `<blobs>/roms/<rom_sha256>.<ext>`, or a caller ROM map / single `--rom`.

Builders write blobs; runners only read them and verify each blob's sha256
against its name before launch.

## Result record

The runner appends one JSON line per scenario run to `<out>/results.jsonl`:

- identity: `scenario, condition, mode, runtime_class, core_sha256,
  frontend_sha256, rom_sha256, pack_sha256, scenario_file, bundle`;
- timing: `started, ended, frames_run, wall_seconds, fps` (stepping rate);
- `exit_class`: `clean | nonzero | forced | timeout | load-failed |
  step-timeout | orphan-reaped | not-run` (`not-run`: inputs missing or
  mismatched, nothing launched);
- `checkpoints[{after_frames, ram{}, ram_errors{}, capture_path,
  capture_sha256, uniform_black}]`, `end_state_sha256`;
- `hires {summary_lines, summary_count, provider, entries, hits, draw_hits,
  misses, fallback_reasons, cache_load_failed}`;
- `gates[{name, status: pass|fail|review|skip, detail}]`, `log_flags[]`,
  `orphans[]`, `error`, `expectation_source`, `expectation_core_sha256`.
