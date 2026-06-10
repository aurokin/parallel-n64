# Workspace Paths

Machine-specific workspace layout as of the 2026-06-10 reboot. This machine is
essentially fresh; this file is the source of truth for local path assumptions.

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

## GlideN64 Reference Vehicle

- Prebuilt core: `/home/auro/code/cores/mupen64plus_next_libretro.so`
  (mupen64plus-next nightly; verified it reads the modern `.hts` pack).
- Pack symlinked at `~/.config/retroarch/system/Mupen64plus/cache/`.
- Core options (prefix `mupen64plus-`): `rdp-plugin=gliden64`, `txHiresEnable=True`,
  `EnableEnhancedHighResStorage=True`, `txCacheCompression=True`,
  `txHiresFullAlphaChannel=True`.
- Source clone for patched builds: `/home/auro/code/mupen64plus-libretro-nx`
  (txDump support needs a one-line patch there).

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
- Staged working ROM: `assets/Paper Mario (USA).zip` (untracked).
- Never point runtime scenarios at the NFS mount; stage working copies in `assets/`.

## Texture Packs And Runtime Packages

- Tracking file (committed): [assets/TEXTURE_PACKS.md](/home/auro/code/parallel-n64/assets/TEXTURE_PACKS.md)
  — status, source URL, and sha256 for every pack.
- On disk now: `assets/PAPER MARIO_HIRESTEXTURES.hts` (Paper Mario Redone HD) plus its
  original archive. SM64 and OoT packs are NEEDED; the user is acquiring them.
- Cold-storage convention: archive original downloads and extracted `.hts` files to
  `/pluto/game/texture_packs/n64/` before use (create the directory on first archive).
- Active runtime package: `artifacts/hts2phrb-review/local-pm64-zero-config/package.phrb`
  (405MB, zero-config compat, untracked).

## Local Assets And Generated Output

- `assets/` and `artifacts/` binaries are untracked and expendable:
  - `.phrb` packages are regenerable from their `.hts` via `tools/hts2phrb.py`.
  - packs and ROMs are re-acquirable/re-stageable per `assets/TEXTURE_PACKS.md`.
  - the `.hts` originals are the assets to protect (cold storage rule above).
- Savestates live under `assets/states/<fixture>/ParaLLEl N64/` (untracked). The
  authoritative savestate ladder is being reminted on this machine
  (REBOOT_PLAN work order step 1); treat existing state files as unverified until
  the remint promotes them.

## Maintenance Rule

When a new external repo, local corpus, or machine-specific dependency becomes part
of the workflow, add it here before relying on it in plans or tooling. Keep scripts'
path assumptions explicit and easy to override.
