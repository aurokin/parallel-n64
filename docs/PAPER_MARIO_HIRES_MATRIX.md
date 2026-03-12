# Paper Mario HIRES Debug Matrix

This is the current execution matrix for Paper Mario HIRES debugging.
Use it with the standardized seeded intro22 state path and exact pixel diffs.

## Baseline Rules

- Scene:
  - `intro22-state`
- Capture helper:
  - `./run-paper-mario-hires-intro22-state-capture.sh --tag <tag>`
- Standardized state behavior:
  - `--smoke-mode state`
  - `--require-hires`
  - `--state-pause`
  - `--state-frame-advance 1`
- Oracle:
  - use the preserved GLide intro22 matched oracle only for visual comparison
  - do not refresh GLide during normal renderer debugging
- Truth gate:
  - do not trust score movement alone
  - every proof must first show a real baseline pixel diff
  - use the compare metadata from `summary.json` / `summary.txt`

## Region Map

- `top_banner`
  - trusted live owner: `desc68`
  - current best read: alpha/composition lane
- `left_stage_grid`
  - trusted live owners: `desc66` and part of `desc68`
  - current best read: multi-cycle overlay/composition lane
- `bottom_stage_grid`
  - unresolved
  - current best read: downstream/shared composition, not a simple direct-owner lane
- `story_text`
  - de-emphasize for now
  - it fades and is a poor primary truth source unless explicitly frozen at a known phase

## Matrix Axes

Run the matrix in this order.

### 1. Frame Variant

- `state+1f`
  - default truth source
  - use for `top_banner` and `left_stage_grid`
- `state+120f`
  - secondary animated-text phase
  - use only when intentionally investigating `story_text` or `bottom_stage_grid`

### 2. Ownership Level

- descriptor family
  - `PARALLEL_HIRES_SUPPRESS_DRAW_DESC=<list>`
- subtype within a family
  - `PARALLEL_HIRES_MATCH_RASTER_FLAGS=...`
  - `PARALLEL_HIRES_MATCH_C0_A=...`
  - `PARALLEL_HIRES_MATCH_SHADE=...`
  - `PARALLEL_HIRES_SUPPRESS_MATCHED_DRAW=1`
- descriptorless / non-replacement lane
  - `PARALLEL_HIRES_SUPPRESS_DRAW_DESC='*'`
  - pair with subtype filters above

### 3. System Layer

- early draw-state flags
  - `CLEAR_FORCE_BLEND`
  - `CLEAR_MULTI_CYCLE`
  - `CLEAR_IMAGE_READ`
  - `FORCE_IMAGE_READ`
  - `CLEAR_DITHER`
  - `CLEAR_DEPTH_TEST`
  - `CLEAR_DEPTH_UPDATE`
  - `CLEAR_COLOR_ON_CVG`
  - `CLEAR_AA`
  - `CLEAR_ALPHA_TEST`
  - `FORCE_NATIVE_TEXRECT`
  - `FORCE_UPSCALED_TEXRECT`
- blender source selectors
  - `BLEND_1A_*`
  - `BLEND_1B_*`
  - `BLEND_2A_*`
  - `BLEND_2B_*`
- runtime blend outputs
  - `FORCE_BLEND_EN_*`
  - `FORCE_CVG_WRAP_*`
  - `FORCE_BLEND_SHIFT_*`
  - `FORCE_PIXEL_ALPHA_*`
- combiner-cycle probes
  - `FORCE_CYCLE0_RGB_*`
  - `FORCE_CYCLE0_ALPHA_*`
  - `FORCE_CYCLE1_RGB_*`

## Current Known Results

### top_banner

- Owner:
  - `desc68`
- Trusted subtype:
  - `PARALLEL_HIRES_MATCH_RASTER_FLAGS=0x21844108`
  - `PARALLEL_HIRES_MATCH_C0_A=7,7,7,1`
  - `PARALLEL_HIRES_MATCH_SHADE=255,255,255,255`
- Known live levers:
  - `CLEAR_FORCE_BLEND`: real regression
  - `CLEAR_MULTI_CYCLE`: real regression
  - `FORCE_CYCLE0_ALPHA_FULL`: real regression
  - `FORCE_CYCLE0_ALPHA_TEXEL0`: no-op
- Known dead levers:
  - `CLEAR_DITHER`
  - `CLEAR_IMAGE_READ`
  - depth / AA / coverage / alpha-test clears
  - texrect mode overrides
  - blender source selectors

Interpretation:
- do not spend time on outer state flags here
- stay on cycle-0 / cycle-1 alpha-composition semantics

### left_stage_grid

- Trusted owners:
  - `desc66`
  - `desc68` dark subtype
- `desc66` facts:
  - suppression causes a real pixel change
  - `CLEAR_MULTI_CYCLE`: real regression
  - `CLEAR_FORCE_BLEND`: no-op
  - `CLEAR_IMAGE_READ`: no-op
  - `CLEAR_DITHER`: no-op
  - `FORCE_CYCLE1_RGB_COMBINED`: real regression
  - `FORCE_CYCLE1_RGB_ZERO`: stronger regression
  - `FORCE_CYCLE1_RGB_TEXEL0`: regression in the same lane
- `desc68` dark subtype facts:
  - contributes to the same region, but through a different composition path than the bright banner subtype

Interpretation:
- left-stage is a multi-cycle overlay/composition problem
- for `desc66`, the lever is later cycle RGB behavior, not alpha and not outer flags

### bottom_stage_grid

- Current status:
  - unresolved
- What has been eliminated:
  - obvious direct replacement-family owners
  - several obvious descriptorless families
- Current interpretation:
  - likely downstream/shared composition
  - not a simple single-family suppression target

Interpretation:
- do not keep sweeping random descriptor families here
- use this region only after a later shared-path probe exists

## Execution Order

For any new investigation pass:

1. Capture the standardized baseline.
2. Run one proof only.
3. Compare proof vs baseline first.
4. If `exact_equal=True`, discard it immediately.
5. Only if the proof changes pixels:
   - inspect the per-region `diff_bbox`
   - then compare against the preserved GLide oracle

## Recommended Next Matrix

### Lane A: `top_banner`

- keep frame variant at `state+1f`
- lock subtype to the trusted `desc68` bright subtype
- probe only:
  - cycle-0 alpha production
  - cycle-1 alpha/composition handoff
- do not revisit:
  - dither
  - image-read
  - texrect
  - broad blend source selectors

### Lane B: `left_stage_grid`

- keep frame variant at `state+1f`
- split the work:
  - `desc66`
  - `desc68` dark subtype
- probe only:
  - cycle-1 RGB / multi-cycle composition semantics
  - repeated-pass accumulation behavior
- do not revisit:
  - force-blend clears on `desc66`
  - image-read
  - dither

### Lane C: `bottom_stage_grid`

- move to `state+120f` only when intentionally investigating it
- first goal is ownership, not fixing
- do not use score-only movement as evidence

## Minimal Command Pattern

Baseline:

```bash
./run-paper-mario-hires-intro22-state-capture.sh --tag baseline
```

Subtype-targeted proof example:

```bash
PARALLEL_HIRES_SUPPRESS_DRAW_DESC=68 \
PARALLEL_HIRES_MATCH_RASTER_FLAGS=0x21844108 \
PARALLEL_HIRES_MATCH_C0_A=7,7,7,1 \
PARALLEL_HIRES_MATCH_SHADE=255,255,255,255 \
PARALLEL_HIRES_SUPPRESS_MATCHED_DRAW=1 \
./run-paper-mario-hires-intro22-state-capture.sh --tag proof
```

Then compare using the canonical intro22 compare path or direct pixel diff tooling.
