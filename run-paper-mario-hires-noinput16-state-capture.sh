#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
RUNNER="$SCRIPT_DIR/run-paper-mario-hires-capture.sh"
SEED_RUNNER="$SCRIPT_DIR/run-paper-mario-hires-noinput16-seed-state.sh"

tag=""
capture_root=""
savestate_dir="/tmp/parallel-n64-paper-mario-saves/noinput16-seed-r1"
frame_advance="1"
frame_advance_delay="0.05"
seed_if_missing="1"
declare -a extra_args=()

usage() {
  cat <<'EOF_USAGE'
Usage:
  run-paper-mario-hires-noinput16-state-capture.sh [options] [-- EXTRA_ARGS...]

Options:
  --tag NAME                  Capture tag to pass through
  --capture-root DIR          Override the capture root
  --savestate-dir DIR         Override the noinput16 seeded savestate directory
                              (default: /tmp/parallel-n64-paper-mario-saves/noinput16-seed-r1)
  --frame-advance N           Frames to advance after load/pause (default: 1)
  --frame-advance-delay SEC   Delay between FRAMEADVANCE commands (default: 0.05)
  --seed-if-missing           Seed the canonical noinput16 save state if absent (default)
  --no-seed-if-missing        Fail if the canonical noinput16 save state is absent
  -h, --help                  Show this help

Behavior:
  - Uses the seeded noinput16 same-core save state.
  - Forces `--smoke-mode state --require-hires --state-pause`.
  - Standardized default is `--state-frame-advance 1`.
  - Extra arguments after `--` are appended to the underlying helper.
EOF_USAGE
}

while (($#)); do
  case "$1" in
    --tag)
      shift
      tag="${1:-}"
      ;;
    --capture-root)
      shift
      capture_root="${1:-}"
      ;;
    --savestate-dir)
      shift
      savestate_dir="${1:-}"
      ;;
    --frame-advance)
      shift
      frame_advance="${1:-}"
      ;;
    --frame-advance-delay)
      shift
      frame_advance_delay="${1:-}"
      ;;
    --seed-if-missing)
      seed_if_missing="1"
      ;;
    --no-seed-if-missing)
      seed_if_missing="0"
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

state_file="$savestate_dir/ParaLLEl N64/Paper Mario (USA).state"
if [[ ! -f "$state_file" ]]; then
  if [[ "$seed_if_missing" == "1" ]]; then
    "$SEED_RUNNER" --savestate-dir "$savestate_dir"
  else
    echo "Missing noinput16 save state: $state_file" >&2
    exit 1
  fi
fi

declare -a cmd=(
  "$RUNNER"
  --smoke-mode state
  --savestate-dir "$savestate_dir"
  --require-hires
  --state-pause
  --state-frame-advance "$frame_advance"
  --state-frame-advance-delay "$frame_advance_delay"
)

if [[ -n "$tag" ]]; then
  cmd+=(--tag "$tag")
fi

if [[ -n "$capture_root" ]]; then
  cmd+=(--capture-root "$capture_root")
fi

cmd+=("${extra_args[@]}")

exec "${cmd[@]}"
