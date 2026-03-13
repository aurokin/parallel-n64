# Paper Mario HIRES Debug Matrix

This matrix is the current execution order for Paper Mario HIRES debugging.

It replaces the older mixed timed/state and `120f` methodology. For renderer
truth, use only the standardized `intro22-state + 1 frame` path.

## Standard Workflow

- Capture baseline:
  - `./run-paper-mario-hires-intro22-baseline-capture.sh`
- Capture probe:
  - `./run-paper-mario-hires-intro22-state-capture.sh --tag <tag>`
- Compare probe vs baseline first:
  - `./run-paper-mario-hires-intro22-probe-compare.sh --tag <tag>`
- Compare probe vs preserved GLide oracle second:
  - `./run-paper-mario-hires-intro22-compare.sh --tag <tag>`

Rules:
- `state+1f` only
- exact baseline diff first
- GLide second
- do not use timed intro22 captures as renderer truth

## Current Region Priorities

- `top_banner`
  - primary unresolved lane
- `left_stage_grid`
  - primary unresolved lane
- `bottom_stage_grid`
  - secondary unresolved lane
- `story_text`
  - de-emphasized as a primary oracle region
  - partially fixed, but still less stable than the static regions

See the full current owner/fix ledger in:
- [PAPER_MARIO_HIRES_LANES.md](/home/auro/code/parallel-n64/docs/PAPER_MARIO_HIRES_LANES.md)

## Matrix Axes

### 1. Ownership

- descriptor family:
  - `PARALLEL_HIRES_SUPPRESS_DRAW_DESC=<list>`
- subtype within a family:
  - `PARALLEL_HIRES_MATCH_RASTER_FLAGS=...`
  - `PARALLEL_HIRES_MATCH_C0_A=...`
  - `PARALLEL_HIRES_MATCH_SHADE=...`
  - `PARALLEL_HIRES_SUPPRESS_MATCHED_DRAW=1`
- descriptorless lanes:
  - `PARALLEL_HIRES_SUPPRESS_DRAW_DESC='*'`
  - pair with subtype filters

### 2. System Layer

- early draw-state flags
- blender source selectors
- runtime blend outputs
- combiner-cycle probes

Use only the smallest probe surface that matches the current lane theory.

## Execution Order

For every new investigation pass:

1. Capture or reuse the canonical intro22 baseline.
2. Run one proof only.
3. Compare probe vs baseline.
4. If the probe is pixel-identical to baseline, discard it.
5. If the probe changes pixels, inspect the affected lane/region.
6. Only then compare against GLide.

## What Not To Do

- do not use `state+120f` as a normal debugging mode
- do not use score-only movement as evidence
- do not mix timed intro22 captures into renderer diagnosis
- do not sweep random descriptor families without a lane hypothesis
