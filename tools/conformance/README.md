# Conformance Test ROMs

Public N64 test ROMs that check CPU, COP0, COP1, exception and RSP behavior
against hardware results. They complement the game scenarios in
[`tools/regression`](../regression/README.md): a CP0 or interpreter regression
shows up here as a named failing test instead of a changed game end state.

`run_test_roms.py` runs each ROM in a fresh paused frontend session through the
regression runner's adapter path (power on, step a fixed number of frames,
capture, save the end state) and reads the verdict from the saved state with a
suite decoder. No screenshot comparison enters a gate. It is emulator-facing:
run it serially on a host with no other RetroArch process.

## Suites

| Suite | Source and pin | License | In this repository |
|---|---|---|---|
| `krom` | [PeterLemon/N64](https://github.com/PeterLemon/N64) `7085543e` | Unlicense; each ROM embeds Nintendo's CIC-6102 IPL3 | fetched by sha256, not committed |
| `dillon` | [Dillonb/n64-tests](https://github.com/Dillonb/n64-tests) release `latest` (2024-10-13) | none declared; ROMs embed Nintendo's IPL3 | fetched by sha256, not committed |
| `systemtest` | [thelemmy/nemu64-test](https://github.com/thelemmy/nemu64-test) `9a8b9f7d` (n64-systemtest 3.0.0, base features) | MIT; libdragon's open-source IPL3 | built reproducibly from source, not committed |

[`test_roms.json`](test_roms.json) pins every ROM by sha256 and records each
suite's source, license and redistribution decision. ROMs stay local inputs,
like every ROM this repository uses.

How each suite reports, and what the decoder reads
([`decoders.py`](decoders.py)):

- `krom` draws 8x8 `PASS` (white on green) and `FAIL` (white on red) cells
  into a 32bpp framebuffer with the CPU. The decoder finds the cells in RDRAM
  at the VI origin and width, pixel-exact, and compares the PASS and FAIL
  counts with `reference_cells`: the cells decoded from the screenshot
  committed next to each ROM upstream. Counts rather than positions, because
  some screenshots sit a pixel or two off the framebuffer grid. Fewer PASS
  cells than the reference is an unfinished test, which fails.
- `dillon` leaves `r30 = -1` when every case passed and the 1-based failing
  case number otherwise; the decoder reads the saved GPR.
- `systemtest` formats `Finished in ...s. Base: Failed F of N tests` and a
  `Test '<name>' failed` line per failure into heap memory; the decoder scans
  RDRAM for them.

Not included: krom ROMs without PASS/FAIL cells (`FPUCompare`, `COP1FullMode`,
`LL_LLD_SC_SCD`, the `DMAAlignment` ROMs, `RDRAMTest`, `RSPXBUSRDP`) and the
two NTSC timing ROMs, which have no reference screenshot.

## Staging

```sh
tools/conformance/fetch_test_roms.py                 # krom and dillon, about 190 MB
tools/conformance/build_n64_systemtest.sh            # needs rustup; add --install-toolchain
```

Both write to `assets/conformance-roms/<suite>/` (ignored by git); pass
`--dest DIR` to stage elsewhere. A download or build whose sha256 differs from
the manifest is an error. The systemtest build pins its nightly toolchain and
nust64 version and remaps embedded source paths, so the ROM is identical on
any host.

## Running

```sh
EMU_CONFORMANCE_ROMS=assets/conformance-roms \
RETROARCH_BIN=... RETROARCH_BASE_CONFIG=... RETROARCH_EXTRA_APPEND_CONFIG=... \
EMU_CONFORMANCE_RUNTIME_CLASS=linux-radv-headless-vk-4x \
  ./run-tests.sh --profile emu-conformance-roms
```

The profile rebuilds the live core and runs `emu.conformance.test_roms_krom`,
`_dillon` and `_systemtest`, each on the three CPU engines
(`EMU_CONFORMANCE_CPUCORES` narrows them). A suite with no staged ROM skips; a
partially staged suite fails after running what is staged. Reports go to `artifacts/conformance-roms/<suite>-<time>/<engine>/`.

One engine directly:

```sh
tools/conformance/run_test_roms.py --roms assets/conformance-roms \
  --core parallel_n64_libretro.so --retroarch-bin "$RETROARCH_BIN" \
  --base-config "$RETROARCH_BASE_CONFIG" --runtime-class linux-radv-headless-vk-4x \
  --cpucore cached_interpreter --select suite=krom --out artifacts/conformance-roms/krom-cached
```

`--select` takes an id glob or `suite=a,b`. Each session sets
`PARALLEL_N64_CPUCORE_OVERRIDE`, and the run checks the core log's
`Starting R4300 emulator:` line, so a session on the wrong engine fails.

## Outcomes

| Outcome | Meaning | Gates |
|---|---|---|
| `pass` | the suite's pass evidence was found | no |
| `fail` | failing evidence, an unfinished test, a crash or hang, or the wrong engine | yes |
| `known-fail` | a failure `known_failures` lists for this engine | no |
| `fixed` | a listed known failure now passes; remove its entry | no |
| skipped | the ROM is not staged | no, and never counted as a pass |

A `known_failures` entry names the test, the engines (`"*"` for all) and a
diagnosis. For `systemtest`, `failing_tests` lists the failing test names it
covers, so a new failure in the same ROM still gates. Passing and known-failing
bundles are deleted; failing bundles keep the capture, end state and logs.

## Updating a pin

Change the commit or URL in the suite's `fetch` entry, stage the new files,
and replace each test's `sha256`. For `krom`, regenerate `reference_cells`
by running `decoders.find_krom_cells` over the reference screenshot's RGBA
bytes. Re-run all three engines and update `known_failures` with diagnoses.
