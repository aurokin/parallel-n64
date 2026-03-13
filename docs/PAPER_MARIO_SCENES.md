# Paper Mario Scenes

This file is the short manifest for reusable Paper Mario validation scenes.
Use it to avoid rediscovering timing, compare profiles, or oracle paths.

## intro22

- Purpose:
  - Timed intro used only for preserving and refreshing the matched GLide oracle.
- Parallel capture:
  - `./run-paper-mario-hires-intro22-capture.sh --tag <tag>`
- GLide oracle capture:
  - `./run-paper-mario-hires-intro22-capture.sh --glide --tag <tag>`
- Timing:
  - no-input timed capture
  - aligned default is `parallel=22s`, `glide=19s`
  - `parallel` and GLide can still be offset independently when the scene drifts
  - `parallel` close delay `10s`
  - `GLide` input deferred with `--start-delay 40 --post-delay 2`
  - both helpers pause immediately before the screenshot by default
- Preserved GLide oracle:
  - `/home/auro/code/parallel-n64-paper-mario-backups/20260306-hires-audit/hires/oracle-gliden64-4x-hires-on-intro22-matched-1/Paper Mario (USA)-260308-170727.png`

## intro22-state

- Purpose:
  - Standardized seeded same-core intro22 state for reliable renderer debugging.
  - This is the primary HIRES truth path.
- Parallel capture:
  - `./run-paper-mario-hires-intro22-state-capture.sh --tag <tag>`
- Canonical baseline capture:
  - `./run-paper-mario-hires-intro22-baseline-capture.sh`
- Probe vs baseline compare:
  - `./run-paper-mario-hires-intro22-probe-compare.sh --tag <tag>`
- Oracle compare:
  - `./run-paper-mario-hires-intro22-compare.sh --tag <tag>`
- Standardization:
  - uses `/tmp/parallel-n64-paper-mario-saves/intro22-seed-r1`
  - forces `--smoke-mode state --require-hires --state-pause`
  - standardized default is `--state-frame-advance 1`
- Notes:
  - earlier intro22 state/debug captures without this standardized path should be treated as stale
  - do not refresh GLide for this workflow; keep the preserved timed intro22 oracle fixed
  - use probe-vs-baseline compare first, then GLide
  - use:
    - `docs/PAPER_MARIO_HIRES_MATRIX.md`
    - `docs/PAPER_MARIO_HIRES_LANES.md`

## noinput16

- Purpose:
  - Older no-input HIRES scene kept for historical comparisons and legacy crop alignment.
- Parallel capture:
  - `./run-paper-mario-hires-capture.sh --smoke-mode timed --screenshot-at 16 --tag <tag> --require-hires`
- Compare:
  - `./run-paper-mario-hires-zoom-compare.sh --profile noinput16`
- Preserved GLide oracle:
  - `/home/auro/code/parallel-n64-paper-mario-backups/20260306-hires-audit/hires/oracle-gliden64-4x-hires-on-noinput-16s-1/Paper Mario (USA)-260308-011300.png`

## file-select-state

- Purpose:
  - Fast same-core iteration scene for HIRES-on or HIRES-off state-based debugging.
  - Best for repeated renderer experiments where a deterministic same-core state exists.
- Parallel capture:
  - `./run-paper-mario-hires-capture.sh --smoke-mode state --tag <tag> --require-hires`
- Notes:
  - Use only same-core save states.
  - Do not use save states from `/home/auro/code/paper_mario`.
