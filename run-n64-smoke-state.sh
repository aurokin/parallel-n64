#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
RUNNER="$SCRIPT_DIR/run-n64.sh"

load_delay="2.2"
pause_delay="0.2"
shot_delay="1.2"
close_delay="0.2"
netcmd_port="55355"
state_cmd="LOAD_STATE"
send_pause="0"

declare -a runner_args=()
run_pid=""

usage() {
  cat <<'EOF_USAGE'
Usage:
  run-n64-smoke-state.sh [options] [RUN_N64_ARGS...]

Options:
  --load-delay SEC       Delay before sending state command (default: 2.2)
  --pause-delay SEC      Delay after state load before PAUSE_TOGGLE (default: 0.2)
  --shot-delay SEC       Delay after PAUSE_TOGGLE before SCREENSHOT (default: 1.2)
  --close-delay SEC      Delay after SCREENSHOT before close (default: 0.2)
  --pause                Send PAUSE_TOGGLE step before screenshot
  --no-pause             Skip PAUSE_TOGGLE step before screenshot (default)
  --port PORT            RetroArch network command UDP port (default: 55355)
  --state-cmd CMD        Command to send for state load (default: LOAD_STATE)
  -h, --help             Show this help

Examples:
  ./run-n64-smoke-state.sh -- --verbose
  ./run-n64-smoke-state.sh --load-delay 2.2 --shot-delay 1.2 --close-delay 0.2 -- --verbose
  ./run-n64-smoke-state.sh --pause --pause-delay 1.0 --shot-delay 0.2 -- --verbose
EOF_USAGE
}

is_nonnegative_number() {
  [[ "$1" =~ ^([0-9]+([.][0-9]+)?|[.][0-9]+)$ ]]
}

is_positive_int() {
  [[ "$1" =~ ^[1-9][0-9]*$ ]]
}

send_netcmd() {
  local cmd="$1"
  printf '%s\n' "$cmd" | nc -u -w1 127.0.0.1 "$netcmd_port" >/dev/null 2>&1
}

stop_runner() {
  if [[ -n "$run_pid" ]] && kill -0 "$run_pid" 2>/dev/null; then
    kill -INT "$run_pid" >/dev/null 2>&1 || true
    sleep 1
    if kill -0 "$run_pid" 2>/dev/null; then
      kill -KILL "$run_pid" >/dev/null 2>&1 || true
    fi
  fi
}

cleanup() {
  stop_runner
}

while (($#)); do
  case "$1" in
    --load-delay)
      shift
      load_delay="${1:-}"
      ;;
    --pause-delay)
      shift
      pause_delay="${1:-}"
      ;;
    --shot-delay)
      shift
      shot_delay="${1:-}"
      ;;
    --close-delay)
      shift
      close_delay="${1:-}"
      ;;
    --no-pause)
      send_pause="0"
      ;;
    --pause)
      send_pause="1"
      ;;
    --port)
      shift
      netcmd_port="${1:-}"
      ;;
    --state-cmd)
      shift
      state_cmd="${1:-}"
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

if ! command -v nc >/dev/null 2>&1; then
  echo "nc (netcat) is required for network commands." >&2
  exit 1
fi

if ! is_nonnegative_number "$load_delay"; then
  echo "--load-delay must be a non-negative number: $load_delay" >&2
  exit 1
fi

if ! is_nonnegative_number "$pause_delay"; then
  echo "--pause-delay must be a non-negative number: $pause_delay" >&2
  exit 1
fi

if ! is_nonnegative_number "$shot_delay"; then
  echo "--shot-delay must be a non-negative number: $shot_delay" >&2
  exit 1
fi

if ! is_nonnegative_number "$close_delay"; then
  echo "--close-delay must be a non-negative number: $close_delay" >&2
  exit 1
fi

if ! is_positive_int "$netcmd_port"; then
  echo "--port must be a positive integer: $netcmd_port" >&2
  exit 1
fi

trap cleanup EXIT

pause_state="disabled"
if [[ "$send_pause" == "1" ]]; then
  pause_state="enabled"
fi
echo "Smoke-state: load_cmd='$state_cmd' at +${load_delay}s, pause=${pause_state}, pause_delay=${pause_delay}s, screenshot +${shot_delay}s after pause/load, close +${close_delay}s after screenshot, port=${netcmd_port}"

"$RUNNER" "${runner_args[@]}" &
run_pid="$!"

sleep "$load_delay"
if kill -0 "$run_pid" 2>/dev/null; then
  if send_netcmd "$state_cmd"; then
    echo "NetCmd: sent '$state_cmd'"
  else
    echo "NetCmd failed: '$state_cmd'" >&2
  fi
fi

if [[ "$send_pause" == "1" ]]; then
  sleep "$pause_delay"
  if kill -0 "$run_pid" 2>/dev/null; then
    if send_netcmd "PAUSE_TOGGLE"; then
      echo "NetCmd: sent 'PAUSE_TOGGLE'"
    else
      echo "NetCmd failed: 'PAUSE_TOGGLE'" >&2
    fi
  fi
fi

sleep "$shot_delay"
if kill -0 "$run_pid" 2>/dev/null; then
  if send_netcmd "SCREENSHOT"; then
    echo "NetCmd: sent 'SCREENSHOT'"
  else
    echo "NetCmd failed: 'SCREENSHOT'" >&2
  fi
fi

sleep "$close_delay"
stop_runner

rc=0
if ! wait "$run_pid"; then
  rc=$?
fi

if (( rc == 130 || rc == 143 )); then
  rc=0
fi

exit "$rc"
