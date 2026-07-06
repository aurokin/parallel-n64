# Headless Vulkan eval backend spike

Status: experimental branch `spike/headless-vulkan-eval-backend`.

## Goal

Run tracked ParaLLEl/RetroArch eval scenarios on SR-IOV Vulkan render devices that do not expose a desktop/display connector. This avoids the lavapipe fallback and avoids depending on X11/Wayland DRI3 presentation.

## Prototype shape

This branch pairs with the same branch name in `aurokin/RetroArch`.

RetroArch change:

- adds a Vulkan context driver named `headless_vk`
- creates a `VK_EXT_headless_surface` surface instead of an X11/Wayland/KHR-display surface
- keeps the existing Vulkan swapchain/present path so libretro Vulkan hardware contexts still receive normal WSI handles

parallel-n64 adapter change:

- `tools/adapters/retroarch_stdin_session.sh` now honors `RETROARCH_VIDEO_CONTEXT_DRIVER`
- setting `RETROARCH_VIDEO_CONTEXT_DRIVER=headless_vk` writes `video_context_driver = "headless_vk"` into the per-bundle append config
- adapter provenance records `VIDEO_CONTEXT_DRIVER` in `retroarch.session.env`

## Intended test command

On `saur`/`tortle` after building the matching RetroArch branch:

```bash
cd /home/auro/code/parallel-n64
RETROARCH_PATH=/home/auro/code/RetroArch \
RETROARCH_VIDEO_CONTEXT_DRIVER=headless_vk \
DISPLAY= WAYLAND_DISPLAY= \
tools/scenarios/paper-mario-title-screen.sh --mode off
```

For a hi-res run, use the existing full-cache/selected-package scenario with the same environment override.

## Acceptance criteria

- RetroArch log shows the `headless_vk` context driver.
- Vulkan selects the B50 VF, not llvmpipe/lavapipe.
- No `No DRI3 support detected` failure.
- Adapter commands still complete: `PING`, `GET_STATUS`, `STEP_FRAME`, `SCREENSHOT`, `QUIT`.
- Evidence bundle includes one capture plus existing semantic/runtime evidence.

## Known risks

- Some drivers may expose `VK_EXT_headless_surface` but not support the swapchain path needed by RetroArch.
- If RetroArch screenshots require a native window rather than Vulkan readback, capture may need a second patch.
- This is a reviewable spike, not yet the default eval path.
