#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
PARALLEL_RUNNER="$SCRIPT_DIR/run-paper-mario-hires-capture.sh"
GLIDE_RUNNER="$SCRIPT_DIR/run-paper-mario-gliden64-capture.sh"

mode="parallel"
tag=""
capture_root=""
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
  -h, --help         Show this help

Behavior:
  - `parallel` capture uses the trusted no-input intro `Today...` scene:
    `--smoke-mode timed --screenshot-at 22 --timed-close-delay 10 --require-hires`
  - `glide` capture uses the matching no-input oracle path:
    `--screenshot-at 22 --start-delay 40 --post-delay 2 --hires-on`
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
    --screenshot-at 22
    --timed-close-delay 10
    --require-hires
  )
else
  cmd=(
    "$GLIDE_RUNNER"
    --screenshot-at 22
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

cmd+=("${extra_args[@]}")

exec "${cmd[@]}"
