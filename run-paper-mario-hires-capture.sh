#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
SMOKE_START_RUNNER="$SCRIPT_DIR/run-n64-smoke-start.sh"
SMOKE_STATE_RUNNER="$SCRIPT_DIR/run-n64-smoke-state.sh"
DEFAULT_ROM_DIR="/home/auro/code/n64_roms"
DEFAULT_ROM_NAME="Paper Mario (USA).zip"
DEFAULT_CORE_OPTIONS_FILE="$HOME/.config/retroarch/config/ParaLLEl N64/ParaLLEl N64.opt"
DEFAULT_SCREENSHOT_DIR="$HOME/.config/retroarch/screenshots"

smoke_mode="buttons"
start_delay=20
post_delay=10
input_interval=5
button_hold_ms=140
max_presses=2
buttons_csv="start"
screenshot_at=27
state_load_delay="2.2"
state_pause_delay="0.2"
state_shot_delay="1.2"
state_close_delay="0.2"
state_cmd="LOAD_STATE"
state_pause="0"
netcmd_port=55355
rom_dir="$DEFAULT_ROM_DIR"
rom_path="$DEFAULT_ROM_NAME"
capture_root="${TMPDIR:-/tmp}/parallel-n64-paper-mario-captures"
tag=""
debug_hires="0"
force_fullscreen="${RUN_N64_FULLSCREEN:-0}"

capture_cfg=""
capture_dir=""
log_file=""
core_options_file=""
stamp_file=""
run_pid=""
declare -a runner_args=()
declare -a core_option_overrides=()

usage() {
  cat <<'EOF_USAGE'
Usage:
  run-paper-mario-hires-capture.sh [options] [-- RUN_N64_ARGS...]

Options:
  --smoke-mode MODE       Capture path: buttons|state (default: buttons)
  --tag NAME              Capture subdirectory name (default: timestamp)
  --capture-root PATH     Root directory for screenshots/logs (default: /tmp/parallel-n64-paper-mario-captures)
  --rom PATH              ROM path passed to run-n64.sh (default: Paper Mario (USA).zip)
  --rom-dir PATH          ROM base directory (default: /home/auro/code/n64_roms)
  --start-delay SEC       Seconds before input automation begins (default: 20)
  --post-delay SEC        Seconds to keep running after first input tick (default: 10)
  --interval SEC          Seconds between input ticks (default: 5)
  --button-hold-ms MS     Milliseconds to hold each button tap (default: 140)
  --buttons CSV           Buttons to tap each tick (default: start)
  --max-presses N         Cap total input ticks (default: 2)
  --screenshot-at SEC     Seconds after launch to send SCREENSHOT (default: 27)
  --state-load-delay SEC  Delay before sending state command in state mode (default: 2.2)
  --state-pause-delay SEC Delay after state load before PAUSE_TOGGLE (default: 0.2)
  --state-shot-delay SEC  Delay after state load/pause before SCREENSHOT (default: 1.2)
  --state-close-delay SEC Delay after SCREENSHOT before close in state mode (default: 0.2)
  --state-cmd CMD         Command to send for state load in state mode (default: LOAD_STATE)
  --state-pause           Send PAUSE_TOGGLE before screenshot in state mode
  --no-state-pause        Skip PAUSE_TOGGLE in state mode (default)
  --port PORT             RetroArch network command UDP port (default: 55355)
  --core-option K=V       Override a ParaLLEl core option in the temp options file
  --debug-hires           Enable PARALLEL_RDP_HIRES_DEBUG=1 for the run
  --no-debug-hires        Disable PARALLEL_RDP_HIRES_DEBUG (default)
  --fullscreen            Force fullscreen launch
  --no-fullscreen         Disable fullscreen launch
  -h, --help              Show this help

Behavior:
  - Copies the active ParaLLEl core options file into the capture directory and
    runs against that temp copy for repeatability.
  - Adds a temporary RetroArch appendconfig that points screenshot output at the
    capture directory.
  - buttons mode uses run-n64-smoke-start.sh for deterministic Paper Mario input:
    boot, wait 20s, press start, wait 5s, press start, wait 2s.
  - state mode uses run-n64-smoke-state.sh to load the current same-core save state.
  - buttons mode sends RetroArch's SCREENSHOT network command at --screenshot-at seconds.
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

ensure_core_option() {
  local file="$1"
  local key="$2"
  local value="$3"

  mkdir -p "$(dirname "$file")"
  if [[ ! -f "$file" ]]; then
    printf '%s = "%s"\n' "$key" "$value" >"$file"
    return
  fi

  if rg -q "^${key} = " "$file"; then
    sed -i "s#^${key} = .*#${key} = \"${value}\"#" "$file"
  else
    printf '%s = "%s"\n' "$key" "$value" >>"$file"
  fi
}

apply_core_option_overrides() {
  local override=""
  local key=""
  local value=""

  for override in "${core_option_overrides[@]}"; do
    key="${override%%=*}"
    value="${override#*=}"
    ensure_core_option "$core_options_file" "$key" "$value"
  done
}

write_capture_appendconfig() {
  capture_cfg="$(mktemp /tmp/parallel-n64-paper-mario-capture.XXXX.cfg)"
  cat >"$capture_cfg" <<EOF_CFG
screenshot_directory = "$capture_dir"
screenshots_in_content_dir = "false"
sort_screenshots_by_content_enable = "false"
auto_screenshot_filename = "true"
notification_show_screenshot = "false"
notification_show_screenshot_flash = "0"
EOF_CFG
}

cleanup() {
  if [[ -n "$run_pid" ]] && kill -0 "$run_pid" 2>/dev/null; then
    kill -INT "$run_pid" >/dev/null 2>&1 || true
    sleep 1
    if kill -0 "$run_pid" 2>/dev/null; then
      kill -KILL "$run_pid" >/dev/null 2>&1 || true
    fi
  fi

  if [[ -n "$capture_cfg" && -f "$capture_cfg" ]]; then
    rm -f "$capture_cfg" || true
  fi

  if [[ -n "$stamp_file" && -f "$stamp_file" ]]; then
    rm -f "$stamp_file" || true
  fi
}

while (($#)); do
  case "$1" in
    --smoke-mode)
      shift
      smoke_mode="${1:-}"
      ;;
    --tag)
      shift
      tag="${1:-}"
      ;;
    --capture-root)
      shift
      capture_root="${1:-}"
      ;;
    --rom)
      shift
      rom_path="${1:-}"
      ;;
    --rom-dir)
      shift
      rom_dir="${1:-}"
      ;;
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
    --screenshot-at)
      shift
      screenshot_at="${1:-}"
      ;;
    --state-load-delay)
      shift
      state_load_delay="${1:-}"
      ;;
    --state-pause-delay)
      shift
      state_pause_delay="${1:-}"
      ;;
    --state-shot-delay)
      shift
      state_shot_delay="${1:-}"
      ;;
    --state-close-delay)
      shift
      state_close_delay="${1:-}"
      ;;
    --state-cmd)
      shift
      state_cmd="${1:-}"
      ;;
    --state-pause)
      state_pause="1"
      ;;
    --no-state-pause)
      state_pause="0"
      ;;
    --port)
      shift
      netcmd_port="${1:-}"
      ;;
    --core-option)
      shift
      core_option_overrides+=("${1:-}")
      ;;
    --debug-hires)
      debug_hires="1"
      ;;
    --no-debug-hires)
      debug_hires="0"
      ;;
    --fullscreen)
      force_fullscreen="1"
      ;;
    --no-fullscreen)
      force_fullscreen="0"
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

if [[ "$smoke_mode" != "buttons" && "$smoke_mode" != "state" ]]; then
  echo "--smoke-mode must be 'buttons' or 'state': $smoke_mode" >&2
  exit 1
fi

if [[ ! -x "$SMOKE_START_RUNNER" ]]; then
  echo "run-n64-smoke-start.sh is missing or not executable: $SMOKE_START_RUNNER" >&2
  exit 1
fi

if [[ ! -x "$SMOKE_STATE_RUNNER" ]]; then
  echo "run-n64-smoke-state.sh is missing or not executable: $SMOKE_STATE_RUNNER" >&2
  exit 1
fi

if [[ ! -f "$DEFAULT_CORE_OPTIONS_FILE" ]]; then
  echo "Missing core options file: $DEFAULT_CORE_OPTIONS_FILE" >&2
  exit 1
fi

if ! command -v nc >/dev/null 2>&1; then
  echo "nc (netcat) is required for RetroArch network commands." >&2
  exit 1
fi

if ! is_nonnegative_number "$start_delay"; then
  echo "--start-delay must be a non-negative number: $start_delay" >&2
  exit 1
fi

if ! is_nonnegative_number "$post_delay"; then
  echo "--post-delay must be a non-negative number: $post_delay" >&2
  exit 1
fi

if ! is_positive_int "$input_interval"; then
  echo "--interval must be a positive integer: $input_interval" >&2
  exit 1
fi

if ! is_positive_int "$button_hold_ms"; then
  echo "--button-hold-ms must be a positive integer: $button_hold_ms" >&2
  exit 1
fi

if ! is_positive_int "$max_presses"; then
  echo "--max-presses must be a positive integer: $max_presses" >&2
  exit 1
fi

if ! is_nonnegative_number "$screenshot_at"; then
  echo "--screenshot-at must be a non-negative number: $screenshot_at" >&2
  exit 1
fi

if ! is_nonnegative_number "$state_load_delay"; then
  echo "--state-load-delay must be a non-negative number: $state_load_delay" >&2
  exit 1
fi

if ! is_nonnegative_number "$state_pause_delay"; then
  echo "--state-pause-delay must be a non-negative number: $state_pause_delay" >&2
  exit 1
fi

if ! is_nonnegative_number "$state_shot_delay"; then
  echo "--state-shot-delay must be a non-negative number: $state_shot_delay" >&2
  exit 1
fi

if ! is_nonnegative_number "$state_close_delay"; then
  echo "--state-close-delay must be a non-negative number: $state_close_delay" >&2
  exit 1
fi

if ! is_positive_int "$netcmd_port"; then
  echo "--port must be a positive integer: $netcmd_port" >&2
  exit 1
fi

if [[ -z "$tag" ]]; then
  tag="$(date +%Y%m%d-%H%M%S)"
fi

capture_dir="$capture_root/$tag"
mkdir -p "$capture_dir"

log_file="$capture_dir/run.log"
core_options_file="$capture_dir/ParaLLEl N64.opt"
cp "$DEFAULT_CORE_OPTIONS_FILE" "$core_options_file"
apply_core_option_overrides
write_capture_appendconfig
stamp_file="$(mktemp /tmp/parallel-n64-paper-mario-shot-stamp.XXXX)"

trap cleanup EXIT

declare -a smoke_cmd=()
if [[ "$smoke_mode" == "buttons" ]]; then
  smoke_cmd+=("$SMOKE_START_RUNNER")
  smoke_cmd+=("--start-delay" "$start_delay")
  smoke_cmd+=("--post-delay" "$post_delay")
  smoke_cmd+=("--interval" "$input_interval")
  smoke_cmd+=("--button-hold-ms" "$button_hold_ms")
  smoke_cmd+=("--buttons" "$buttons_csv")
  smoke_cmd+=("--max-presses" "$max_presses")
else
  smoke_cmd+=("$SMOKE_STATE_RUNNER")
  smoke_cmd+=("--load-delay" "$state_load_delay")
  smoke_cmd+=("--pause-delay" "$state_pause_delay")
  smoke_cmd+=("--shot-delay" "$state_shot_delay")
  smoke_cmd+=("--close-delay" "$state_close_delay")
  smoke_cmd+=("--state-cmd" "$state_cmd")
  if [[ "$state_pause" == "1" ]]; then
    smoke_cmd+=(--pause)
  else
    smoke_cmd+=(--no-pause)
  fi
fi
smoke_cmd+=("--rom-dir" "$rom_dir" "$rom_path")
if [[ "$force_fullscreen" == "0" ]]; then
  smoke_cmd+=("--no-fullscreen")
fi
if ((${#runner_args[@]} > 0)); then
  smoke_cmd+=("${runner_args[@]}")
fi
smoke_cmd+=(-- --appendconfig "$capture_cfg")

echo "Capture dir: $capture_dir"
echo "Log file: $log_file"
echo "Temp core options: $core_options_file"
if [[ "$smoke_mode" == "buttons" ]]; then
  echo "Smoke mode: buttons"
  echo "Screenshot timing: +${screenshot_at}s"
else
  echo "Smoke mode: state"
  echo "State load timing: +${state_load_delay}s, screenshot +${state_shot_delay}s after load/pause"
fi

(
  export RUN_N64_FULLSCREEN="$force_fullscreen"
  export RUN_N64_MAXIMIZE=1
  export RUN_N64_CORE_OPTIONS_FILE="$core_options_file"
  if [[ "$debug_hires" == "1" ]]; then
    export PARALLEL_RDP_HIRES_DEBUG=1
  fi
  "${smoke_cmd[@]}"
) >"$log_file" 2>&1 &
run_pid="$!"

if [[ "$smoke_mode" == "buttons" ]]; then
  sleep "$screenshot_at"
  if kill -0 "$run_pid" 2>/dev/null; then
    if send_netcmd "SCREENSHOT"; then
      echo "NetCmd: sent 'SCREENSHOT'"
    else
      echo "NetCmd failed: 'SCREENSHOT'" >&2
    fi
  fi
fi

rc=0
if ! wait "$run_pid"; then
  rc=$?
fi

if (( rc == 130 || rc == 143 )); then
  rc=0
fi

latest_screenshot="$(find "$capture_dir" -maxdepth 1 -type f -name '*.png' -printf '%T@ %p\n' | sort -nr | head -n 1 | cut -d' ' -f2-)"
if [[ -z "$latest_screenshot" && -d "$DEFAULT_SCREENSHOT_DIR" ]]; then
  latest_screenshot="$(find "$DEFAULT_SCREENSHOT_DIR" -maxdepth 1 -type f -name '*.png' -newer "$stamp_file" -printf '%T@ %p\n' | sort -nr | head -n 1 | cut -d' ' -f2-)"
  if [[ -n "$latest_screenshot" ]]; then
    copied_screenshot="$capture_dir/$(basename "$latest_screenshot")"
    cp "$latest_screenshot" "$copied_screenshot"
    latest_screenshot="$copied_screenshot"
  fi
fi
if [[ -z "$latest_screenshot" ]]; then
  echo "No screenshot produced under $capture_dir" >&2
  exit 1
fi

echo "Screenshot: $latest_screenshot"
echo "Capture dir: $capture_dir"
echo "Log file: $log_file"

exit "$rc"
