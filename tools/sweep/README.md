# Library boot sweep

Boots every ROM in a library list on a libretro N64 core, captures frames at
fixed points, classifies each run mechanically and makes contact sheets that
a person or a model can scan. Run it once per core (ParaLLEish, upstream
parallel-n64, mupen64plus-next or another profile) and compare the result
directories afterwards. Verdicts are mechanical. `pass` does not mean a title
screen, menu or gameplay was reached; the contact sheets show that.

Each run is one fresh frontend session through the regression runner's adapter
session ([`run_scenarios.py`](../regression/run_scenarios.py)). It uses the
same host lock, stepping, capture and teardown, and only one emulator runs per
host. The tool schedules nothing. To split a library across hosts, run it on
each host with part of the list.

## Commands

```sh
# One core, one result directory. Headless hosts also need
# RETROARCH_VIDEO_CONTEXT_DRIVER=headless_vk (see ../adapters/README.md).
python3 tools/sweep/run.py --roms LIBRARY_DIR_OR_LIST --core CORE.so \
  --profile paralleish --retroarch-bin RETROARCH --runtime-class LABEL \
  --out RESULTS/paralleish

# Compare result directories; LABEL=DIR names a column.
python3 tools/sweep/report.py ours=RESULTS/paralleish next=RESULTS/next \
  --out RESULTS/report [--shed]
```

`--roms` takes a directory, searched recursively for `.z64`, `.v64`, `.n64`
and `.zip` files, or a text file with one path per line (relative paths are
resolved from the file's directory; `#` starts a comment). A zip must contain
exactly one ROM. The ROM sha256 is that of the ROM bytes as supplied, i.e. the
zip member. Other `run.py` options:

- `--option KEY=VALUE` sets a core option on top of the profile, for example
  `--option parallel-n64-cpucore=pure_interpreter`.
- `--input`, `--input-overrides DIR` (see [Input](#input)).
- `--retry VERDICT` reruns complete runs that have that verdict.
- `--limit N` runs at most N ROMs, for smoke tests.
- `--base-config` defaults to a generated `OUT/base.cfg` that turns off
  `config_save_on_exit`.

Before the first session, `run.py` loads the core with `ctypes`
([`core_info.py`](core_info.py)) to read the options it declares. It refuses
to start if a profile option is undeclared or has a value the core does not
offer. `--no-option-check` skips this, and the record's effective options are
then unknown. `run.py` also runs the adapter's `doctor` before each ROM and
stops if the host is not idle. It stops as well if a frontend process
survives teardown.

## Output

`OUT/runs/<rom sha256[:16]>-<core sha256[:12]>-<run config[:12]>/` holds one
run:

- `record.json` holds the verdict and sub-class, the frames planned and
  reached, each capture's frame, sha256 and blank flag, and the exit class,
  error and log flags. It also has the requested and effective core options,
  the core library name and version, the input pattern, timings and the host.
  Its presence marks the run complete.
- `contact.jpg` is one row of capture thumbnails.
- `scenario.json`, `core-options.opt` and `frontend.cfg` are the generated
  inputs.
- `bundles/` is the adapter bundle: captures, logs, savefiles.

The run config hash combines the sweep config hash with the input pattern's
hash. The sweep config hash covers the profile and its options, the shared
frontend settings, the frontend binary's sha256, the base config and the
caller's `RETROARCH_VIDEO_*`/`MVK_*` environment. Paths, the runtime-class
label and the host are recorded but not hashed, so hosts with the same
frontend and settings produce comparable runs. Rerunning with the same core,
ROM and config skips complete runs. A run directory without `record.json` is
moved aside to `<key>.incomplete-<time>` and run again. `OUT/configs/` keeps
each config's full identity and `OUT/rom-index.json` caches ROM hashes.

`report.py` writes:

- `verdicts.csv`: one row per ROM, with each column's verdict, sub-class,
  screen class, frames reached, static-from frame and sheet path.
- `disagreements.csv`: the same rows, for ROMs whose verdicts differ.
- `summary.md`: verdict counts per column and the disagreement table.
- `sheets/<column>/page-NNN.jpg`: overview pages of run rows.
- `sheets/disagreements/<rom>.jpg`: the columns' rows stacked, one under
  another.

A column is one (core sha256, config hash) pair in a result directory. A
directory holding runs of two core builds therefore yields two columns,
labelled `LABEL@<core>.<config>`. Within a column, the latest run of a ROM
counts. A ROM's status is `agree`, `disagree`, `incomplete` (missing from some
column; the rest agree) or `single`.

`--shed` deletes `bundles/` from every run of each `agree` ROM and keeps
`record.json` and `contact.jpg`. It needs at least two columns. Shed only
after every core you mean to compare has run.

## Verdicts

The rules are in [`verdicts.py`](verdicts.py). A run is *complete* when it
reached every planned frame and has every planned capture.

| Verdict | Meaning | Sub-class |
|---|---|---|
| `pass` | Complete, clean exit, not blank, not frozen. | |
| `frozen` | Complete and clean, and the last `frozen_min_captures` captures (default 5 of 8, so frames 2400–4800) are byte-identical. `static_from_frame` is where the identical tail starts. | `static-from-start` (every capture identical) or `advanced-then-static` |
| `blank` | Complete and clean, and every capture is blank: at most three colours, or at least 99.5% of pixels with max RGB ≤ 8. | |
| `quit-fail` | Complete, but the frontend did not quit cleanly. `screen` keeps the visual class (`pass`, `frozen`, `blank`). | `quit-timeout`, `stop-failed` or `orphan-reaped` |
| `hang` | The session stopped making progress before completing. | `step` (stepping stopped), `unresponsive` (another command timed out while the frontend was alive) or `ttl` (session TTL expired) |
| `crash` | The frontend exited before completing. | |
| `load-fail` | The frontend reported that the content could not be loaded. | |
| `error` | Harness failure: capture, artifact or staging. Not evidence about the core. | the runner's exit class |

`frozen` is purely mechanical: the frames stopped changing. A game waiting at
a menu that ignores the pattern's inputs is frozen, and so is a hard lock-up.
The sheets tell them apart.

## Input

[`patterns/default.json`](patterns/default.json) runs 4800 frames, with a
capture every 600 frames. Each press is held for 6 frames:

| Frames | Input | Why |
|---|---|---|
| 0–1199 | none | Boot logos and intros run untouched, so early captures compare across cores. |
| 1200, 1800, 2700, 3600 | Start | Skips intros and title screens. |
| 1500, 2100, 3000, 3900, 4500 | A | Confirms the default choice. |
| 2400 → 2430 | Down, then A | Picks the second entry of a vertical menu. |
| 3300 → 3330 | Right, then A | Picks the second entry of a horizontal choice, for example Yes/No. |
| 4200 → 4230 | Down, then A | The same, on the next menu down the path. |

Choices behind the pattern:

- Directions press the D-pad and fully deflect the left stick at once,
  because N64 menus read one or the other.
- A 6-frame hold covers games that poll the controller every third frame and
  stays below typical menu auto-repeat delays.
- The pattern never presses B (cancel), Select, Z, L or R. Mupen-derived
  cores bind Select to a controller-remap callback, and Z, L and R combinations
  can trigger resets or debug modes in some games.

Input names map to RetroPad state through the standard mupen64plus libretro
mapping (A = RetroPad B, B = RetroPad Y, Z = L2, C buttons = right stick). A
core that remaps controls per game can differ.

A ROM that needs other input gets a file `<rom sha256>.json` with the same
schema in the `--input-overrides` directory. A pattern has `frames`,
`captures` (increasing, at most `frames`), `hold_frames` and
`frozen_min_captures`, plus `presses` of the form
`{"frame": N, "input": "a" | ["down", "a"], "hold": optional}`. Presses must
not overlap. The pattern's behaviour is part of the run key, so changing an
override reruns only that ROM; descriptions and `why` notes are not part of
the key.

## Core profiles

[`profiles.json`](profiles.json) holds the frontend settings shared by every
core (a fixed 320x240 GPU-screenshot viewport and no vsync) and one entry per
core:

```json
"my-core": {
  "description": "What it is and how it was built.",
  "port1_accessory": {"option": "my-core-pak1", "value": "memory"},
  "options": {"my-core-rdp-plugin": "parallel", "my-core-upscaling": "1x"}
}
```

`port1_accessory` is required. Every profile uses a Controller Pak (`memory`),
so no core stops at a "No Controller Pak" prompt that another core never
shows. Find the option names and values with
`python3 tools/sweep/core_info.py CORE.so`. Options a profile does not set stay
at the core's defaults, and the record lists them under
`options.effective`. Each run starts with empty, bundle-local savefiles, so a
new pak is formatted by the core.

## Tests

`./run-tests.sh -R 'emu\.support\.sweep_'` runs the unit tests for patterns,
verdicts, the runner (against a fake adapter) and the report. They need
Python 3 and Pillow, not an emulator.
