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

Status: queued implementation follow-up.

RetroArch provides `WAIT_LOAD_STATE DONE` after draining the
asynchronous load task. The interactive adapter's `cmd_load_slot` still
treats the load-start log as completion and waits a fixed interval before
checking failure.

Required change:

- send `WAIT_LOAD_STATE` after the load acknowledgement;
- wait for `WAIT_LOAD_STATE DONE` before returning;
- remove the fixed completion delay;
- fail clearly when the frontend lacks the command;
- cover success, failed load, and missing-command behavior with focused tests.
