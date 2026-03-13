#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
PARALLEL_RUNNER="$SCRIPT_DIR/run-paper-mario-hires-capture.sh"
GLIDE_RUNNER="$SCRIPT_DIR/run-paper-mario-gliden64-capture.sh"

mode="parallel"
tag=""
capture_root=""
parallel_screenshot_at="22"
glide_screenshot_at="19"
pause_before_shot="1"
pause_before_shot_delay="0.2"
declare -a extra_args=()

usage() {
  cat <<'EOF_USAGE'
Usage:
  run-paper-mario-hires-intro22-capture.sh [options] [-- EXTRA_ARGS...]

Options:
  --parallel         Use the `parallel` helper (default)
  --glide            Use the GLideN64 helper
  --tag NAME         Capture tag to pass through
  --capture-root DIR Override the capture root
  --parallel-screenshot-at SEC
                    Screenshot time for `parallel` (default: 22)
  --glide-screenshot-at SEC
                    Screenshot time for `glide` (default: 19)
  --pause-before-shot
                    Send PAUSE_TOGGLE before SCREENSHOT (default)
  --no-pause-before-shot
                    Skip PAUSE_TOGGLE before SCREENSHOT
  --pause-before-shot-delay SEC
                    Delay after PAUSE_TOGGLE before SCREENSHOT (default: 0.2)
  -h, --help         Show this help

Behavior:
  - `parallel` capture uses the trusted timed intro22 oracle scene:
    `--smoke-mode timed --screenshot-at <parallel-shot> --timed-close-delay 10 --require-hires`
  - `glide` capture uses the matching no-input oracle path:
    `--screenshot-at <glide-shot> --start-delay 40 --post-delay 2 --hires-on`
  - Extra arguments after `--` are appended to the underlying helper.
EOF_USAGE
}

while (($#)); do
  case "$1" in
    --parallel)
      mode="parallel"
      ;;
    --glide)
      mode="glide"
      ;;
    --tag)
      shift
      tag="${1:-}"
      ;;
    --capture-root)
      shift
      capture_root="${1:-}"
      ;;
    --parallel-screenshot-at)
      shift
      parallel_screenshot_at="${1:-}"
      ;;
    --glide-screenshot-at)
      shift
      glide_screenshot_at="${1:-}"
      ;;
    --pause-before-shot)
      pause_before_shot="1"
      ;;
    --no-pause-before-shot)
      pause_before_shot="0"
      ;;
    --pause-before-shot-delay)
      shift
      pause_before_shot_delay="${1:-}"
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    --)
      shift
      extra_args+=("$@")
      break
      ;;
    *)
      echo "Unknown argument: $1" >&2
      usage >&2
      exit 1
      ;;
  esac
  shift
done

declare -a cmd=()

if [[ "$mode" == "parallel" ]]; then
  cmd=(
    "$PARALLEL_RUNNER"
    --smoke-mode timed
    --screenshot-at "$parallel_screenshot_at"
    --timed-close-delay 10
    --require-hires
  )
else
  cmd=(
    "$GLIDE_RUNNER"
    --screenshot-at "$glide_screenshot_at"
    --start-delay 40
    --post-delay 2
    --hires-on
  )
fi

if [[ -n "$tag" ]]; then
  cmd+=(--tag "$tag")
fi

if [[ -n "$capture_root" ]]; then
  cmd+=(--capture-root "$capture_root")
fi

if [[ "$pause_before_shot" == "1" ]]; then
  cmd+=(--pause-before-shot --pause-before-shot-delay "$pause_before_shot_delay")
else
  cmd+=(--no-pause-before-shot)
fi

cmd+=("${extra_args[@]}")

exec "${cmd[@]}"
