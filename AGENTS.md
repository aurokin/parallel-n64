# Agent Instructions

## Build System
- No package manager.
- Build with `./run-build.sh` or `make HAVE_PARALLEL=1 HAVE_PARALLEL_RSP=1`.
- Run tests with `./run-tests.sh`.

## Commit Attribution
- AI commits MUST include:
```text
Co-Authored-By: Codex <noreply@openai.com>
```

## Key Conventions
- Keep `parallel` as the active graphics path for local validation.
- Treat HIRES-off behavior as invariant: no provider/registry/shader replacement path changes when HIRES is disabled.
- Prefer local helpers over ad hoc commands:
  - `./run-build.sh`
  - `./run-tests.sh`
  - `./run-n64.sh`
  - `./run-n64-smoke-state.sh`
  - `./run-paper-mario-hires-capture.sh`
  - `./run-paper-mario-gliden64-capture.sh`
- Keep CI local-only unless the user explicitly asks to add remote CI.
- Keep the writable target inside this repo only.
- Revert dead-end local experiments before compacting or handing off.

## Local Repos
- Current fork: `/home/auro/code/parallel-n64`
  - `origin`: `git@github.com:aurokin/parallel-n64.git`
  - `upstream`: `git@github.com:libretro/parallel-n64.git`
- Read-only upstream parallel-n64 reference: `/home/auro/code/mupen/parallel-n64-upstream`
  - branch: `master`
  - remote: `https://github.com/libretro/parallel-n64.git`
- Read-only upstream paraLLEl-RDP reference: `/home/auro/code/mupen/parallel-rdp-upstream`
  - branch: `master`
  - remote: `https://github.com/Themaister/parallel-rdp.git`
- Read-only RetroArch reference: `/home/auro/code/mupen/RetroArch-upstream`
  - branch: `fix/android-adreno8xx-gl2-texstorage-guard`
  - remotes: `origin=https://github.com/libretro/RetroArch.git`, `aurokin=git@github.com:aurokin/RetroArch.git`
- Read-only GLideN64 reference: `/home/auro/code/mupen/GLideN64-upstream`
  - branch: `master`
  - remote: `https://github.com/gonetz/GLideN64.git`
- Read-only Paper Mario decomp/reference: `/home/auro/code/paper_mario/papermariodx`
- Read-only Paper Mario pack source: `/home/auro/code/paper_mario/PAPER MARIO_HIRESTEXTURES.hts`
- ROMs: `/home/auro/code/n64_roms`

## Testing Docs
- Emulator test tiers: `docs/EMU_TESTING.md`
- HIRES behavior and roadmap: `docs/HIRES_BEHAVIOR.md`

## Paper Mario HIRES Loop
- Primary path: `parallel`
- Oracle path: `GLideN64`
- Parallel capture:
  - `./run-paper-mario-hires-capture.sh --tag <tag>`
- GLide oracle capture:
  - `./run-paper-mario-gliden64-capture.sh --tag <tag>`
- Preserved GLide oracle:
  - `/home/auro/code/parallel-n64-paper-mario-backups/20260306-hires-audit/gliden64/oracle-gliden64-5`
- Button-path scene for comparable screenshots:
  - boot
  - wait `20s`
  - press `Start`
  - wait `5s`
  - press `Start`
  - wait `2s`
- Save-state warning:
  - prefer save states for fast iteration when they were created from the same ROM image and the same core path you are validating
  - same-core save states are valid across HIRES-on and HIRES-off runs
  - do not cross save states between different cores or different ROM revisions
  - avoid save states from `/home/auro/code/paper_mario`
  - they were made from a modified ROM and a different emulator
