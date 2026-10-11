# Open Technical Issues

This register holds technical details owned by `parallel-n64`.
Scheduling, priority, fleet rollout, eval operation, and gameplay research are
tracked by their owning systems.

## IL-3: Load-settle input pathology

Status: unconfirmed historical report; needs a minimal reproduction before fix work.

Input injected during the three-frame settle after a state load has shown
suspect behavior. The fault boundary may be core state restoration, frontend
task completion, or adapter sequencing. Do not change renderer/core behavior
until a minimal fixture identifies the layer. The completed load barrier and
bounded control checks do not reproduce this report or establish that it is
fixed. Do not treat it as a prerequisite for ordinary emulator development;
retain it as a reproduction lead if post-load input fails again.

Acceptance:

- a deterministic minimal reproduction;
- classification to core, RetroArch, or adapter;
- a focused regression test in the owning layer.

## IL-19: Complete `load-slot` with `WAIT_LOAD_STATE`

Status: implemented; frontend rollout must pass capability preflight.

`load-slot` now waits for the load command acknowledgement, then sends
`WAIT_LOAD_STATE` and requires a fresh `WAIT_LOAD_STATE DONE` before returning.
It checks this attempt's load record and failure log after the barrier because
DONE only reports task completion, not successful deserialization. The fixed
completion delay is removed. An explicit failed load may retry once after the
barrier and a one-second contention backoff. Acknowledgement or barrier timeout
leaves the outcome unknown and fails without another load.

`check-frontend --retroarch-bin PATH` probes the compiled command table with
`--verbose` and an invalid `--command` operand (no content launch or command
transmission), with a five-second timeout. It prints `FRONTEND_LOAD_BARRIER=ready` and exits zero only when
both load verbs and `WAIT_LOAD_STATE` are advertised. Failures report `missing`,
`unsupported`, `timeout`, or `probe-error` and exit nonzero, without exposing
binary diagnostics or private paths. `start` runs this check before creating a
session; `load-slot` repeats it against the recorded session binary before
sending a load or barrier. An absent recorded binary fails closed.

The display-free `emu.support.interactive_load_contract` test exercises the
actual adapter FIFO/log path with a fake frontend: delayed completion and
failure, explicit retry, missing load record, stale logs, acknowledgement and
barrier timeout, unsupported frontend, and start preflight. It does not qualify
a deployed frontend or resolve IL-3's separate post-load input pathology.

A bounded Linux headless Vulkan check on September 29, 2026 passed real slot-4
restoration, input/frame stepping and capture, explicit missing-slot failure,
successful recovery, and clean shutdown. The separately recorded runtime receipt
pins the frontend and adapter bytes. The capability probe also exposed a
command-only teardown crash fixed in the frontend owner before qualification.
Rollback keeps the preceding adapter and its matching frontend together; do not
substitute this adapter into an unqualified frozen runtime. No renderer/core
behavior or benchmark scoring changed.

## Batch adapter macOS metadata and early-exit cleanup

Status: fixed October 3, 2026.

The batch adapter used GNU-only `sed -i` to update bundle metadata after
launching RetroArch. BSD sed rejected it on macOS, and the adapter's exit trap
removed the FIFO without stopping its child. Metadata now updates the nested
JSON status through Python before launch and on completion. Early exits reap
only the adapter's owned child, with bounded TERM/KILL cleanup, before releasing
the runtime lock. Signal handlers use the same exit path.

The display-free batch lifecycle contract checks portable status updates,
preservation of unrelated metadata, rejection of invalid metadata before
launch, and command-failure child cleanup through the real FIFO. It passed on
Linux and macOS. The macOS title scenario subsequently completed command
execution and clean teardown. Its native sampled-entry failure exposed a stale
fixture requirement, corrected under ADR-0006 as described below.

## Paper Mario fixture identity gates

Status: aligned with ADR-0006 October 3, 2026.

The title, file-select and `kmr_03 ENTRY_5` runtime profiles still required native
sampled entries after that enrichment program was frozen. The active path uses
Rice-compatible draw-time replacement, so a compatibility-only package could
render correctly and still fail the obsolete gate.

These profiles now require compatible draw-hit presence. Loaded entries and
upload hits alone do not satisfy it. The authority wrapper accepts compatible
entries by default, requires actual draw-time activity, and removes its obsolete
native minimum/compat opt-in options. Native counts remain diagnostic evidence.
Provider/source, scene semantics, provenance, feature-off parity and explicit
visual review remain required; no renderer logic or pack identity changed.

Display-free contracts accept compatibility-only evidence and reject missing,
zero or malformed draw activity, including independent wrapper reuse checks.
Dormant converter enrichment input validation remains separate and unchanged.

## Hi-res replacement admission budget

Status: stable-descriptor admission accounting fixed; runtime pressure unqualified.

The opt-in `PARALLEL_RDP_HIRES_GPU_BUDGET_MB` configuration previously disappeared
when replacement resources reset during pack initialization. The setting now
survives provider/cache resets. Images that do not fit reject admission and use
the existing native-texture fallback. Already admitted replacement bindings stay
stable until the cache resets. Zero retains the default unlimited behavior.

The old single-image LRU eviction could still exceed the byte budget and reused
indices that buffered draws or tile bindings could still reference. It is removed;
introducing eviction requires an explicit descriptor-retirement design.

The display-free `emu.unit.hires_registry_budget` regression exercises distinct
slots, exact fits, full/oversized rejection, unlimited mode, descriptor exhaustion,
overflow-safe comparisons, and unchanged accounting on rejection. Renderer
compilation checks the integration. This accounts reserved replacement mip-chain
bytes; failed image creation can conservatively retain a reservation until cache
reset. It is not a physical VRAM ceiling. GPU-pressure rendering and nonzero-budget
runtime behavior still need bounded qualification before deployment.

An October 11 isolated 4x title probe with a 4 MiB admission limit completed
load, step, capture and shutdown with coherent mixed native/replacement output.
Its strict hi-res fixture rejected the missing compatible draw hits; it is a
fallback observation, not a passing replacement qualification or a measured
physical-memory bound. Broader pressure behavior remains unverified.

## Trilinear replacement cutout fringes

Status: alpha-weighted interpolation across mip levels fixed October 11, 2026.

Replacement mip samples contain straight RGB after opacity-weighted bilinear
filtering. Ordinary RGBA interpolation between them darkened cutout edges when
one mip contributed transparent black, or when the two opacities differed.
Trilinear filtering now weights the two RGB samples by their alpha contribution
and keeps interpolated alpha. Fully transparent results return zero RGBA.
Nearest, bilinear and feature-off sampling are unchanged.

`emu.unit.hires_mip_filter` executes the shared production shader arithmetic:
transparent boundaries, unequal opacity, ordinary opaque/equal-alpha filtering,
endpoints and a fractional-LOD cutout. The pinned shader tool regenerates the
same header repeatedly; all four feature-off rasterizer SPIR-V variants remain
byte-identical. These checks establish the sampling correction, not a verdict
on every historical scene or pack.

The isolated rebuilt core passes the hi-res title and named kmr03 fixtures at
4x with bounded visual review. Feature-off before/after title captures are
byte-identical on the same current frontend/configuration; both differ from the older recorded
headless baseline. That baseline mismatch is retained, not reminted by this
correction. Full GPU fractional-LOD cutout and macOS checks remain outstanding.

## CI32 diagnostic palette bounds

Status: out-of-range diagnostic groups rejected October 11, 2026.

The optional CI palette probe can derive groups above entry 255 from 16-bit
texels, while the renderer's TLUT shadow holds only 256 entries. Computing its
palette CRC read beyond that shadow. The probe now returns its existing
unavailable sentinel for an incomplete group rather than reading, wrapping or
hashing only a prefix. Normal replacement identity selection is unchanged.

The CI palette unit test covers the last valid group, first and highest
unavailable groups, and mixed valid/unavailable usage. AddressSanitizer exposed
the prior read and passes the corrected cases.

## Input override cleanup after a failed hold

Status: bounded release on failure/interruption added October 11, 2026.

The interactive adapter previously exited after failed input acknowledgement,
frame-status read or stepping without releasing its persistent input override.
Later stepping or state loads could therefore inherit held buttons or analog
input. An exit handler now attempts a bounded clear of the selected port and
checks its acknowledgement; an unreachable frontend still leaves an explicitly
uncertain outcome. Successful holds retain their existing clear sequence.

The display-free interactive contract exercises successful frame/analog holds,
missing set acknowledgement, failed status reads and interruption during a
stalled frame wait or timed hold through the real FIFO/log path. Timed holds
release promptly on interruption and reap their sleeping child. This does not
reproduce or close IL-3, whose historical fault boundary remains unconfirmed.

## Paper Mario fixture capture and scene admission

Status: invalid captures and wrong named world scenes rejected October 11, 2026.

The shared verifier previously accepted a non-image capture and accepted any
world scene for the `kmr_03 ENTRY_5` fixture when world callbacks and replacement
traffic matched. It now requires a decodable capture with positive dimensions.
Fixture profiles may declare typed area/map/entry expectations; the kmr03
profile requires area 0, map `kmr_03`, entry 5. Generic world fixtures remain
unconstrained unless their profile declares a scene.

Focused contracts reject malformed/truncated captures and wrong or missing
scene identities; existing title/file-select/kmr03 evidence remains admissible.
These are evidence admission checks. Correct textures, regions, filtering and
composition still require semantic evidence and explicit visual judgment.
