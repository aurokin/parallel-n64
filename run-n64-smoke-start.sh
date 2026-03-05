#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
RUNNER="$SCRIPT_DIR/run-n64.sh"
RETROARCH_CFG_DEFAULT="$HOME/.config/retroarch/retroarch.cfg"

start_delay=20
post_delay=10
start_key=""
window_name="RetroArch"
start_burst_count=99
start_burst_interval=5
start_key_hold_ms=200
declare -a runner_args=()
declare -a start_keys=()

usage() {
  cat <<'EOF'
Usage:
  run-n64-smoke-start.sh [options] [RUN_N64_ARGS...]

Options:
  --start-delay SEC       Seconds before sending Start input (default: 20)
  --post-delay SEC        Seconds to keep running after Start input (default: 10)
  --start-key KEY         Key name to inject (default: auto-detect from RetroArch cfg)
  --window-name NAME      Window title match for xdotool (default: RetroArch)
  --start-burst-count N   Max number of Start key attempts (default: 99)
  --start-burst-interval SEC
                         Seconds between Start attempts (default: 5)
  --start-key-hold-ms MS  Milliseconds to hold each key press (default: 200)
  -h, --help              Show this help

Behavior:
  - Launches ./run-n64.sh with provided args.
  - Sends Start repeatedly after --start-delay and every --start-burst-interval.
  - Repeats only inside the --post-delay window, then sends SIGINT and exits.

Examples:
  ./run-n64-smoke-start.sh -- --verbose
  ./run-n64-smoke-start.sh --start-delay 15 --post-delay 20 -- --verbose
  ./run-n64-smoke-start.sh --start-key Return --start-burst-interval 5 -- --verbose
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

require_positive_int() {
  local value="$1"
  local name="$2"
  if [[ ! "$value" =~ ^[1-9][0-9]*$ ]]; then
    echo "$name must be a positive integer: $value" >&2
    return 1
  fi
}

normalize_key_for_xdotool() {
  local raw="$1"
  case "${raw,,}" in
    ""|nul|none)
      printf '%s' ""
      ;;
    enter|return)
      printf '%s' "Return"
      ;;
    kp_enter|keypad_enter)
      printf '%s' "KP_Enter"
      ;;
    *)
      printf '%s' "$raw"
      ;;
  esac
}

detect_retroarch_start_key() {
  local cfg="$RETROARCH_CFG_DEFAULT"
  local token=""

  if [[ ! -f "$cfg" ]]; then
    printf '%s' ""
    return 0
  fi

  token="$(awk -F'"' '/^input_player1_start = /{print $2; exit}' "$cfg" 2>/dev/null || true)"
  normalize_key_for_xdotool "$token"
}

append_unique_start_key() {
  local candidate="$1"
  local existing

  [[ -z "$candidate" ]] && return 0

  for existing in "${start_keys[@]}"; do
    if [[ "$existing" == "$candidate" ]]; then
      return 0
    fi
  done

  start_keys+=("$candidate")
}

configure_start_keys() {
  local detected=""

  if [[ -n "$start_key" ]]; then
    append_unique_start_key "$(normalize_key_for_xdotool "$start_key")"
    return 0
  fi

  detected="$(detect_retroarch_start_key)"
  append_unique_start_key "$detected"
  append_unique_start_key "Return"
  append_unique_start_key "KP_Enter"

  if ((${#start_keys[@]} == 0)); then
    append_unique_start_key "Return"
  fi
}

find_retroarch_window_id() {
  local target_pid="$1"
  local name="$2"
  local window_id=""

  window_id="$(xdotool search --pid "$target_pid" --name "$name" 2>/dev/null | head -n 1 || true)"
  if [[ -z "$window_id" ]]; then
    window_id="$(xdotool search --onlyvisible --name "$name" 2>/dev/null | head -n 1 || true)"
  fi

  printf '%s' "$window_id"
}

send_start_key_once() {
  local target_pid="$1"
  local key="$2"
  local name="$3"
  local window_id=""
  local window_label=""
  local hold_seconds

  hold_seconds="$(awk -v ms="$start_key_hold_ms" "BEGIN { printf \"%.3f\", ms / 1000.0 }")"
  window_id="$(find_retroarch_window_id "$target_pid" "$name")"

  if [[ -n "$window_id" ]]; then
    window_label="$(xdotool getwindowname "$window_id" 2>/dev/null || true)"
    echo "Start target window: id=$window_id name=${window_label:-unknown} key=$key hold_ms=$start_key_hold_ms"
    timeout 2s xdotool windowactivate "$window_id" >/dev/null 2>&1 || true
    timeout 2s xdotool keydown --window "$window_id" --clearmodifiers "$key" >/dev/null 2>&1
    sleep "$hold_seconds"
    timeout 2s xdotool keyup --window "$window_id" --clearmodifiers "$key" >/dev/null 2>&1
  else
    echo "Start target window: fallback-active key=$key hold_ms=$start_key_hold_ms"
    timeout 2s xdotool keydown --clearmodifiers "$key" >/dev/null 2>&1
    sleep "$hold_seconds"
    timeout 2s xdotool keyup --clearmodifiers "$key" >/dev/null 2>&1
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
    --start-burst-count)
      shift
      start_burst_count="${1:-}"
      ;;
    --start-burst-interval)
      shift
      start_burst_interval="${1:-}"
      ;;
    --start-key-hold-ms)
      shift
      start_key_hold_ms="${1:-}"
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
if ! require_nonnegative_int "$start_burst_count" "--start-burst-count"; then
  exit 1
fi
if ! require_positive_int "$start_burst_interval" "--start-burst-interval"; then
  exit 1
fi
if ! require_positive_int "$start_key_hold_ms" "--start-key-hold-ms"; then
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

configure_start_keys

"$RUNNER" "${runner_args[@]}" &
run_pid=$!

echo "Smoke-start: keys=${start_keys[*]} at +${start_delay}s, repeat every ${start_burst_interval}s within +${post_delay}s window, hold=${start_key_hold_ms}ms, stop at window end."

(
  elapsed=0
  while (( elapsed < start_delay )); do
    if ! kill -0 "$run_pid" 2>/dev/null; then
      exit 0
    fi
    sleep 1
    ((elapsed += 1))
  done

  effective_presses="$start_burst_count"
  if (( effective_presses > 0 )); then
    max_window_presses=$(( post_delay / start_burst_interval + 1 ))
    if (( effective_presses > max_window_presses )); then
      effective_presses="$max_window_presses"
    fi
  fi

  sent_any=0
  press=1
  next_press_at=0
  elapsed_post=0

  while true; do
    if ! kill -0 "$run_pid" 2>/dev/null; then
      exit 0
    fi

    if (( press <= effective_presses && elapsed_post >= next_press_at )); then
      for key in "${start_keys[@]}"; do
        if send_start_key_once "$run_pid" "$key" "$window_name"; then
          echo "Sent start key [$press/$effective_presses]: $key"
          sent_any=1
          break
        fi
      done
      ((press += 1))
      ((next_press_at += start_burst_interval))
    fi

    if (( elapsed_post >= post_delay )); then
      break
    fi

    sleep 1
    ((elapsed_post += 1))
  done

  if (( effective_presses > 0 && sent_any == 0 )); then
    echo "Failed to send any start key from set: ${start_keys[*]}" >&2
  fi

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
