# Adapters

This directory is for wrapper glue that connects this repo to external projects and local tooling.

Expected adapter targets include:

- RetroArch command/control helpers
- local environment discovery
- artifact collection and normalization

Current tracked adapter seeds:

- [`retroarch_stdin_session.sh`](/home/auro/code/parallel-n64/tools/adapters/retroarch_stdin_session.sh)
- [`retroarch_interactive_session.sh`](/home/auro/code/parallel-n64/tools/adapters/retroarch_interactive_session.sh)
- [`prepare_retroarch_mvk141_app.sh`](/home/auro/code/parallel-n64/tools/adapters/prepare_retroarch_mvk141_app.sh)
- [`promote_interactive_state.py`](/home/auro/code/parallel-n64/tools/adapters/promote_interactive_state.py)
- [`png_uniform_black.py`](/home/auro/code/parallel-n64/tools/adapters/png_uniform_black.py)

Higher-level TAS step/capture loops, Paper Mario probes, gameplay macros, and
durable gameplay state indexes live in `parallel-n64-lab`.

Current RetroArch adapter notes:

- the adapter refuses to start if any other `retroarch` process is already running
- the adapter now also holds a runtime lock so concurrent tracked launches cannot race past the singleton check
- runtime launches are standardized for tracked capture; macOS launches are windowed by default with a 1920x1080 target so Computer Use can observe and operate RetroArch without fullscreen Spaces
- commands are sent serially over the stdin command interface
- `WAIT <seconds>` is a local adapter pseudo-command and is not forwarded to RetroArch
- `WAIT_COMMAND_READY <timeout_seconds>` is a local adapter pseudo-command that waits for a `PING OK` reply from RetroArch before tracked command sequences proceed
- `WAIT_STATUS_FRAME <state> <min_frame> <timeout_seconds>` is a local adapter pseudo-command for frame-aware waits based on `GET_STATUS`
- `WAIT_CORE_MEMORY_HEX <address> <number_of_bytes> <expected_hex> <timeout_seconds>` is a local adapter pseudo-command for exact RAM-signature waits
- `SNAPSHOT_CORE_MEMORY <label> <address> <number_of_bytes>` is a local adapter pseudo-command that captures a `READ_CORE_MEMORY` reply into a bundle trace file
- the adapter disables RetroArch quit confirmation in its per-run appendconfig so a single tracked `QUIT` command exits deterministically
- the adapter disables savestate thumbnails in its per-run appendconfig because that frontend path currently destabilizes ParaLLEl-RDP save-state runs
- the adapter disables RetroArch widgets and screenshot/save-state notifications in tracked runs so capture bytes remain stable
- the adapter writes bundle-local core options and points RetroArch at them so tracked runs can force a deterministic local core configuration
- tracked Paper Mario runs currently force `video_driver = "vulkan"` and `PARALLEL_N64_GFX_PLUGIN_OVERRIDE=parallel` to keep the baseline on the intended ParaLLEl path
- on macOS, prepare `artifacts/external/RetroArch-MVK141.app` with `tools/adapters/prepare_retroarch_mvk141_app.sh`; for `--mode on`, the runtime adapters prefer that app copy when it exists unless `--retroarch-bin` is passed explicitly, because the stock 1.2.8 MoltenVK bundle cannot run the hi-res Metal argument-buffer path
- for current Paper Mario hi-res gameplay evidence, export `PARALLEL_RDP_HIRES_CACHE_PATH=/Users/auro/code/parallel-n64/artifacts/hts2phrb-review/local-pm64-exact-variant-set/package.phrb` before launch; the metapod host config currently does not set that package path
- on macOS `--mode off` keeps `PARALLEL_RDP_DISABLE_HIRES_SHADER=1`; `--mode on` defaults `MVK_CONFIG_USE_METAL_ARGUMENT_BUFFERS=1` only when the selected RetroArch binary is the prepared MVK141 app copy
- the current RetroArch stdin agent command surface includes explicit pause, frame-step, savestate-load-paused, save-task wait, and input-port control commands
- pause via `SET_PAUSE ON|OFF|TOGGLE`; use `ON` before frame-stepped agent play
- the current RetroArch stdin command surface also includes `PING`, which is used only as a readiness probe for the adapter
- tracked Paper Mario flows now use a log-gated startup handoff plus `WAIT_COMMAND_READY` instead of blind startup sleeps
- when a core does not publish a libretro memory map, the local RetroArch build now falls back to `RETRO_MEMORY_SYSTEM_RAM` for `READ_CORE_MEMORY`
- `SAVE_STATE` is asynchronous in RetroArch; tracked flows now use `WAIT_SAVE_STATE`, and save tasks should be sequenced before screenshot tasks when minting authoritative states

Capture and end-of-session evidence hardening (both runtime adapters):

- every completed capture is pixel-decoded (`png_uniform_black.py`, stdlib-only, exact — not a checksum heuristic); a uniformly black capture emits a warning and a `logs/capture.warnings.log` record but never fails the command, because black is legitimate mid-fade
- the usual causes of black captures are a screenshot before the first presented frame after `LOAD_STATE_SLOT_PAUSED` (step at least one frame first) and GPU-backbuffer screenshots while presentation is suspended (`video_gpu_screenshot = "false"` reads the core framebuffer instead)
- session end reasons are recorded in `logs/session.end-reason` so bundles explain their own truncation (explicit-fallback evidence contract)

Interactive agent-play adapter notes (`retroarch_interactive_session.sh`):

- `start` keeps one session alive across agent turns: RetroArch runs in its own setsid process group, holds the same runtime flock as the batch adapter, and self-terminates after `--ttl-seconds` (default 3600) so a forgotten session can never become a daemon
- a TTL grace watchdog sends `QUIT` ~30s (`RETROARCH_TTL_GRACE_SECONDS`) before the hard kill and records `ttl-grace-quit` in `logs/session.end-reason`, so the core's end-of-run summaries (e.g. the hi-res keying summary) flush instead of dying with the process; the hard `timeout` kill remains as the backstop
- `start --savefile-source PATH` stages an explicit `.srm` or savefile directory into the bundle-local savefile directory before launch; use this for real gameplay file-select/loading tests
- `send`/`input`/`screenshot`/`status`/`save-slot`/`load-slot` talk to the live session over the bundle FIFO; `stop` QUITs and falls back to killing the process group
- the game runs in REAL TIME between agent commands; for deterministic play keep the session paused and use `input --frames N` (TAS-style: input held for exactly N stepped frames, proven bit-identical on replay), reserving `--hold-seconds` for menus/title screens
- RetroPad mask bits include `A=0x100`, `B=0x1`, `START=0x8`, d-pad `UP=0x10 DOWN=0x20 LEFT=0x40 RIGHT=0x80`, `L=0x400`, `R=0x800`, `Z/L2=0x1000`, `R2=0x2000`, `L3=0x4000`, `R3=0x8000`; analog values are raw signed 16-bit values
- `save-slot` tracks the active slot locally (STATE_SLOT_PLUS/MINUS are silent) and verifies the save log names the expected `.state<N>` file
- screenshots are written asynchronously by RetroArch; the adapter waits for a non-empty, size-stable capture file before reporting its path
- state loads can transiently fail while another frontend task is in flight; `load-slot` retries once before failing loudly
- use `promote_interactive_state.py` only for legacy/manual promotion flows; new
  gameplay durable states should be indexed and documented from
  `parallel-n64-lab`.

Adapters should translate between systems.
They should not become the main source of truth for renderer correctness or scene semantics.

If an adapter starts carrying major project logic, move that logic into:

- the relevant implementation repo
- a fixture manifest
- or a planning document under [`docs/`](/home/auro/code/parallel-n64/docs)
