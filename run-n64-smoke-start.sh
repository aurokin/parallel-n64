#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
RUNNER="$SCRIPT_DIR/run-n64.sh"
VPAD_TOOL="$SCRIPT_DIR/tools/virtual_gamepad.py"

start_delay=20
post_delay=10
input_interval=5
button_hold_ms=140
max_presses=99
buttons_csv="start,a"
vpad_socket="/tmp/parallel-n64-vpad-smoke.sock"
vpad_device_name="parallel-n64 Virtual Pad"

vpad_log=""
vpad_pid=""
run_pid=""
smoke_input_cfg=""
declare -a runner_args=()
declare -a buttons=()

usage() {
  cat <<'EOF_USAGE'
Usage:
  run-n64-smoke-start.sh [options] [RUN_N64_ARGS...]

Options:
  --start-delay SEC       Seconds before input automation begins (default: 20)
  --post-delay SEC        Seconds to keep running after first input tick (default: 10)
  --interval SEC          Seconds between input ticks (default: 5)
  --button-hold-ms MS     Milliseconds to hold each button tap (default: 140)
  --buttons CSV           Buttons to tap each tick (default: start,a)
  --max-presses N         Cap total input ticks (default: 99)
  --vpad-socket PATH      UNIX socket path for virtual gamepad daemon
  -h, --help              Show this help

Behavior:
  - Starts tools/virtual_gamepad.py daemon automatically.
  - Launches ./run-n64.sh with provided args.
  - Injects a temporary RetroArch input override for the virtual pad.
  - After --start-delay, sends each --buttons entry every --interval seconds.
  - Never sends LOAD_STATE/SAVE_STATE network commands (input automation only).
  - Runs for --post-delay seconds total after first input tick, then sends SIGINT.
  - Stops virtual gamepad daemon automatically on exit.

Examples:
  ./run-n64-smoke-start.sh -- --verbose
  ./run-n64-smoke-start.sh --start-delay 20 --post-delay 15 --interval 5 -- --verbose
  ./run-n64-smoke-start.sh --buttons start,a,b --button-hold-ms 180 -- --verbose
EOF_USAGE
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

split_buttons() {
  local raw="$1"
  local token
  local -a temp=()

  IFS=',' read -r -a temp <<< "$raw"
  buttons=()

  for token in "${temp[@]}"; do
    token="${token//[[:space:]]/}"
    token="${token,,}"
    [[ -z "$token" ]] && continue
    buttons+=("$token")
  done

  if ((${#buttons[@]} == 0)); then
    echo "--buttons must contain at least one non-empty button name." >&2
    return 1
  fi
}

start_vpad_daemon() {
  if [[ ! -x "$VPAD_TOOL" ]]; then
    echo "Virtual gamepad tool is missing or not executable: $VPAD_TOOL" >&2
    return 1
  fi

  vpad_log="$(mktemp /tmp/parallel-n64-vpad-daemon.XXXX.log)"
  python3 "$VPAD_TOOL" stop --socket "$vpad_socket" >/dev/null 2>&1 || true
  python3 "$VPAD_TOOL" stop --socket "/tmp/parallel-n64-vpad-gliden64.sock" >/dev/null 2>&1 || true
  rm -f "$vpad_socket"

  python3 "$VPAD_TOOL" daemon --socket "$vpad_socket" --name "$vpad_device_name" >"$vpad_log" 2>&1 &
  vpad_pid="$!"

  local i
  for i in $(seq 1 40); do
    if [[ -S "$vpad_socket" ]]; then
      break
    fi
    sleep 0.1
  done

  if [[ ! -S "$vpad_socket" ]]; then
    echo "Virtual gamepad daemon failed to start (socket not ready): $vpad_socket" >&2
    echo "Daemon log: $vpad_log" >&2
    sed -n '1,80p' "$vpad_log" >&2 || true
    return 1
  fi

  for i in $(seq 1 40); do
    if rg -Fq "$vpad_device_name" /proc/bus/input/devices 2>/dev/null; then
      return 0
    fi
    sleep 0.1
  done

  echo "Virtual gamepad device failed to enumerate: $vpad_device_name" >&2
  echo "Daemon log: $vpad_log" >&2
  sed -n '1,80p' "$vpad_log" >&2 || true
  return 1
}

write_smoke_input_override() {
  smoke_input_cfg="$(mktemp /tmp/parallel-n64-smoke-input.XXXX.cfg)"
  cat >"$smoke_input_cfg" <<EOF_INPUT
input_autodetect_enable = "false"
pause_nonactive = "false"
input_player1_joypad_index = "0"
input_player1_device_reservation_type = "0"
input_player1_reserved_device = "$vpad_device_name"
input_player1_a_btn = "0"
input_player1_b_btn = "1"
input_player1_x_btn = "2"
input_player1_y_btn = "3"
input_player1_l_btn = "4"
input_player1_r_btn = "5"
input_player1_l2_btn = "6"
input_player1_r2_btn = "7"
input_player1_select_btn = "8"
input_player1_start_btn = "9"
input_player1_l3_btn = "11"
input_player1_r3_btn = "12"
input_player1_up_btn = "13"
input_player1_down_btn = "14"
input_player1_left_btn = "15"
input_player1_right_btn = "16"
EOF_INPUT
}

focus_retroarch_window() {
  if ! command -v xdotool >/dev/null 2>&1; then
    return 0
  fi

  local window_ids=""
  window_ids="$(xdotool search --onlyvisible --name "RetroArch" 2>/dev/null || true)"
  if [[ -z "$window_ids" ]]; then
    return 0
  fi

  while read -r wid; do
    [[ -z "$wid" ]] && continue
    xdotool windowactivate "$wid" >/dev/null 2>&1 || true
    return 0
  done <<< "$window_ids"
}

append_runner_passthrough() {
  local arg=""
  local has_separator=0

  for arg in "${runner_args[@]}"; do
    if [[ "$arg" == "--" ]]; then
      has_separator=1
      break
    fi
  done

  if (( has_separator == 0 )); then
    runner_args+=(--)
  fi

  runner_args+=("$@")
}

stop_vpad_daemon() {
  if [[ -n "$vpad_socket" && -S "$vpad_socket" ]]; then
    python3 "$VPAD_TOOL" stop --socket "$vpad_socket" >/dev/null 2>&1 || true
  fi

  if [[ -n "$vpad_pid" ]]; then
    wait "$vpad_pid" >/dev/null 2>&1 || true
  fi

  if [[ -n "$vpad_socket" ]]; then
    rm -f "$vpad_socket" || true
  fi
}

cleanup() {
  if [[ -n "$run_pid" ]] && kill -0 "$run_pid" 2>/dev/null; then
    kill -INT "$run_pid" >/dev/null 2>&1 || true
    sleep 1
    if kill -0 "$run_pid" 2>/dev/null; then
      kill -KILL "$run_pid" >/dev/null 2>&1 || true
    fi
  fi
  stop_vpad_daemon
  if [[ -n "$smoke_input_cfg" && -f "$smoke_input_cfg" ]]; then
    rm -f "$smoke_input_cfg" || true
  fi
}

send_button_tap() {
  local button="$1"
  local output=""

  if output="$(python3 "$VPAD_TOOL" send --socket "$vpad_socket" tap "$button" "$button_hold_ms" 2>&1)"; then
    echo "Virtual pad tap: button=$button hold_ms=$button_hold_ms ($output)"
    return 0
  fi

  echo "Virtual pad tap failed: button=$button ($output)" >&2
  return 1
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
    --interval)
      shift
      input_interval="${1:-}"
      ;;
    --button-hold-ms)
      shift
      button_hold_ms="${1:-}"
      ;;
    --buttons)
      shift
      buttons_csv="${1:-}"
      ;;
    --max-presses)
      shift
      max_presses="${1:-}"
      ;;
    --vpad-socket)
      shift
      vpad_socket="${1:-}"
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
if ! command -v python3 >/dev/null 2>&1; then
  echo "python3 is required for virtual gamepad automation." >&2
  exit 1
fi

if ! require_nonnegative_int "$start_delay" "--start-delay"; then
  exit 1
fi
if ! require_nonnegative_int "$post_delay" "--post-delay"; then
  exit 1
fi
if ! require_positive_int "$input_interval" "--interval"; then
  exit 1
fi
if ! require_positive_int "$button_hold_ms" "--button-hold-ms"; then
  exit 1
fi
if ! require_positive_int "$max_presses" "--max-presses"; then
  exit 1
fi
if ! split_buttons "$buttons_csv"; then
  exit 1
fi

if [[ "$vpad_socket" == "/tmp/parallel-n64-vpad-smoke.sock" ]]; then
  vpad_socket="$(mktemp -u /tmp/parallel-n64-vpad-smoke.XXXX.sock)"
fi

trap cleanup EXIT

start_vpad_daemon
write_smoke_input_override
append_runner_passthrough --appendconfig "$smoke_input_cfg"

echo "Smoke-start: buttons=${buttons[*]} at +${start_delay}s, every ${input_interval}s, hold=${button_hold_ms}ms, post_window=${post_delay}s, socket=$vpad_socket"
echo "Smoke-start: retroarch input override=$smoke_input_cfg"

"$RUNNER" "${runner_args[@]}" &
run_pid="$!"

(
  elapsed=0
  while (( elapsed < start_delay )); do
    if ! kill -0 "$run_pid" 2>/dev/null; then
      exit 0
    fi
    sleep 1
    ((elapsed += 1))
  done

  # Input ticks happen at t=0, interval, 2*interval ... inside post window.
  effective_presses="$max_presses"
  max_window_presses=$(( post_delay / input_interval + 1 ))
  if (( effective_presses > max_window_presses )); then
    effective_presses="$max_window_presses"
  fi

  tick=1
  elapsed_post=0
  sent_any=0

  while (( tick <= effective_presses )); do
    if ! kill -0 "$run_pid" 2>/dev/null; then
      exit 0
    fi

    focus_retroarch_window

    for button in "${buttons[@]}"; do
      if send_button_tap "$button"; then
        sent_any=1
      fi
    done

    if (( elapsed_post + input_interval > post_delay )); then
      break
    fi

    sleep "$input_interval"
    ((elapsed_post += input_interval))
    ((tick += 1))
  done

  # Fill remaining post window time.
  while (( elapsed_post < post_delay )); do
    if ! kill -0 "$run_pid" 2>/dev/null; then
      exit 0
    fi
    sleep 1
    ((elapsed_post += 1))
  done

  if (( sent_any == 0 )); then
    echo "No virtual pad inputs were sent successfully." >&2
  fi

  if kill -0 "$run_pid" 2>/dev/null; then
    kill -INT "$run_pid" >/dev/null 2>&1 || true
    sleep 5
    if kill -0 "$run_pid" 2>/dev/null; then
      kill -KILL "$run_pid" >/dev/null 2>&1 || true
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

if [[ -n "$vpad_log" ]]; then
  echo "Virtual gamepad daemon log: $vpad_log"
fi

exit "$rc"
