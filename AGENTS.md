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
- Parallel scaling capture:
  - `./run-paper-mario-scaling-capture.sh --tag <tag>`
  - uses same-core state-mode capture with HIRES off
  - uses an isolated XDG root with an explicit temp `core-options.cfg`
  - RetroArch should be pointed at that temp file with `global_core_options = true`
- Parallel scaling compare:
  - `./run-paper-mario-scaling-compare.sh --tag <tag>`
- GLide oracle capture:
  - `./run-paper-mario-gliden64-capture.sh --tag <tag>`
- Preserved GLide oracle:
  - `/home/auro/code/parallel-n64-paper-mario-backups/20260306-hires-audit/gliden64/oracle-gliden64-5`
- Preserved GLide 4x HIRES-off scaling oracle:
  - `/home/auro/code/parallel-n64-paper-mario-backups/20260306-hires-audit/scaling/oracle-gliden64-4x-hires-off-2`
  - validated button-path screenshot: `/home/auro/code/parallel-n64-paper-mario-backups/20260306-hires-audit/scaling/oracle-gliden64-4x-hires-off-2/Paper Mario (USA)-260306-212123.png`
- Button-path scene for comparable screenshots:
  - boot
  - wait `20s`
  - press `Start`
  - wait `5s`
  - press `Start`
  - wait `2s`
- Scaling-phase capture note:
  - prefer `./run-paper-mario-scaling-capture.sh` for current `parallel` scaling work
  - do not spend time re-establishing button-path parity for `parallel` unless the state path becomes invalid
  - run captures sequentially; the helpers share RetroArch netcmd defaults and are not meant for parallel launches
  - use `--dump-vi-stages aa,divot,scale,final` when you need one-shot VI stage dumps under `capture_dir/vi-stages`
  - stage dumps are trigger-based and should line up with the helper screenshot timing, not the launch frame
  - on the current 4x state path, identical `scale` and `final` dumps mean the artifact is already present by `scale_stage()`
  - if `accurate` vs `experimental` only diverge at `scale/final` and not `aa/divot`, keep the investigation in `scale_stage()` / `vi_scale.frag`
  - the current experimental VI path already improves the oracle, but it did not solve the main horizontal line / seam artifact
  - the current committed experimental VI path combines a non-linear row-phase-aware Y adjustment (`0/2/7/18`) with an upward-skewed 4-tap vertical footprint (`upper 8/16`, `lower 7/16`), a localized `y_frac` remap for phases `1/2` in the upper source band (`y >> 10 < 640`), and experimental 4x source-domain biases of `x_add -= 17`, `y_add -= 30`, `y_base += 736`, plus an upper-band phase-1 source Y bias of `+384`, a lower-band phase-1 source Y bias of `-512`, and an upper-band phase-3 source Y bias of `+512`; keep that as the committed baseline unless a replacement clearly beats it
  - for local scaling sweeps, you can override those source-domain values at runtime with `PARALLEL_VI_SOURCE_{X,Y}_{ADD,BASE}_BIAS`, and you can probe the per-phase seam controls with `PARALLEL_VI_PHASE1_Y_BIAS`, `PARALLEL_VI_PHASE1_LOWER_Y_BIAS`, `PARALLEL_VI_PHASE3_Y_BIAS`, and `PARALLEL_VI_PHASE3_X_BIAS`
  - row-periodicity analysis on `scale` dumps is useful here, but it is not the only truth signal: the prior best row-phase-only path measured `mod4 0.4867`, `mod8 1.1960`, `mod12 1.7706`, while the current combined path is slightly worse on that metric (`mod4 0.5443`, `mod8 1.2770`, `mod12 1.8259`) but still better on the saved Paper Mario oracle overall
  - when validating this path, expect a small left-side capture variance on repeated runs from the same save-state; the stable regions to trust most are `right`, `top`, `bottom`, `file2_new`, and the `scale` dump itself
  - current clean metrics for that experimental baseline: `full 18.5840`, `left 19.3148`, `right 30.2321`, `top 17.9058`, `bottom 21.0256`, `file2_new 2.9931`
  - the plugin-level `interlacing` flag is now wired into scanout (`blend_previous_frame = true`, `upscale_deinterlacing = false` when enabled), but on the current Paper Mario save-state it only perturbs the left-side variance region and does not explain the main horizontal split
  - the biggest recent improvements came from source-domain setup, not another VI kernel tweak; if future tuning stalls again, inspect the source mapping fed into `scale_stage()` before spending many more cycles on shader kernel variants
- Save-state warning:
  - prefer save states for fast iteration when they were created from the same ROM image and the same core path you are validating
  - same-core save states are valid across HIRES-on and HIRES-off runs
  - do not cross save states between different cores or different ROM revisions
  - avoid save states from `/home/auro/code/paper_mario`
  - they were made from a modified ROM and a different emulator
