#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
RUNNER="$SCRIPT_DIR/run-n64.sh"

session_name="parallel-n64-pm-hires"
rom_path="Paper Mario (USA).zip"
load_delay="2.2"
netcmd_port="55355"
state_cmd="LOAD_STATE"
send_load_state="1"
use_tmux="1"
force_replace="0"
force_fullscreen="${RUN_N64_FULLSCREEN:-0}"
log_root="${TMPDIR:-/tmp}/parallel-n64-paper-mario"
declare -a runner_args=()

usage() {
  cat <<'EOF_USAGE'
Usage:
  run-paper-mario-hires-debug.sh [options] [-- RUN_N64_ARGS...]

Options:
  --session NAME        tmux session name (default: parallel-n64-pm-hires)
  --load-delay SEC      Delay before sending LOAD_STATE (default: 2.2)
  --port PORT           RetroArch network command UDP port (default: 55355)
  --state-cmd CMD       Network command to send after launch (default: LOAD_STATE)
  --no-load-state       Skip sending the post-launch network command
  --tmux                Launch inside tmux and keep the session alive (default)
  --no-tmux             Launch in the foreground
  --replace             Replace any existing tmux session with the same name
  --fullscreen          Force fullscreen launch
  --no-fullscreen       Disable fullscreen launch (default)
  --rom PATH            ROM path passed to run-n64.sh (default: Paper Mario (USA).zip)
  -h, --help            Show this help

Behavior:
  - Always enables `PARALLEL_RDP_HIRES_DEBUG=1`.
  - Uses RetroArch's live savestate path. It does not copy states from
    `/home/auro/code/paper_mario`.
  - By default passes `--verbose` through to `run-n64.sh`.

Examples:
  ./run-paper-mario-hires-debug.sh
  ./run-paper-mario-hires-debug.sh --replace
  ./run-paper-mario-hires-debug.sh --no-tmux -- --verbose
EOF_USAGE
}

is_nonnegative_number() {
  [[ "$1" =~ ^([0-9]+([.][0-9]+)?|[.][0-9]+)$ ]]
}

is_positive_int() {
  [[ "$1" =~ ^[1-9][0-9]*$ ]]
}

require_live_state() {
  local live_state="$HOME/.config/retroarch/states/ParaLLEl N64/Paper Mario (USA).state"
  if [[ ! -f "$live_state" ]]; then
    echo "Expected live RetroArch savestate missing: $live_state" >&2
    exit 1
  fi
}

send_netcmd() {
  local cmd="$1"
  printf '%s\n' "$cmd" | nc -u -w1 127.0.0.1 "$netcmd_port" >/dev/null 2>&1
}

shell_quote() {
  printf '%q' "$1"
}

while (($#)); do
  case "$1" in
    --session)
      shift
      session_name="${1:-}"
      ;;
    --load-delay)
      shift
      load_delay="${1:-}"
      ;;
    --port)
      shift
      netcmd_port="${1:-}"
      ;;
    --state-cmd)
      shift
      state_cmd="${1:-}"
      ;;
    --no-load-state)
      send_load_state="0"
      ;;
    --tmux)
      use_tmux="1"
      ;;
    --no-tmux)
      use_tmux="0"
      ;;
    --replace)
      force_replace="1"
      ;;
    --fullscreen)
      force_fullscreen="1"
      ;;
    --no-fullscreen)
      force_fullscreen="0"
      ;;
    --rom)
      shift
      rom_path="${1:-}"
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    --)
      shift
      runner_args+=("$@")
      break
      ;;
    *)
      echo "Unknown option: $1" >&2
      usage >&2
      exit 2
      ;;
  esac
  shift
done

if [[ ! -x "$RUNNER" ]]; then
  echo "run-n64.sh is missing or not executable: $RUNNER" >&2
  exit 1
fi

if ! command -v nc >/dev/null 2>&1; then
  echo "nc (netcat) is required for RetroArch network commands." >&2
  exit 1
fi

if [[ "$use_tmux" == "1" ]] && ! command -v tmux >/dev/null 2>&1; then
  echo "tmux is required for --tmux mode." >&2
  exit 1
fi

if ! is_nonnegative_number "$load_delay"; then
  echo "--load-delay must be a non-negative number: $load_delay" >&2
  exit 1
fi

if ! is_positive_int "$netcmd_port"; then
  echo "--port must be a positive integer: $netcmd_port" >&2
  exit 1
fi

require_live_state

mkdir -p "$log_root"
log_file="$log_root/${session_name}-$(date +%Y%m%d-%H%M%S).log"

if ((${#runner_args[@]} == 0)); then
  runner_args+=(--verbose)
fi

declare -a launch_cmd=()
launch_cmd+=("env" "PARALLEL_RDP_HIRES_DEBUG=1" "RUN_N64_FULLSCREEN=$force_fullscreen" "RUN_N64_MAXIMIZE=1")
launch_cmd+=("$RUNNER" "--rom-dir" "/home/auro/code/n64_roms" "$rom_path")
if [[ "$force_fullscreen" == "0" ]]; then
  launch_cmd+=("--no-fullscreen")
fi
if ((${#runner_args[@]} > 0)); then
  launch_cmd+=("--")
  launch_cmd+=("${runner_args[@]}")
fi

launch_line=""
for arg in "${launch_cmd[@]}"; do
  if [[ -n "$launch_line" ]]; then
    launch_line+=" "
  fi
  launch_line+="$(shell_quote "$arg")"
done
launch_line+=" 2>&1 | tee $(shell_quote "$log_file")"

if [[ "$use_tmux" == "1" ]]; then
  if tmux has-session -t "$session_name" 2>/dev/null; then
    if [[ "$force_replace" == "1" ]]; then
      tmux kill-session -t "$session_name"
    else
      echo "tmux session already exists: $session_name" >&2
      echo "Use --replace to restart it." >&2
      exit 1
    fi
  fi

  tmux new-session -d -s "$session_name" -n retroarch
  pane_target="$(tmux list-panes -t "$session_name" -F '#{session_name}:#{window_index}.#{pane_index}' | head -n 1)"
  if [[ -z "$pane_target" ]]; then
    echo "Failed to resolve tmux pane target for session: $session_name" >&2
    exit 1
  fi

  tmux send-keys -t "$pane_target" -l -- "$launch_line"
  tmux send-keys -t "$pane_target" Enter

  sleep "$load_delay"
  if [[ "$send_load_state" == "1" ]]; then
    if send_netcmd "$state_cmd"; then
      echo "NetCmd: sent '$state_cmd'"
    else
      echo "NetCmd failed: '$state_cmd'" >&2
    fi
  fi

  cat <<EOF_INFO
Session: $session_name
Log: $log_file
State source: $HOME/.config/retroarch/states/ParaLLEl N64/Paper Mario (USA).state

To monitor:
  tmux attach -t $session_name
  tmux capture-pane -p -J -t $pane_target -S -200
EOF_INFO
else
  bash -lc "$launch_line" &
  run_pid=$!

  sleep "$load_delay"
  if [[ "$send_load_state" == "1" ]] && kill -0 "$run_pid" 2>/dev/null; then
    if send_netcmd "$state_cmd"; then
      echo "NetCmd: sent '$state_cmd'"
    else
      echo "NetCmd failed: '$state_cmd'" >&2
    fi
  fi

  wait "$run_pid"
fi
