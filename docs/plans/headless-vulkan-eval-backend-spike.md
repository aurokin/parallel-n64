# Headless Vulkan eval backend spike

Status: experimental branch `spike/headless-vulkan-eval-backend`. No merge is planned at this time.

## Goal

Run tracked ParaLLEl/RetroArch eval scenarios on SR-IOV Vulkan render devices that do not expose a desktop/display connector. This avoids the lavapipe fallback and avoids depending on X11/Wayland DRI3 presentation.

## Branches

Two RetroArch spike branches exist:

- `aurokin/RetroArch:spike/headless-vulkan-eval-backend`
  - pure headless Vulkan context spike
  - useful for reviewing the context in isolation
- `aurokin/RetroArch:spike/agent-headless-vulkan`
  - based on `origin/agent-control`
  - use this for `parallel-n64` scenario runs because it includes the stdin command protocol (`PING`, `LOAD_STATE_SLOT_PAUSED`, `READ_CORE_MEMORY`, etc.)

The `parallel-n64` side is:

- `aurokin/parallel-n64:spike/headless-vulkan-eval-backend`

## Prototype shape

RetroArch change:

- adds a Vulkan context driver named `headless_vk`
- creates a `VK_EXT_headless_surface` surface instead of an X11/Wayland/KHR-display surface
- deliberately skips `vkQueuePresentKHR` for this context on swap, because Intel/Mesa on the B50 VF can create the headless swapchain but crashes on present
- still submits GPU work and supports readback/screenshot evidence for automated evals

parallel-n64 adapter change:

- `tools/adapters/retroarch_stdin_session.sh` honors `RETROARCH_VIDEO_CONTEXT_DRIVER`
- setting `RETROARCH_VIDEO_CONTEXT_DRIVER=headless_vk` writes `video_context_driver = "headless_vk"` into the per-bundle append config
- headless runs write `input_driver = "null"` and `input_joypad_driver = "null"` to avoid `/dev/input` failures in SSH/non-seat sessions
- adapter provenance records `VIDEO_CONTEXT_DRIVER` in `retroarch.session.env`

## Why skip present?

Initial validation showed the B50 VF successfully reached hardware Vulkan and created a headless swapchain, then crashed in Mesa Intel Vulkan on present:

```text
SIGSEGV in /lib/x86_64-linux-gnu/libvulkan_intel.so
#8  vulkan_present()
#9  gfx_ctx_headless_vk_swap_buffers()
```

For automated evals, visible compositor presentation is unnecessary. The useful artifact is submitted GPU work plus RetroArch screenshot/readback evidence. So the spike intentionally leaves `headless_vk` as a **skip-present** context rather than trying to force `vkQueuePresentKHR` on a driver path that crashes.

## Validated test command

On `saur`, using the RetroArch branch based on `agent-control`:

```bash
cat > /tmp/agent-headless-vk-title.runtime.env <<'EOF'
RETROARCH_BIN="/home/auro/code/_spikes/RetroArch-agent-headless-vk/retroarch"
RETROARCH_BASE_CONFIG="/home/auro/code/_spikes/RetroArch-agent-headless-vk/retroarch.cfg"
CORE_PATH="/home/auro/code/parallel-n64/parallel_n64_libretro.so"
ROM_PATH="/home/auro/code/parallel-n64/assets/Paper Mario (USA).zip"
STARTUP_WAIT="0"
STARTUP_READY_PATTERN="EmuThread: M64CMD_EXECUTE."
POST_LOAD_SETTLE_FRAMES="3"
AUTHORITATIVE_STATE_PATH="/home/auro/code/parallel-n64/assets/states/paper-mario-title-screen/ParaLLEl N64/Paper Mario (USA).state"
EXPECTED_INIT_SYMBOL="state_init_title_screen"
EXPECTED_STEP_SYMBOL="state_step_title_screen"
SAVEFILE_PATH=""
EOF

cd /home/auro/code/_spikes/parallel-n64-headless-vk
DISABLE_SCREENSHOT_VERIFY=1 \
RUNTIME_ENV_OVERRIDE=/tmp/agent-headless-vk-title.runtime.env \
RETROARCH_VIDEO_CONTEXT_DRIVER=headless_vk \
DISPLAY= WAYLAND_DISPLAY= \
tools/scenarios/paper-mario-title-screen.sh \
  --mode off \
  --run \
  --bundle-dir /tmp/agent-headless-vk-title-bundle-nostrict
```

Observed result:

```text
scenario_rc=0
"passed": true
```

Relevant RetroArch log lines:

```text
[Vulkan] Found GPU #0: "Intel(R) Arc(tm) Pro B50 Graphics (BMG G21)".
[Vulkan] Found GPU #1: "llvmpipe (LLVM 20.1.2, 256 bits)".
[Vulkan] Using GPU #0: "Intel(R) Arc(tm) Pro B50 Graphics (BMG G21)".
[Vulkan] Created headless Vulkan surface/swapchain: 640x480.
```

Command proof from the bundle:

```text
WAIT_COMMAND_READY 120      proof=ping-ack
LOAD_STATE_SLOT_PAUSED 0    proof=ack
STEP_FRAME 3                proof=wait-status-frame
WAIT_STATUS_FRAME PAUSED... proof=status-frame
SNAPSHOT_CORE_MEMORY...     proof=read-core-memory
SCREENSHOT                  proof=wait-new-capture
QUIT                        proof=clean-process-exit
```

Capture hash from the validated no-strict run:

```text
3350afcff7a72c0ecaef19401bf4f968780f727937846fcbf834dba0b736d89f
```

## Screenshot baseline note

Strict screenshot verification failed against the headed/X11 baseline:

```text
expected: 42e501afb2548a5067bc034578c5bcebf0bf2a40f612bbcc94972af716ad6ff2
actual:   3350afcff7a72c0ecaef19401bf4f968780f727937846fcbf834dba0b736d89f
```

Semantic verification passed:

```text
init_symbol: state_init_title_screen
step_symbol: state_step_title_screen
cur_game_mode_match: true
```

Treat the headless-B50 screenshot as a distinct rendering baseline. Do not silently replace the existing headed baseline; mint a new baseline only after visual review.

## Acceptance criteria for future runs

- RetroArch log shows the `headless_vk` context driver.
- Vulkan selects the B50 VF, not llvmpipe/lavapipe.
- No `No DRI3 support detected` failure.
- Adapter commands complete: `PING`, `LOAD_STATE_SLOT_PAUSED`, `GET_STATUS`, `STEP_FRAME`, `READ_CORE_MEMORY`, `SCREENSHOT`, `QUIT`.
- Evidence bundle includes one capture plus semantic/runtime evidence.

## Known limitations

- Not a visible desktop presentation path.
- Relies on readback/screenshot evidence rather than compositor presentation.
- Screenshot hashes can differ from headed baselines.
- Branches are intentionally isolated; no merge is planned at this time.
