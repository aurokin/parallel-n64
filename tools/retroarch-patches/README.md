# RetroArch Agent-Control Patches

These 8 patches add the deterministic agent-control command set that the
scenario/adapter stack in this repo depends on (`tools/adapters/retroarch_stdin_session.sh`,
all `tools/scenarios/*` runners, the remint scripts, and the runtime conformance lanes).

Commands added: `PING`, `SET_PAUSE`, `STEP_FRAME <n>`, `SET_INPUT_PORT` /
`CLEAR_INPUT_PORT` / `GET_INPUT_PORT`, `LOAD_STATE_SLOT_PAUSED`, `WAIT_SAVE_STATE`,
a fixture-relative `frame=` counter in `GET_STATUS`, and a `READ_CORE_MEMORY`
fallback to system RAM. They register in RetroArch's shared command table, so the
same vocabulary works over the stdin transport (used here; zero ports, zero daemons)
and the network command interface.

Patch 0008 adds the `--start-paused` CLI flag: content loads normally, then the
frontend pauses before the first core frame runs, with the agent frame counter
zeroed (`GET_STATUS` reports `PAUSED ... frame=0`). Combined with `STEP_FRAME`
this gives fully deterministic power-on starts (validated: two boots stepped to
frame 180 produce byte-identical captures). The interactive adapter exposes it
as `start --start-paused`.

## Canonical locations

- Branch: `agent-control` on `github.com:aurokin/RetroArch` (tip `9bbfd647ad`)
- Local checkout: `/home/auro/code/RetroArch` (same branch)
- These patch files are the in-repo backup so the control stack survives any
  RetroArch re-clone or reset. **Never leave these commits unreferenced again** —
  on 2026-06-09 the checkout was reset to upstream and the patches survived only
  in the reflog.

## Rebuild

```sh
cd /home/auro/code/RetroArch
git checkout agent-control
./configure --disable-wayland --enable-x11 --enable-opengl --enable-vulkan \
            --enable-sdl2 --enable-alsa --enable-udev --enable-freetype --enable-zlib
make -j"$(nproc)"
```

If starting from a fresh clone without the branch:

```sh
git checkout -b agent-control upstream/master   # or a pinned upstream commit
git am /home/auro/code/parallel-n64/tools/retroarch-patches/*.patch
```

## Verify

```sh
strings /home/auro/code/RetroArch/retroarch | grep -E '^(PING|STEP_FRAME|SET_INPUT_PORT)$'
```

then a live check: any scenario run's adapter prologue gates on `PING OK`
(`WAIT_COMMAND_READY` in `tools/adapters/retroarch_stdin_session.sh`).
