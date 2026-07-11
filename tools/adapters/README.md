# RetroArch Adapters

These adapters translate portable scenario operations into RetroArch's generic
agent-control command surface. Renderer meaning and scene semantics stay in the
core, fixture manifests, and scenario verification.

## Entrypoints

- `retroarch_stdin_session.sh` — one batch session driven from a
  command file.
- `retroarch_interactive_session.sh` — a bounded live session
  controlled through a FIFO.
- `prepare_retroarch_mvk141_app.sh` — prepare an explicit macOS app
  bundle for a compatible MoltenVK runtime.
- `build_retroarch_agent_control_macos.sh` — build/deploy helper for
  the agent-control frontend.
- `promote_interactive_state.py` — legacy/manual state promotion.
- `png_uniform_black.py` — exact black-capture detection.

`vision_tools_mcp.py` is a legacy eval-specific shim pending migration to its
owning system. It is not a renderer adapter and must not gain new consumers
here.

Use each entrypoint's `--help` output for its current options.

## Session Contract

- Refuse concurrent RetroArch processes and hold a host-local runtime lock.
- Keep control serial over RetroArch stdin; do not create a daemon or listener.
- Gate startup on `PING OK`.
- Keep saves, options, logs, captures, and process metadata bundle-local.
- Record an explicit session-end reason.
- Bound interactive sessions with a TTL and terminate the process group on
  teardown.

Local pseudo-commands such as `WAIT_COMMAND_READY`,
`WAIT_STATUS_FRAME`, memory waits, and memory snapshots are adapter
operations; they are not frontend commands.

## Deterministic Control

- Pause before deterministic play and step frames explicitly.
- Use frame-counted input for replayable paths; wall-clock holds are
  exploratory.
- Pair `SAVE_STATE` with `WAIT_SAVE_STATE`.
- Pair asynchronous state loading with `WAIT_LOAD_STATE` before
  treating the state as resident.
- Do not capture until at least one presented frame exists after a paused load.

The interactive `load-slot` helper has not yet adopted the
`WAIT_LOAD_STATE` barrier; [IL-19](../../docs/ISSUE_LOG.md#il-19-complete-load-slot-with-wait_load_state)
tracks that bounded follow-up.

## Portable Inputs

Supply the RetroArch binary, core, ROM, state source, pack, output directory,
and optional video-context driver through existing arguments or environment
variables. Do not add hostnames or personal checkout roots as defaults.

For headless Vulkan, set
`RETROARCH_VIDEO_CONTEXT_DRIVER=headless_vk` explicitly. That backend
skips presentation and relies on readback evidence; it must never become an
automatic fallback, and its feature-off baseline is distinct from a headed
baseline.

## Evidence

- Screenshot completion waits for a non-empty, size-stable PNG.
- Uniform-black detection records a warning but does not fail by itself because
  a fade can legitimately be black.
- State and capture operations fail loudly when acknowledgements or expected
  files are absent.
- Adapters record resolved input identities; they do not decide renderer
  correctness.
