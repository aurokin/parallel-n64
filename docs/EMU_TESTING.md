# Emulator Test Profiles

Use `./run-tests.sh --profile <name>`. The script configures and
incrementally builds `build/ctest` unless another build directory is
provided.

## Profiles

- `emu-required`: display-free unit, support, and static conformance checks.
  This is the required change gate.
- `emu-runtime-conformance`: lavapipe smoke plus the Paper Mario
  authority lane. It enables runtime opt-in and rebuilds the live core.
- `emu-conformance`: every conformance test, including opt-in lanes.
- `emu-tsan`: ThreadSanitizer coverage for the command ring and worker
  thread, with a capability preflight.
- `all`: the full registry; runtime tests skip unless explicitly
  enabled.

Useful forms:

```sh
./run-tests.sh -R 'emu.unit.<name>'
./run-tests.sh --clean --profile emu-required
./run-tests.sh --list --profile emu-required
```

## Required Gate

The required profile covers:

- renderer-policy and data-structure unit tests;
- fixture, converter, and capability shell contracts;
- GlideN64 oracle guardrails;
- static VI and RDP conformance checks.

It does not launch an emulator or prove runtime rendering.

## Runtime Lane

`emu.conformance.paper_mario_full_cache_phrb_authorities` runs the
title-screen, file-select, and `kmr_03 ENTRY_5` authorities. It asserts:

- at least one hi-res entry loaded;
- draw-time replacement traffic exists;
- the source mode is `phrb-only`;
- fallbacks carry explicit reasons;
- the bundle records ROM, state, pack, and configuration identity.

The wrapper's observed default is
`artifacts/hts2phrb-review/local-pm64-exact-variant-set/package.phrb`.
Override it with `EMU_RUNTIME_PM64_FULL_CACHE_PHRB`. Fixture manifests
still name the zero-config package as their declared input; scenario runners
record the package actually selected at runtime.

The lane never uses an on-path capture digest or exact metadata count as a
correctness gate. Digests are limited to feature-off parity and authority
remint verification.

## Skip And Failure Policy

Clean skips are limited to deliberate opt-outs or unavailable optional
capabilities:

- runtime tests without `EMU_ENABLE_RUNTIME_CONFORMANCE=1`;
- lavapipe smoke without a lavapipe ICD;
- full-cache converter coverage without the local Paper Mario source cache;
- TSAN when the preflight proves it unsupported;
- on-demand breadth lanes with unstaged inputs.

Once a gating runtime lane is opted in, missing ROMs, packs, states, core, or
frontend are failures with staging guidance.

## Compatibility Breadth

SM64 and OoT boot/title lanes are registered but excluded from gating profiles.
Run them explicitly when their inputs are staged:

```sh
EMU_ENABLE_RUNTIME_CONFORMANCE=1 ./run-tests.sh \
  -R 'emu.conformance.(sm64|oot)_hires_(boot|title_fixture)'
```

MK64 and MM remain manual breadth checks unless a behavior-backed test is
added.

## Runtime Constraints

- Run at 4x internal scale.
- Run one emulator-facing test at a time.
- Do not launch while another RetroArch process owns the runtime path.
- Keep save data and evidence bundle-local.
- Prefer hi-res-on for normal renderer validation; use feature-off for the
  protected baseline and explicit controls.

## Triage

1. Re-run the narrow failing test with `./run-tests.sh -R '<name>'`.
2. For runtime failures, inspect the validation summary, RetroArch log, and
   hi-res evidence before re-running.
3. Reproduce fuzz failures with the logged `EMU_FUZZ_SEED`.
