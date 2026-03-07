# Emulator Test Tiers

This repo uses tiered, local-only emulator-behavior test gates to separate required checks from heavier optional checks.

## Related Docs

- HIRES behavior and roadmap:
  - `docs/HIRES_BEHAVIOR.md`

## Local Commands

- Required gate (PR-safe):
  - `./run-tests.sh --profile emu-required`
- Optional conformance gate:
  - `./run-tests.sh --profile emu-conformance`
- Optional runtime lavapipe conformance gate:
  - `./run-tests.sh --profile emu-runtime-conformance`
- Optional dump-replay gate (provisions validator if missing):
  - `./run-dump-tests.sh --provision-validator`
- Optional strict dump-composition gate:
  - `./run-dump-tests.sh --strict-composition`
- Optional combined non-required gate:
  - `./run-tests.sh --profile emu-optional`
- Optional TSAN race check tier (local debug):
  - `./run-tests.sh --profile emu-tsan`
- HIRES-readiness safety gate (local):
  - `./run-tests.sh --profile hires-readiness`
- Quick HIRES visual smoke (state-load screenshot capture):
  - `./run-n64-smoke-state.sh -- --verbose`
- Quick runtime smoke:
  - `./run-n64.sh -- --verbose`
- Paper Mario button-path capture on `parallel`:
  - `./run-paper-mario-hires-capture.sh --tag <tag>`
- Paper Mario button-path oracle capture on `GLideN64`:
  - `./run-paper-mario-gliden64-capture.sh --tag <tag>`

## Profiles

- `all`: full CTest run (default)
- `emu-required`: `emu.unit.*`
- `emu-optional`: `emu.conformance.*` + `emu.dump.*`
- `emu-conformance`: `emu.conformance.*`
- `emu-runtime-conformance`: runtime lavapipe conformance (`runtime_smoke_lavapipe` + `lavapipe_frame_hash` + `lavapipe_vi_filters_hash` + `lavapipe_vi_filters_mixed_hash` + `lavapipe_vi_downscale_hash` + `lavapipe_sm64_frame_hash`) with opt-in env automatically set
- `emu-dump`: `emu.dump.*`
- `emu-tsan`: `emu.unit.command_ring_policy` + `emu.unit.worker_thread` with ThreadSanitizer flags
- `hires-readiness`: `hires.texture.*` + `emu.unit.hires_*`

## Triage Flow

1. Re-run the failing tier with output:
   - `./run-tests.sh --profile <profile> -- --output-on-failure`
2. For dump failures, run validator directly:
   - `rdp-validate-dump <dump>.rdp`
   - `rdp-validate-dump <dump>.rdp --sync-only`
3. If only optional tiers fail, keep required tier green and file follow-up with:
   - failing test name
   - ROM/dump used
   - commit SHA
   - platform + Vulkan driver string

## Notes

- `emu.dump.*` is skip-by-default without `rdp-validate-dump`.
- Baseline fixture is committed at `tests/rdp_dumps/baseline_minimal_eof.rdp`.
- Remote CI enforcement is intentionally disabled for now; run tiers locally.
- `emu-tsan` runs a compiler/runtime preflight first; if TSAN is unsupported locally it exits with a clear skip message.
- Set `EMU_TSAN_FORCE=1` to bypass preflight and force TSAN execution.
- Randomized ingest fuzz tests are deterministic by default and log their seed.
- Set `EMU_FUZZ_SEED=<value>` (hex or decimal) to reproduce/override `emu.unit.rdp_command_ingest` fuzz runs.
- `run-tests.sh` profile mapping/guard behavior is locked by `emu.unit.test_runner_profile_contract`.
- `run-dump-tests.sh` CLI/env handoff behavior is locked by `emu.unit.dump_runner_contract`.
- `run-build.sh` CLI/env handoff behavior is locked by `emu.unit.build_runner_contract`.
- `run-build.sh` auto-cleans when effective build flags change; set `RUN_BUILD_AUTO_CLEAN=0` to disable.
- `run-n64.sh` runtime launch contract behavior is locked by `emu.unit.run_n64_contract`.
- HIRES mini-pack tooling contract is covered by `hires.texture_minipack_tool` (`tools/hires_minipack.py` end-to-end generation + provider decode).
- HIRES work must preserve native behavior when HIRES is disabled; any disabled-path regression is a blocker.
- The former non-HIRES emulator test-roadmap plan (`EMULATOR_TEST_TASKS.md`) was closed and retired on 2026-03-05 after `T0`..`T10` completion.

## Paper Mario HIRES Workflow

- Writable code lives in `/home/auro/code/parallel-n64`.
- Read-only references:
  - `/home/auro/code/mupen/parallel-n64-upstream`
  - `/home/auro/code/mupen/parallel-rdp-upstream`
  - `/home/auro/code/mupen/RetroArch-upstream`
  - `/home/auro/code/mupen/GLideN64-upstream`
  - `/home/auro/code/paper_mario/papermariodx`
  - `/home/auro/code/paper_mario/PAPER MARIO_HIRESTEXTURES.hts`
- Default debugging path:
  - use `parallel` first
  - use `GLideN64` only as an oracle or after major scene changes
- Comparable Paper Mario scene:
  1. boot
  2. wait `20s`
  3. press `Start`
  4. wait `5s`
  5. press `Start`
  6. wait `2s`
- `parallel` capture helper:
  - builds an isolated RetroArch config root under the capture dir
  - stages a temp ParaLLEl core options file and temporarily installs it at the real home core-options path for the run
  - drives the button path with the virtual pad
  - captures a RetroArch screenshot into the temp capture dir
- `parallel` scaling helper:
  - `./run-paper-mario-scaling-capture.sh --tag <tag>`
  - uses the same-core save state path for faster, stable scaling iteration
  - forces HIRES off, `4x` upscaling, and `disable` downscaling
- `parallel` scaling compare helper:
  - `./run-paper-mario-scaling-compare.sh --tag <tag>`
  - or `./run-paper-mario-scaling-compare.sh --candidate /path/to/capture.png`
  - aligns the capture to the preserved GLide scaling oracle
  - writes summary metrics and visual diffs under `/tmp/parallel-n64-paper-mario-scaling-compare/<tag-or-capture-name>`
- `GLideN64` capture helper:
  - stages the real RetroArch `Mupen64Plus-Next.opt`
  - with `--hires-on`, stages `PAPER MARIO_HIRESTEXTURES.hts` at `system/Mupen64plus/cache`
  - with `--hires-off`, skips hi-res pack staging for native-scaling captures
  - drives the same button path and captures a RetroArch screenshot
- Preserved GLide oracle:
  - `/home/auro/code/parallel-n64-paper-mario-backups/20260306-hires-audit/gliden64/oracle-gliden64-5`
- Most recent validated oracle screenshot:
  - `/home/auro/code/parallel-n64-paper-mario-backups/20260306-hires-audit/gliden64/oracle-gliden64-5/Paper Mario (USA)-260306-151242.png`
- Preserved GLide 4x HIRES-off scaling oracle:
  - `/home/auro/code/parallel-n64-paper-mario-backups/20260306-hires-audit/scaling/oracle-gliden64-4x-hires-off-2`
- Validated button-path scaling oracle screenshot:
  - `/home/auro/code/parallel-n64-paper-mario-backups/20260306-hires-audit/scaling/oracle-gliden64-4x-hires-off-2/Paper Mario (USA)-260306-212123.png`
- Matching `parallel` 4x HIRES-off reference:
  - `/home/auro/code/parallel-n64-paper-mario-backups/20260306-hires-audit/scaling/reference-parallel-4x-hires-off-1`
- Full-frame 4x HIRES-off comparison image:
  - `/home/auro/code/parallel-n64-paper-mario-backups/20260306-hires-audit/scaling/paper-mario-glide-vs-parallel-4x-hires-off-full.png`
- Current divergence to focus on:
  - `parallel` still differs from the oracle on Paper Mario file-select stage/frame scaling and minification
- Current scaling-phase capture policy:
  - use `./run-paper-mario-scaling-capture.sh` for `parallel`
  - keep the preserved GLide button-path screenshot as the oracle
- Save-state caution:
  - prefer save states when they match the same ROM image and the same core path under test; they are the fastest way to iterate on a stable scene
  - same-core save states are valid across HIRES-on and HIRES-off runs
  - do not cross save states between different cores or different ROM variants
  - do not use save states from `/home/auro/code/paper_mario`
  - they were created from a modified ROM and a different emulator
