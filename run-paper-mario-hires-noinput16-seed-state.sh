#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
RUNNER="$SCRIPT_DIR/run-paper-mario-hires-capture.sh"

tag="noinput16-seed-r1"
capture_root="${TMPDIR:-/tmp}/parallel-n64-paper-mario-captures"
savestate_dir="/tmp/parallel-n64-paper-mario-saves/noinput16-seed-r1"
screenshot_at="16"
timed_close_delay="20"
declare -a extra_args=()

usage() {
  cat <<'EOF_USAGE'
Usage:
  run-paper-mario-hires-noinput16-seed-state.sh [options] [-- EXTRA_ARGS...]

Options:
  --tag NAME              Timed capture tag to pass through (default: noinput16-seed-r1)
  --capture-root DIR      Override the capture root
  --savestate-dir DIR     Canonical save-state directory to populate
                          (default: /tmp/parallel-n64-paper-mario-saves/noinput16-seed-r1)
  --screenshot-at SEC     Timed screenshot/save-state point (default: 16)
  --timed-close-delay SEC Delay after screenshot before close (default: 20)
  -h, --help              Show this help

Behavior:
  - Runs the legacy noinput16 timed scene long enough to send SAVE_STATE at the
    screenshot point.
  - Copies the resulting slot-0 save state into the canonical noinput16 seed dir.
  - If the timed helper fails only because no screenshot flushed, this script still
    succeeds as long as the save state was created.
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
    --screenshot-at)
      shift
      screenshot_at="${1:-}"
      ;;
    --timed-close-delay)
      shift
      timed_close_delay="${1:-}"
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

capture_dir="$capture_root/$tag"
fallback_state_dir="$capture_dir/xdg/retroarch/states/ParaLLEl N64"
dest_state_dir="$savestate_dir/ParaLLEl N64"
dest_state_file="$dest_state_dir/Paper Mario (USA).state"
dest_state_png="$dest_state_dir/Paper Mario (USA).state.png"
fallback_state_file="$fallback_state_dir/Paper Mario (USA).state"
fallback_state_png="$fallback_state_dir/Paper Mario (USA).state.png"

mkdir -p "$capture_root"
mkdir -p "$dest_state_dir"

set +e
"$RUNNER" \
  --smoke-mode timed \
  --screenshot-at "$screenshot_at" \
  --timed-save-state-at "$screenshot_at" \
  --timed-close-delay "$timed_close_delay" \
  --savestate-dir "$savestate_dir" \
  --capture-root "$capture_root" \
  --tag "$tag" \
  --require-hires \
  "${extra_args[@]}"
runner_status=$?
set -e

if [[ ! -f "$dest_state_file" && -f "$fallback_state_file" ]]; then
  cp -f "$fallback_state_file" "$dest_state_dir/"
  if [[ -f "$fallback_state_png" ]]; then
    cp -f "$fallback_state_png" "$dest_state_dir/"
  fi
fi

if [[ ! -f "$dest_state_file" ]]; then
  echo "noinput16 seed failed: missing $dest_state_file" >&2
  if [[ -f "$capture_dir/run.log" ]]; then
    echo "Capture log: $capture_dir/run.log" >&2
  fi
  exit "${runner_status:-1}"
fi

echo "Seed capture dir: $capture_dir"
echo "Canonical save-state dir: $savestate_dir"
echo "Seeded state: $dest_state_file"
if [[ "$runner_status" -ne 0 ]]; then
  echo "Timed helper exited non-zero after SAVE_STATE, but the canonical state was copied successfully."
fi
