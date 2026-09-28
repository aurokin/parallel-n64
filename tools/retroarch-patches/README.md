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

## Maintained frontend versus historical patch series

Use `aurokin/RetroArch:agent-control` for the maintained frontend, including
headless Vulkan and platform fixes. These ten patches reconstruct the older
stdin-control feature series only. They do not reconstruct the complete
maintained branch, and applying them does not qualify a headless or macOS runner.

On September 28, 2026, all ten patches applied in order to base
`4d9e2ebf280c22fbf1ed85c8a030ca8af03ffd63`. Compared with maintained revision
`159dac03a8`, the reconstructed tree lacks the headless Vulkan context and
subsequent frontend/build/platform fixes. These hashes identify the comparison,
not a promise that a moving upstream branch accepts the same patches.

## Reconstruct the historical series

For research into the historical stdin feature series, set `RETROARCH_ROOT`
to an isolated RetroArch checkout containing the recorded base, then apply
in order:

```sh
git -C "$RETROARCH_ROOT" checkout -b historical-stdin-series 4d9e2ebf280c22fbf1ed85c8a030ca8af03ffd63
git -C "$RETROARCH_ROOT" am \
  "$PWD"/tools/retroarch-patches/*.patch
```

Keep this reconstruction separate from the maintained frontend and existing
runtime artifacts. A successfully applied patch series is source evidence;
it is not runtime-equivalence evidence.

## Build And Verify

Configure RetroArch for the target platform, then verify the resulting binary:

```sh
strings "$RETROARCH_ROOT/retroarch" |
  grep -E '^(PING|STEP_FRAME|SET_INPUT_PORT|WAIT_LOAD_STATE)$'
```

A live adapter session must additionally receive `PING OK` and prove
pause, step, save/load completion, screenshot, and clean teardown.
