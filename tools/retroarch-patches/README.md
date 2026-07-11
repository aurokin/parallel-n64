# RetroArch Agent-Control Patch Set

These patches add deterministic, generic frontend control used by the adapters
and scenarios in this repository. They contain no game- or renderer-specific
semantics.

## Command Surface

- readiness and status: `PING`, `GET_STATUS`;
- pause and stepping: `SET_PAUSE`, `STEP_FRAME`,
  `--start-paused`;
- input: `SET_INPUT_PORT`, `CLEAR_INPUT_PORT`,
  `GET_INPUT_PORT`;
- states: `LOAD_STATE_SLOT_PAUSED`, `WAIT_SAVE_STATE`,
  `WAIT_LOAD_STATE`;
- replay: `RECORD_REPLAY_PATH`, `PLAY_REPLAY_PATH`,
  `STOP_REPLAY`;
- memory: `READ_CORE_MEMORY` with system-RAM fallback.

`WAIT_LOAD_STATE` drains the asynchronous frontend load task. A
client must issue it after `LOAD_STATE_SLOT_PAUSED` when completion
matters.

## Apply

Set `RETROARCH_ROOT` to a RetroArch checkout and apply the series in
order:

```sh
git -C "$RETROARCH_ROOT" checkout -b agent-control upstream/master
git -C "$RETROARCH_ROOT" am \
  "$PWD"/tools/retroarch-patches/*.patch
```

The maintained public branch is `aurokin/RetroArch:agent-control`.
Patch files remain the portable reconstruction path; a commit hash is evidence,
not a permanent installation path.

## Build And Verify

Configure RetroArch for the target platform, then verify the resulting binary:

```sh
strings "$RETROARCH_ROOT/retroarch" |
  grep -E '^(PING|STEP_FRAME|SET_INPUT_PORT|WAIT_LOAD_STATE)$'
```

A live adapter session must additionally receive `PING OK` and prove
pause, step, save/load completion, screenshot, and clean teardown.
