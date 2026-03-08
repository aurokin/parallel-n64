# Paper Mario Scenes

This file is the short manifest for reusable Paper Mario validation scenes.
Use it to avoid rediscovering timing, compare profiles, or oracle paths.

## intro22

- Purpose:
  - Primary HIRES scene for current corruption/composition debugging.
  - Good for the wide intro story text, top banner, bottom stage grid, and left stage grid.
- Parallel capture:
  - `./run-paper-mario-hires-intro22-capture.sh --tag <tag>`
- Tune per-core timing when `parallel` and GLide drift:
  - `./run-paper-mario-hires-intro22-capture.sh --parallel-screenshot-at <sec> --glide-screenshot-at <sec>`
- Frame freeze before screenshot:
  - enabled by default with `PAUSE_TOGGLE`
  - disable only if it proves harmful:
    - `./run-paper-mario-hires-intro22-capture.sh --no-pause-before-shot`
- Refresh latest compare in one command:
  - `./run-paper-mario-hires-intro22-refresh.sh`
- GLide oracle capture:
  - `./run-paper-mario-hires-intro22-capture.sh --glide --tag <tag>`
- Compare:
  - `./run-paper-mario-hires-intro22-compare.sh`
  - open latest compare:
    - `./run-paper-mario-open-compare.sh --profile intro22`
  - raw equivalent:
    - `./run-paper-mario-hires-zoom-compare.sh --profile intro22`
- Timing:
  - no-input timed capture
  - aligned default is `parallel=22s`, `glide=19s`
  - `parallel` and GLide can still be offset independently when the scene drifts
  - `parallel` close delay `10s`
  - `GLide` input deferred with `--start-delay 40 --post-delay 2`
  - both helpers pause immediately before the screenshot by default
- Preserved GLide oracle:
  - `/home/auro/code/parallel-n64-paper-mario-backups/20260306-hires-audit/hires/oracle-gliden64-4x-hires-on-intro22-matched-1/Paper Mario (USA)-260308-170727.png`

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
