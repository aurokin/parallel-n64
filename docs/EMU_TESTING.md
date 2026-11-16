# Emulator Test Profiles

Use `./run-tests.sh --profile <name>`. The script configures and
incrementally builds `build/ctest` unless another build directory is
provided.

## Prerequisites

- CMake 3.16 or newer and `make`.
- A C and C++ compiler.
- zlib development libraries; the Vulkan core and texture-replacement
  provider test link `z`. No OpenGL development library is required.
- `python3` on `PATH` with Pillow. `tools/hires_pack_materialize_package.py`
  imports `PIL`, and the `hts2phrb` support tests in `emu-required` fail
  without it.

## Profiles

- `emu-harness`: fake-frontend adapter load, lifecycle and configuration tests.
  These exercise harness behavior separately from the required product gate.
- `emu-required`: display-free unit, support, and static conformance checks.
  This is the required change gate.
- `emu-runtime-conformance`: lavapipe smoke. It enables runtime opt-in and rebuilds the live core.
- `emu-conformance-roms`: public CPU, COP0, COP1, exception and RSP test
  ROMs on each CPU engine ([Conformance Test ROMs](#conformance-test-roms)).
  It enables runtime opt-in and rebuilds the live core.
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
- converter and scenario runner contracts;
- GlideN64 oracle guardrails;
- static VI and RDP conformance checks;
- conformance-ROM decoder, manifest and runner contracts.

It does not launch an emulator or prove runtime rendering.

## Runtime Qualification

Use the [scenario runner pre-merge lanes](../tools/regression/README.md).
Hi-res qualification requires loaded entries and draw-time replacement hits.
The optional `emu-runtime-conformance` profile retains only the lavapipe smoke.
Runtime tests require explicit local inputs and exclusive host runtime access.

## Conformance Test ROMs

`emu.conformance.test_roms_krom`, `_dillon` and `_systemtest` run pinned public
test ROMs ([tools/conformance](../tools/conformance/README.md)) on the dynamic
recompiler and both interpreters and read each verdict from the saved end
state: PASS/FAIL cells in the framebuffer, a result register, or the suite's
summary text. ROMs are fetched or built by sha256 into
`assets/conformance-roms/`; none is tracked. Failures the manifest lists as
known, with a diagnosis, are reported without gating; anything else fails,
including a crash, a hang or a session on the wrong engine.

```sh
tools/conformance/fetch_test_roms.py
EMU_CONFORMANCE_RUNTIME_CLASS=<class> RETROARCH_BIN=... RETROARCH_BASE_CONFIG=... \
  ./run-tests.sh --profile emu-conformance-roms
```

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
