# Open Technical Issues

This register holds technical details owned by `parallel-n64`.
Scheduling, priority, fleet rollout, eval operation, and gameplay research are
tracked by their owning systems.

## IL-3: Load-settle input pathology

Status: needs a minimal reproduction.

Input injected during the three-frame settle after a state load has shown
suspect behavior. The fault boundary may be core state restoration, frontend
task completion, or adapter sequencing. Do not change renderer/core behavior
until a minimal fixture identifies the layer.

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
