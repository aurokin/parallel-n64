#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
RUNNER="$SCRIPT_DIR/run-n64.sh"

start_delay=20
post_delay=10
start_key="Return"
window_name="RetroArch"
declare -a runner_args=()

usage() {
  cat <<'EOF'
Usage:
  run-n64-smoke-start.sh [options] [RUN_N64_ARGS...]

Options:
  --start-delay SEC    Seconds before sending Start input (default: 20)
  --post-delay SEC     Seconds to keep running after Start input (default: 10)
  --start-key KEY      Key name to inject (default: Return)
  --window-name NAME   Window title match for xdotool (default: RetroArch)
  -h, --help           Show this help

Behavior:
  - Launches ./run-n64.sh with provided args.
  - Sends one key press to the RetroArch window after --start-delay.
  - Sends SIGINT after --post-delay and exits.

Examples:
  ./run-n64-smoke-start.sh -- --verbose
  ./run-n64-smoke-start.sh --start-delay 15 --post-delay 20 -- --verbose
  ./run-n64-smoke-start.sh --reference "Mario Kart 64 (USA).z64" -- --verbose
EOF
}

require_nonnegative_int() {
  local value="$1"
  local name="$2"
  if [[ ! "$value" =~ ^[0-9]+$ ]]; then
    echo "$name must be a non-negative integer: $value" >&2
    return 1
  fi
}

send_start_key() {
  local target_pid="$1"
  local key="$2"
  local name="$3"
  local window_id=""

  window_id="$(xdotool search --pid "$target_pid" --name "$name" 2>/dev/null | head -n 1 || true)"
  if [[ -z "$window_id" ]]; then
    window_id="$(xdotool search --onlyvisible --name "$name" 2>/dev/null | head -n 1 || true)"
  fi

  if [[ -n "$window_id" ]]; then
    timeout 2s xdotool windowactivate "$window_id" >/dev/null 2>&1 || true
    timeout 2s xdotool key --window "$window_id" --clearmodifiers "$key" >/dev/null 2>&1
  else
    timeout 2s xdotool key --clearmodifiers "$key" >/dev/null 2>&1
  fi
}

while (($#)); do
  case "$1" in
    --start-delay)
      shift
      start_delay="${1:-}"
      ;;
    --post-delay)
      shift
      post_delay="${1:-}"
      ;;
    --start-key)
      shift
      start_key="${1:-}"
      ;;
    --window-name)
      shift
      window_name="${1:-}"
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    --)
      shift
      runner_args+=(-- "$@")
      break
      ;;
    *)
      runner_args+=("$1")
      ;;
  esac
  shift
done

if [[ ! -x "$RUNNER" ]]; then
  echo "run-n64.sh is missing or not executable: $RUNNER" >&2
  exit 1
fi

if ! require_nonnegative_int "$start_delay" "--start-delay"; then
  exit 1
fi
if ! require_nonnegative_int "$post_delay" "--post-delay"; then
  exit 1
fi
if ! command -v xdotool >/dev/null 2>&1; then
  echo "run-n64-smoke-start.sh requires xdotool in PATH." >&2
  exit 1
fi
if [[ -z "${DISPLAY:-}" ]]; then
  echo "run-n64-smoke-start.sh requires DISPLAY to be set." >&2
  exit 1
fi

"$RUNNER" "${runner_args[@]}" &
run_pid=$!

echo "Smoke-start: key=$start_key at +${start_delay}s, stop +${post_delay}s later."

(
  elapsed=0
  while (( elapsed < start_delay )); do
    if ! kill -0 "$run_pid" 2>/dev/null; then
      exit 0
    fi
    sleep 1
    ((elapsed += 1))
  done

  if kill -0 "$run_pid" 2>/dev/null; then
    if send_start_key "$run_pid" "$start_key" "$window_name"; then
      echo "Sent start key: $start_key"
    else
      echo "Failed to send start key: $start_key" >&2
    fi
  fi

  elapsed=0
  while (( elapsed < post_delay )); do
    if ! kill -0 "$run_pid" 2>/dev/null; then
      exit 0
    fi
    sleep 1
    ((elapsed += 1))
  done

  if kill -0 "$run_pid" 2>/dev/null; then
    kill -INT "$run_pid" 2>/dev/null || true
    sleep 5
    if kill -0 "$run_pid" 2>/dev/null; then
      kill -KILL "$run_pid" 2>/dev/null || true
    fi
  fi
) &
helper_pid=$!

rc=0
if ! wait "$run_pid"; then
  rc=$?
fi
wait "$helper_pid" || true

if (( rc == 130 || rc == 143 )); then
  rc=0
fi

exit "$rc"
