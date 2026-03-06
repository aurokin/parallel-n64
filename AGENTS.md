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
- Keep CI local-only unless the user explicitly asks to add remote CI.

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
- ROMs: `/home/auro/code/n64_roms`

## Testing Docs
- Emulator test tiers: `docs/EMU_TESTING.md`
- HIRES status and open work: `docs/HIRES_TEXTURE_TASKS.md`
