# Emulator Test Tiers

The test surface was trimmed at the 2026-06-10 reboot to a behavior-backed core.
This file describes the mechanism. Do not pin exact runtime counts or artifact
inventories here; gates are class-level by design (see
[REBOOT_PLAN.md](/home/auro/code/parallel-n64/docs/REBOOT_PLAN.md)).

## Profiles

Run profiles through `./run-tests.sh --profile <name>`:

- `all`: full CTest registry (default).
- `emu-required`: the PR-safe gate. Fast (a few seconds warm) and display-free:
  - `emu.unit.*` — C++ unit tests of renderer policy seams (RDP command ingest,
    frame mapping/fallback, scanout, tile/rect/triangle setup policy, VI scaling,
    hi-res compat-CRC override, texture replacement provider, CI palette policy,
    command ring, worker thread, and similar).
  - `emu.support.*` — functional shell contracts: fixture verification, the hi-res
    capability runtime contract, the GlideN64 reference-rig no-metric guardrails
    (`emu.support.gliden64_reference_guardrails`), and the `hts2phrb` converter
    contracts (smoke, round trip, directory input, all-families, gates, full-cache).
  - the static `emu.conformance.*` C++ checks: VI register contract, VI scanout
    range, VI scaling crop, RDP command fields, RDP command lengths, RDP texture
    load sequence.
- `emu-conformance`: every `emu.conformance.*` test, including the runtime lanes.
- `emu-runtime-conformance`: the display-occupying runtime gate. Runs
  `emu.conformance.runtime_smoke_lavapipe` plus the single Paper Mario lane
  `emu.conformance.paper_mario_full_cache_phrb_authorities`, and sets the
  `EMU_ENABLE_RUNTIME_CONFORMANCE=1` opt-in automatically. It also rebuilds the
  live libretro core before running.
- `emu-tsan`: ThreadSanitizer build of the command-ring and worker-thread unit
  tests, with a preflight that skips cleanly when TSAN is unsupported
  (`EMU_TSAN_FORCE=1` bypasses the preflight).

`run-tests.sh` uses incremental builds; pass `--clean` to wipe the build dir and
force a full rebuild (including a forced libretro core rebuild for the runtime
profile). Use `-R <regex>` for ad hoc selection (not combinable with `--profile`).

## What The Runtime Lane Verifies

`paper_mario_full_cache_phrb_authorities` runs the three Paper Mario authority
fixtures (title screen, file select, `kmr_03 ENTRY_5`) against the local
zero-config PHRB package and asserts class-level semantics only:

- hi-res entries loaded (`entry_count > 0`)
- draw-time replacement traffic (`draw_hits > 0`)
- `source_mode=phrb-only` and `.phrb`-only runtime inputs
- explicit fallback reasons for anything not replaced

It does not compare captures and does not assert exact entry/descriptor counts.
Capture digests remain valid only for feature-off baseline parity and remint
authority verification.

Override the package under test with `EMU_RUNTIME_PM64_FULL_CACHE_PHRB`; keep the
evidence bundles with `EMU_RUNTIME_PM64_FULL_CACHE_BUNDLE_ROOT`.

## Skip Vs. Loud Fail

- The only clean skips are deliberate opt-outs: runtime lanes without
  `EMU_ENABLE_RUNTIME_CONFORMANCE=1`, the lavapipe smoke without a lavapipe ICD,
  converter full-cache contracts without the local Paper Mario `.hts`, and the
  TSAN preflight.
- Once opted in, missing runtime prerequisites (PHRB package, ROM, savestate,
  RetroArch binary, scenario runtime env) FAIL LOUDLY with a staging message that
  says what is missing and how to stage it. This loud-fail guarantee covers the
  gating lanes (the Paper Mario authority lane and the lavapipe smoke); the
  on-demand SM64/OoT breadth lanes instead exit-77 skip with a printed reason when
  their pack/ROM/core/RetroArch is unstaged, even with the opt-in set.

## On-Demand Cross-Game Lanes

The SM64/OoT lanes are registered in ctest but excluded from the gating profiles
(`emu-required`, `emu-runtime-conformance`); the `all` and `emu-conformance`
regexes do match them, where they skip cleanly without the runtime opt-in. Run
them explicitly when their packs/states are staged:

```sh
EMU_ENABLE_RUNTIME_CONFORMANCE=1 ctest --test-dir build/ctest \
  -R 'emu.conformance.(sm64|oot)_hires_(boot|title_fixture)' --output-on-failure
```

They are compat-path breadth checks, not authority gates.

## Runtime Emulator Test Rules

- run emulator-facing tests at `4x` internal scale, one at a time
- they occupy the display; never parallelize them
- standardize runs as fullscreen windows for consistent capture framing
- do not start a tracked runtime scenario while another `retroarch` process runs
  (the adapter enforces a runtime lock)
- tracked scenarios must not depend on global `~/.config/retroarch/saves`

## Triage

1. Re-run the failing tier: `./run-tests.sh --profile <profile> -- --output-on-failure`
2. For runtime lane failures, read the evidence bundle (validation summary,
   RetroArch log, hi-res evidence) before re-running.
3. Fuzzed unit tests log their seed; reproduce with `EMU_FUZZ_SEED=<value>`.
4. Remote CI is intentionally disabled; run tiers locally.
