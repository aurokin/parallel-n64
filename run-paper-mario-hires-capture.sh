#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
SMOKE_RUNNER="$SCRIPT_DIR/run-n64-smoke-start.sh"
DEFAULT_ROM_DIR="/home/auro/code/n64_roms"
DEFAULT_ROM_NAME="Paper Mario (USA).zip"
DEFAULT_CORE_OPTIONS_FILE="$HOME/.config/retroarch/config/ParaLLEl N64/ParaLLEl N64.opt"
DEFAULT_SCREENSHOT_DIR="$HOME/.config/retroarch/screenshots"

start_delay=20
post_delay=10
input_interval=5
button_hold_ms=140
max_presses=2
buttons_csv="start"
screenshot_at=27
netcmd_port=55355
rom_dir="$DEFAULT_ROM_DIR"
rom_path="$DEFAULT_ROM_NAME"
capture_root="${TMPDIR:-/tmp}/parallel-n64-paper-mario-captures"
tag=""
debug_hires="0"
force_fullscreen="${RUN_N64_FULLSCREEN:-1}"

capture_cfg=""
capture_dir=""
log_file=""
core_options_file=""
stamp_file=""
run_pid=""
declare -a runner_args=()

usage() {
  cat <<'EOF_USAGE'
Usage:
  run-paper-mario-hires-capture.sh [options] [-- RUN_N64_ARGS...]

Options:
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
  --port PORT             RetroArch network command UDP port (default: 55355)
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
  - Uses run-n64-smoke-start.sh for deterministic Paper Mario boot/input:
    boot, wait 20s, press start, wait 5s, press start, wait 2s.
  - Sends RetroArch's SCREENSHOT network command at --screenshot-at seconds.
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
    --port)
      shift
      netcmd_port="${1:-}"
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

if [[ ! -x "$SMOKE_RUNNER" ]]; then
  echo "run-n64-smoke-start.sh is missing or not executable: $SMOKE_RUNNER" >&2
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
write_capture_appendconfig
stamp_file="$(mktemp /tmp/parallel-n64-paper-mario-shot-stamp.XXXX)"

trap cleanup EXIT

declare -a smoke_cmd=()
smoke_cmd+=("$SMOKE_RUNNER")
smoke_cmd+=("--start-delay" "$start_delay")
smoke_cmd+=("--post-delay" "$post_delay")
smoke_cmd+=("--interval" "$input_interval")
smoke_cmd+=("--button-hold-ms" "$button_hold_ms")
smoke_cmd+=("--buttons" "$buttons_csv")
smoke_cmd+=("--max-presses" "$max_presses")
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
echo "Screenshot timing: +${screenshot_at}s"

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

sleep "$screenshot_at"
if kill -0 "$run_pid" 2>/dev/null; then
  if send_netcmd "SCREENSHOT"; then
    echo "NetCmd: sent 'SCREENSHOT'"
  else
    echo "NetCmd failed: 'SCREENSHOT'" >&2
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
