# Workspace Paths

Machine-specific workspace layout (last refreshed 2026-07-06). This file is the
source of truth for local path assumptions; status detail defers to
[assets/TEXTURE_PACKS.md](/home/auro/code/parallel-n64/assets/TEXTURE_PACKS.md) and
[PROJECT_NOTES.md](/home/auro/code/parallel-n64/PROJECT_NOTES.md).

## Primary Repo

- `parallel-n64`: `/home/auro/code/parallel-n64`
  Branch for the reboot effort: `parallelish-reboot`. GitHub Actions are disabled on the fork.

## RetroArch (agent control stack)

- Checkout: `/home/auro/code/RetroArch`, branch `agent-control`, binary at
  `/home/auro/code/RetroArch/retroarch`.
- The branch carries 7 custom stdin commands (`PING`, `SET_PAUSE`, `STEP_FRAME`,
  `SET_INPUT_PORT`/`CLEAR_INPUT_PORT`/`GET_INPUT_PORT`, `LOAD_STATE_SLOT_PAUSED`,
  `WAIT_SAVE_STATE`, `READ_CORE_MEMORY` with system-RAM fallback).
- Canonical backup of those patches:
  [tools/retroarch-patches/](/home/auro/code/parallel-n64/tools/retroarch-patches)
  (committed here, also pushed to `aurokin/RetroArch`). See its README for rebuild
  and verification steps.

### metapod macOS RetroArch

- Checkout: `/Users/auro/code/RetroArch`, branch `agent-control`; the worktree is
  intentionally dirty with macOS/agent-control changes.
- Source app: `/Applications/RetroArch.app`.
- Tracked macOS runtime target: `/Users/auro/code/parallel-n64/artifacts/external/RetroArch-MVK141.app`.
  Build or refresh it with
  `/Users/auro/code/parallel-n64/tools/adapters/prepare_retroarch_mvk141_app.sh --force`.
- The prepared app copy replaces only the app-local `MoltenVK.framework` with
  MoltenVK 1.4.1 and is ad-hoc signed. Runtime adapters prefer it on macOS when
  it exists; pass `--retroarch-bin /Applications/RetroArch.app/Contents/MacOS/RetroArch`
  only for explicit vanilla-bundle negative tests.
- Paper Mario hi-res validation on this host requires
  `MVK_CONFIG_USE_METAL_ARGUMENT_BUFFERS=1` and the prepared MoltenVK 1.4.1 app.
  The stock `/Applications/RetroArch.app` bundle carries MoltenVK 1.2.8 and is
  not a valid hi-res Metal argument-buffer path.
- `--mode on` sessions must also export a staged `.phrb` path, for example:
  `PARALLEL_RDP_HIRES_CACHE_PATH=/Users/auro/code/parallel-n64/artifacts/hts2phrb-review/local-pm64-exact-variant-set/package.phrb`.
  Without that package env, the core can enable the hi-res option but render
  native textures only.

### luma macOS RetroArch (M3 Max laptop; onboarded 2026-07-02)

- Same layout as metapod: `/Users/auro/code/{parallel-n64, papermario (SHALLOW
  clone), parallel-n64-lab, n64-agent-evals, RetroArch@agent-control}`; assets
  (ROM zip, exact-variant-set `.phrb`, MoltenVK 1.4.1 tar) scp'd from
  mander/metapod, sha-pin verified.
- Built with `tools/adapters/build_retroarch_agent_control_macos.sh` + `deploy`.
  Onboarding gotchas, all host-state (not recipe) issues:
  - The pre-existing consumer `/Applications/RetroArch.app` had
    `NSPrincipalClass: RApplication` in Info.plist — our source build does not
    register that class and exits ("Unable to find class: RApplication").
    Both staged apps were set to `NSApplication` (matches metapod).
  - Gatekeeper killed the modified bundle (`Killed: 9`) — quarantine xattrs
    inherited from the consumer install; cleared with `xattr -dr`.
  - `/Applications/RetroArch.app` here has NO app-local MoltenVK.framework
    (the consumer install had no Frameworks dir), so eval-workspace adapter
    copies MUST export
    `RETROARCH_MVK141_BIN=/Users/auro/code/parallel-n64/artifacts/external/RetroArch-MVK141.app/Contents/MacOS/RetroArch`
    (the workspace-relative REPO_ROOT default cannot resolve it). The MVK141
    app itself was transplanted from metapod (tar over scp) and re-signed.
  - `brew install molten-vk` is required for the Khronos-loader ICD path
    (`/opt/homebrew/etc/vulkan/icd.d/MoltenVK_icd.json`); `vulkan-loader`,
    `vulkan-headers`, `pkg-config` also installed.
  - Non-login ssh PATH lacks `~/.local/bin` (agent CLIs) and
    `/opt/homebrew/bin`; the eval driver exports both.
  - Laptop: AC profile never sleeps, battery profile sleeps in 1 minute —
    keep docked for runs; the driver wraps `run_eval.sh` in `caffeinate -is`.
  - cursor-agent stores auth in the login keychain, which is locked in ssh
    sessions — `security unlock-keychain` needed before cursor runs.
- Eval driver: `/tmp/luma-eval-run.sh` (volatile — recreate from this doc or
  the metapod driver pattern after reboot).

### saur / tortle headless Linux VMs (Intel Arc Pro B50 SR-IOV; onboarded 2026-07-06)

- Two Ubuntu VMs on the Proxmox host `Bront` (same host as mander), each holding
  half of an Intel Arc Pro B50 as an SR-IOV VF. The VF is render-capable with NO
  display connector: `/dev/dri/card0` is virtio (desktop), `card1`/`renderD129`
  is the B50 VF. Vulkan enumerates "Intel(R) Arc(tm) Pro B50 Graphics (BMG G21)"
  plus llvmpipe.
- Same fleet layout as mander: `~/code/{parallel-n64@parallelish-reboot,
  RetroArch@agent-control, n64-agent-evals, papermario, parallel-n64-lab,
  n64_docs}`; ROM + authoritative states staged in `assets/`.
- Headless operation: export `RETROARCH_VIDEO_CONTEXT_DRIVER=headless_vk`
  (both adapters honor it; writes `video_context_driver` + null input drivers
  into the per-bundle append config and records `VIDEO_CONTEXT_DRIVER` in
  session provenance). The `headless_vk` context is explicit-selection-only
  (ordered after `gfx_ctx_null`) and skips present (Mesa/Intel crashes in
  `vkQueuePresentKHR` on the VF); evidence comes from readback/screenshot.
  See `docs/history/headless-vulkan-eval-backend-spike.md` and RetroArch
  `docs/headless-vulkan-context.md`.
- Feature-off screenshot digests differ from the headed mander baseline but are
  byte-identical across the two hosts; the headless baseline is minted as
  `EXPECTED_SCREENSHOT_SHA256_OFF_HEADLESS` (title-screen scenario; B50-VF scope).
- Onboarding gotchas, all host-state: dev packages missing relative to the
  configured RetroArch feature set (`libfreetype-dev`, `libx11-xcb-dev`,
  `liblzma-dev` installed 2026-07-06); linuxbrew is on PATH and its binutils
  `ld` does the linking — system `-dev` packages must be present for it to
  resolve system libs. 2 vCPUs each: builds are slow (`make -j2`).
- Verified 2026-07-06 on both hosts: title-screen scenario headless end-to-end
  (B50 VF selected, semantic verification passed, strict digest gate green on
  saur) and the interactive adapter's eval command surface (paused start, step,
  save/load, screenshot) headless on saur.

## GlideN64 Reference Vehicle

- Prebuilt core: `/home/auro/code/cores/mupen64plus_next_libretro.so`
  (mupen64plus-next nightly; verified it reads the modern `.hts` pack).
- Pack symlinked at `~/.config/retroarch/system/Mupen64plus/cache/`.
- Core options (prefix `mupen64plus-`): `rdp-plugin=gliden64`, `txHiresEnable=True`,
  `EnableEnhancedHighResStorage=True`, `txCacheCompression=True`,
  `txHiresFullAlphaChannel=True`.
- Source clone for patched builds: `/home/auro/code/mupen64plus-libretro-nx`.
  The env-gated txDump patch (`GLN64_TXDUMP=1`) is committed at
  [tools/gliden64-txdump-patch/](/home/auro/code/parallel-n64/tools/gliden64-txdump-patch)
  (base commit `98c1b0d`); rebuild steps in its README.

## Game References

- `papermario`: `/home/auro/code/papermario`
  Upstream pmret Paper Mario decomp. This replaces the old `papermario-dx` checkout,
  which is no longer on disk. Debug-only semantic reference, not a correctness authority.
- `n64_docs`: `/home/auro/code/n64_docs`
  Local N64 hardware/programming documentation set.
- The `emulator_references`, `oot`, and `sm64` checkouts referenced by older notes are
  not currently on disk; re-clone if needed.

## ROMs

- ROM root: `/pluto/game/rom/n64` (full No-Intro set, NFS mount).
- Staged working ROMs (untracked): `assets/Paper Mario (USA).zip` plus the SM64,
  OoT, MK64, and MM (USA) working copies for the breadth lanes.
- Never point runtime scenarios at the NFS mount; stage working copies in `assets/`.

## Texture Packs And Runtime Packages

- Tracking file (committed): [assets/TEXTURE_PACKS.md](/home/auro/code/parallel-n64/assets/TEXTURE_PACKS.md)
  — status, source URL, and sha256 for every pack.
- On disk now: all five packs — `assets/PAPER MARIO_HIRESTEXTURES.hts` (Paper Mario
  Redone HD) plus its original archive, and the SM64/OoT/MK64/MM Reloaded packs
  under `assets/packs/<name>-hts/`. Status, sources, and hashes in
  `assets/TEXTURE_PACKS.md`.
- Cold-storage convention: archive original downloads and extracted `.hts` files to
  `/pluto/game/texture_packs/n64/` before use. NOTE: `/pluto` is currently mounted
  read-only on this box; originals sit on `koopa:/Users/auro/Downloads/` as interim
  cold storage (see `assets/TEXTURE_PACKS.md`), with `/pluto` the eventual destination.
- Runtime packages (untracked, regenerable): fixtures pin
  `artifacts/hts2phrb-review/local-pm64-zero-config/package.phrb`, while live
  scenario tooling prefers `local-pm64-exact-variant-set` first; boot-validated
  zero-config packages also exist for sm64/oot/mk64/mm under
  `artifacts/hts2phrb-review/`. See the PHRB table in `assets/TEXTURE_PACKS.md`.

## Local Assets And Generated Output

- `assets/` and `artifacts/` binaries are untracked and expendable:
  - `.phrb` packages are regenerable from their `.hts` via `tools/hts2phrb.py`.
  - packs and ROMs are re-acquirable/re-stageable per `assets/TEXTURE_PACKS.md`.
  - the `.hts` originals are the assets to protect (cold storage rule above).
- Savestates live under `assets/states/<fixture>/ParaLLEl N64/` (untracked). The
  authoritative ladder (title -> file select -> `kmr_03 ENTRY_5`) was reminted and
  promoted on this machine on 2026-06-10 (REBOOT_PLAN step 1, DONE; details in
  PROJECT_NOTES.md). Note RetroArch writes 0-byte states if a state-source dir
  lacks the `ParaLLEl N64` core subdir.

## Maintenance Rule

When a new external repo, local corpus, or machine-specific dependency becomes part
of the workflow, add it here before relying on it in plans or tooling. Keep scripts'
path assumptions explicit and easy to override.
