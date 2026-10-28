# Throughput benchmark

`run_benchmark.py` measures a serial scene × CPU engine × internal upscale ×
texture-pack matrix. It uses Python 3.9+ and the standard library, the regression
runner's input helpers, RetroArch and (on Linux) GNU time. No ROMs, states, packs
or binaries are included. This is development tooling, not part of the core.

## Linux

Use the maintained agent-control RetroArch build with `--max-frames` and
`headless_vk` support. Use a clean output directory and an idle host; run only
one RetroArch at a time. The runner holds the adapters' host-local runtime lock
(`$TMPDIR/parallel-n64-retroarch-runtime.lock`, default `/tmp`) and refuses to
start if a `retroarch` process is already present. This does not coordinate
other hosts or other Android controllers.

```sh
python3 tools/benchmark/run_benchmark.py \
  --core /path/to/paralleish_libretro.so \
  --retroarch-bin /path/to/retroarch \
  --base-config /path/to/base.cfg \
  --rom-map /path/to/rom-map.json --blobs /path/to/blobs \
  --scenes oot sm64 mk64 --engines dynamic_recompiler \
  --scales 1x --repetitions 3 \
  --vulkan-icd /path/to/selected_icd.json \
  --out artifacts/benchmark/baseline
```

`--manifest` defaults to the adjacent `scenes.json`; omit `--scenes` to select
all eight. `--engines` accepts `dynamic_recompiler`, `cached_interpreter` and
`pure_interpreter`. `--scales` accepts `1x`, `2x`, `4x`, `8x`. Select several
values after either flag to expand the matrix. Optional `--pack-map` is a JSON
object mapping ROM archive SHA-256 to an absolute local `.phrb` path, like the
regression runner. Each mapped game receives both feature-off and pack-on
cells. Package contents are hashed, used in place on Linux and never modified.
Ensure storage and memory can accommodate your chosen packages.

The ROM map is the regression runner's SHA-256 → absolute filename JSON map.
`resolve_inputs` verifies the mapped archive/file identity and decompresses and
verifies content-addressed `blobs/states/<sha>.state[.gz]` and optional
`blobs/savefiles/<sha>.srm[.gz]`. A ZIP must contain exactly one `.z64`, `.n64`
or `.v64` ROM, which is streamed into the output directory. The extracted ROM
has its own recorded hash. State autoload and persistent saves use that ROM's
basename in fresh, unsorted per-process directories. Corpus files stay intact.

The manifest pins the eight stationary anchors used in the October 27, 2026
benchmark: Kokiri Forest, Termina forest/tunnel, SM64 castle grounds, Luigi
Raceway, Banjo-Kazooie grassy cliffside, Conker stream, Paper Mario kmr_11 and
GoldenEye Dam. `scenario` is a corpus-relative provenance reference, not a file
that must be available at runtime. The pinned ROM/state/savefile hashes are
authoritative. Custom manifests use the same schema and may omit
`savefile_sha256` when no persistent save is needed.

Each cell gets a fresh 16-frame warm-up process, followed by three pairs of
fresh processes at 16 and 1216 frames. Odd-numbered pairs run short then long;
even-numbered pairs reverse order. Override `--short-frames` and `--long-frames`
for longer measurements. Every process starts from the same corpus state.
The explicit append config disables VSync, adaptive VSync, audio output/audio
sync, VRR pacing, runahead, preemptive frames, rewind, frame delay, overrides
and shaders. Fast-forward is never toggled. Vulkan headless rendering still
waits for GPU work; it omits normal presentation. `--video-context` can select
another context, but record that difference when comparing results.

The runner selects CXD4 RSP and synchronous paraLLEl-RDP, retaining native
texrect and native texture LOD. Core-option entries select the engine and
upscale. Each run must exit normally, report the requested engine and successful
state autoload in its core/frontend log, and confirm upscaling above 1x. Pack
cells require a hi-res summary without a cache-load failure; counters are
retained so a zero-hit pack is visible. Pack-on does not guarantee useful
replacements. Ambient `PARALLEL_*`, `RETROARCH_*`, `VK_*` and `MESA_*` overrides
are removed; `--vulkan-icd` explicitly selects an ICD. Shader caches are shared
within an output bundle. These are warmed throughput measurements.

## Android

Use an APK with agent-control UDP `GET_STATUS`, engine logging, state autoload
and Vulkan support. CPU/RSS and core identity collection require a debuggable
APK allowing `adb shell run-as <package>` to read its core and `/proc` data.
The device must be idle and exclusively assigned to this run. The runner
refuses an existing app process; it launches and stops only its selected package.
Use a standalone base config without credentials or external `#include` files.
ADB authorization and a reachable device UDP command port must already work.
ADB TCP forwarding does not forward UDP.

```sh
python3 tools/benchmark/run_benchmark.py \
  --android DEVICE_SERIAL --adb /path/to/adb --status-host DEVICE_IP \
  --core /data/data/com.retroarch/cores/paralleish_libretro_android.so \
  --base-config /path/to/android-base.cfg \
  --rom-map /path/to/rom-map.json --blobs /path/to/blobs \
  --scenes sm64 --engines dynamic_recompiler --scales 1x \
  --warmup-seconds 5 --window-seconds 23 --repetitions 3 \
  --out artifacts/benchmark/android-baseline
```

`--core` is an **existing device path** in this mode; nothing installs a core.
The other inputs are host paths. Supply an Android base config; a Linux config
may select unusable drivers or device paths. `--android-package`,
`--android-activity`, `--android-datadir` and `--android-root` support alternate
APK layouts (set them together). Default paths target `com.retroarch`.

The runner pushes extracted ROM/state/savefile bytes, a separate merged config
and core options to a unique generated subdirectory of `--android-root`. It
launches the activity with explicit ROM/LIBRETRO/CONFIGFILE intent extras and
autoloads the state. It never replaces the normal config/options or core.
Optional packs are pushed per run to the core's documented system-directory
layout and require big-endian `.z64` content. These can be large: allow enough
device space. Generated device directories are removed after runs; host input
copies and logs remain in the output bundle. On failure, inspect the incomplete
bundle. A hard kill of the host runner can leave its generated device directory.

After PLAYING becomes responsive, the runner waits the warm-up wall interval,
then measures frame-counter deltas across the requested fixed wall window.
Request/response midpoints determine the actual elapsed interval, with both
raw endpoints and round-trip times recorded. Non-PLAYING endpoints and reset
or stalled counters fail. No paused counter or `STEP_FRAME` timing is used.
Process CPU comes from `/proc/<pid>/stat` deltas and `getconf CLK_TCK` over a
nearby, separately recorded interval. RSS is process VmHWM. These differ from
Linux's paired CPU subtraction and GNU-time RSS. CPU is for the entire process,
not only the emulation thread. Short-term thermals, Android scheduling and
presentation can affect results. Linux and Android summaries cannot be compared
with `--compare`. Android execution is covered by display-free parsing/timing
contracts; device behavior still requires an exclusive on-device validation.

## Output and interpretation

`runs.jsonl` records each Linux warm-up/process and each paired or Android window
sample, including raw wall/CPU/RSS or status endpoints. `summary.json` records
binary/config/manifest/input hashes, settings and per-cell median/min/max FPS,
median CPU% and median peak RSS in KiB. `complete` is true only after the entire
matrix finishes. Each process's directory retains exact argv, generated config,
core options, log, GNU time output (Linux) and result. Failed process evidence
may exist only in its directory; an incomplete summary is not a full matrix.
Output can contain copyrighted extracted inputs and private config paths: keep
it local and out of source control. Output directories must not already exist;
there is no implicit resume or mixing of prior measurements.

Linux FPS = `(long_frames - short_frames) / (long_wall - short_wall)` using
GNU time's child elapsed value, which has 0.01-second resolution. CPU% uses the
same subtraction for user+system time; 100% means one logical CPU. Peak RSS is
the long process's peak without subtraction. Nonpositive adjusted wall time or
negative CPU time fails; increase run length if startup noise dominates.

```sh
python3 tools/benchmark/run_benchmark.py \
  --compare artifacts/benchmark/baseline/summary.json \
            artifacts/benchmark/candidate/summary.json
```

Comparison prints JSON with each matched cell's candidate-minus-baseline FPS
and percent change, both observed ranges, and `beyond_spread=true` only when
the ranges do not overlap. Added/removed cells are explicit. Changing input
bytes or timing windows under matching cell names is rejected. A self-compare
has zero deltas and still shows the measured spread. Non-overlap is a practical
noise flag, **not a statistical significance test**; one repetition cannot
estimate run-to-run variation. Keep hardware, frontend, driver, config and
background load stable and inspect their recorded evidence before attributing
a difference to the core.

FPS counts libretro/VI-rate frame advances, including duplicate game images.
It does not measure unique rendered frames, frame-time tails, input latency,
audio behavior, active traversal, worst-case gameplay or GPU utilization.
Paired subtraction only approximates fixed startup/teardown costs; shader and
state-load variance remains. There is no claim of bit-identical cross-engine
simulation. An above-60 result is not a general playability guarantee. A more
representative future extension would measure fixed gameplay traversals and
long thermal windows; this tool intentionally uses pinned stationary scenes.

Display-free contracts run with `./run-tests.sh -R emu.support.benchmark_contract`
and are automatically included in `--profile emu-required` through its
`emu.support.*` selection. They do not contact ADB or launch an emulator.
