# Paper Mario HIRES Lanes

This is the current lane ledger for the standardized `intro22-state + 1 frame`
workflow. Use it as the source of truth before adding new probes or fixes.

## Truth Path

- Capture:
  - `./run-paper-mario-hires-intro22-state-capture.sh --tag <tag>`
- Canonical baseline capture:
  - `./run-paper-mario-hires-intro22-baseline-capture.sh`
- Probe vs baseline compare:
  - `./run-paper-mario-hires-intro22-probe-compare.sh --tag <tag>`
  - uses fixed crop boxes with no alignment search
- Oracle compare:
  - `./run-paper-mario-hires-intro22-compare.sh`

Rules:
- Gate every proof on probe-vs-baseline first.
- If the probe compare says the image is pixel-identical to baseline, stop.
- Only compare against GLide after the probe has a real baseline diff.
- Do not use timed intro22 captures as renderer truth. They are for oracle
  maintenance only.

## Resolved Lanes

### story_text

- Fixed lanes:
  - intro22 story overlay force-blend quads:
    - `81,84..91`
    - committed in `35256e77`
  - intro22 `desc65` shadow/tint subgroup:
    - `shade={3,3,2,255}`
    - row `y=3536..3676`
    - `x<=1075`
    - committed in `b33d4e34`
- Current status:
  - improved, but still not a primary truth source
  - fading/animation still makes it less trustworthy than static regions

### left_stage_grid

- Fixed lanes:
  - intro22 `desc66` additive cluster:
    - committed in `85a0b9a8` and broadened in `3105881a`
  - intro22 `desc68` bright sibling raster modulation:
    - committed in `3ce96eba` and `efe32b4b`
- Current status:
  - partially resolved
  - still has remaining washout/stitching

## Unresolved Lanes

### top_banner

- Trusted owner:
  - `desc68`
- Current best read:
  - repeated-pass / composition issue in the remaining `desc68` path
- Known dead directions:
  - dither
  - image-read
  - texrect mode
  - broad blender selector changes
  - simple dark-subtype `c1_mul` or `c1_add` tweaks
- Known live direction:
  - bright subtype cycle-1 modulation was real and already fixed
  - remaining issue likely needs repeated-pass composition analysis, not another
    scalar constant sweep

### left_stage_grid

- Trusted owners:
  - `desc66`
  - `desc68` dark/remaining sibling lanes
- Current best read:
  - multi-cycle repeated-pass composition
- Known dead directions:
  - force-blend clears on `desc66`
  - image-read
  - dither
  - one-size-fits-all shared modulation rules

### bottom_stage_grid

- Current status:
  - unresolved secondary lane
- Current best read:
  - mixed lower-strip/shared composition path
- Notes:
  - do not chase it before proving the lane against the baseline
  - do not use it as the first target for a new probe

## Current Metrics

Trusted intro22 baseline after the latest committed fixes:
- `top_banner 8.6935`
- `story_text 38.7723`
- `bottom_stage_grid 42.8426`
- `left_stage_grid 14.0864`

## Recommended Next Focus

1. `top_banner`
- treat the remaining issue as repeated-pass composition
- instrument pass grouping/order/overlap, not flags

2. `left_stage_grid`
- keep `desc66` and `desc68` dark as separate owners
- inspect repeated-pass accumulation and multi-cycle composition

3. `bottom_stage_grid`
- only after a probe produces a real baseline diff
- avoid speculative owner-family sweeps
