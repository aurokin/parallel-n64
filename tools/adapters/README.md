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

The eval-specific vision shim now belongs to the external eval harness.
Eval exports copy it from there.
Pinned historical exporters may retain a local compatibility copy; it is not
part of this product tree.

Use each entrypoint's `--help` output for its current options.

The macOS build helper selects the Xcode macOS SDK unless `SDKROOT` is supplied.
If the installed FFmpeg release is incompatible with the selected RetroArch
source, set `FFMPEG_PREFIX` to a compatible installation. For example,
`FFMPEG_PREFIX="$(brew --prefix ffmpeg@7)"` selects that versioned keg's headers,
libraries and pkg-config metadata while preserving the system's active FFmpeg.
Run the helper without `deploy` to keep the build separate from installed apps.

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

The interactive `load-slot` helper checks frontend command support, waits for
`WAIT_LOAD_STATE DONE`, and verifies the load log before reporting success.
The barrier confirms task completion; the log establishes whether the state
loaded. See [IL-19](../../docs/ISSUE_LOG.md#il-19-complete-load-slot-with-wait_load_state).

## Portable Inputs

Supply the RetroArch binary, core, ROM, state source, pack, output directory,
and optional video-context driver through existing arguments or environment
variables. Do not add hostnames or personal checkout roots as defaults.

Both session adapters require a caller-selected base configuration through
`--base-config PATH` or `RETROARCH_BASE_CONFIG`; the argument takes precedence.
For the frontend-owned macOS MoltenVK profile, pass an explicit path to
`profiles/agent-control-macos.cfg` in your RetroArch checkout, for example:

```sh
RETROARCH_BASE_CONFIG="$RETROARCH_SOURCE/profiles/agent-control-macos.cfg" \
  tools/adapters/retroarch_stdin_session.sh \
  --bundle-dir "$BUNDLE_DIR" --retroarch-bin "$RETROARCH_BIN" \
  --core "$CORE_PATH" --rom "$ROM_PATH" --command QUIT
```

The profile stays in the frontend repository. Select it explicitly; the
adapters do not infer MoltenVK argument-buffer settings from the executable
name or hi-res mode. They preserve `MVK_CONFIG_USE_METAL_ARGUMENT_BUFFERS`
exactly as supplied by the caller, including an empty value; when absent it
stays absent so frontend configuration can apply. On macOS, feature-off
sessions retain the `PARALLEL_RDP_DISABLE_HIRES_SHADER=1` default unless the
caller supplies an override. Existing Linux launch settings remain unchanged.

`--extra-append-config PATH` copies the supplied file to
`BUNDLE/retroarch.extra.append.cfg`, records its SHA-256 in
`retroarch.session.env`, and loads that snapshot after the generated
`retroarch.append.cfg`. RetroArch receives one quoted argument:
`--appendconfig "generated.cfg|extra.cfg"`. Later files override earlier files,
so the extra config can override generated settings and the generated config
can override the base. Within a single file, RetroArch uses the first occurrence
of a key; concatenating extra lines onto the generated file would not provide
those overrides. Repeated `--appendconfig` flags replace the preceding argument,
and colons are ordinary path characters. Bundle and extra-config paths containing
`|` are rejected before staging because that character separates append files.
The source configs remain caller-owned; the snapshot preserves the extra input
for reproduction after the source changes. Copied/exported adapters need only
their explicit runtime inputs, with no workspace dependency.

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
