#!/usr/bin/env bash
set -euo pipefail

cat <<'EOF_SCENES'
Paper Mario scenes

intro22
  parallel: ./run-paper-mario-hires-intro22-capture.sh --tag <tag>
  refresh:  ./run-paper-mario-hires-intro22-refresh.sh
  glide:    ./run-paper-mario-hires-intro22-capture.sh --glide --tag <tag>
  compare:  ./run-paper-mario-hires-intro22-compare.sh
  open:     ./run-paper-mario-open-compare.sh --profile intro22
  purpose:  timed oracle maintenance for the matched intro story scene

intro22-state
  parallel: ./run-paper-mario-hires-intro22-state-capture.sh --tag <tag>
  baseline: ./run-paper-mario-hires-intro22-baseline-capture.sh
  probe:    ./run-paper-mario-hires-intro22-probe-compare.sh --tag <tag>
  open:     ./run-paper-mario-open-compare.sh --profile intro22-probe
  purpose:  seeded same-core intro22 truth path, standardized on `--state-frame-advance 1`

noinput16
  parallel: ./run-paper-mario-hires-capture.sh --smoke-mode timed --screenshot-at 16 --tag <tag> --require-hires
  compare:  ./run-paper-mario-hires-zoom-compare.sh --profile noinput16
  purpose:  legacy no-input HIRES scene

file-select-state
  parallel: ./run-paper-mario-hires-capture.sh --smoke-mode state --tag <tag> --require-hires
  purpose:  fast same-core state iteration

Reference:
  docs/PAPER_MARIO_SCENES.md
EOF_SCENES
