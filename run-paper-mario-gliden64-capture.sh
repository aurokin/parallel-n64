#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
VPAD_TOOL="$SCRIPT_DIR/tools/virtual_gamepad.py"
DEFAULT_RETROARCH="/home/auro/code/mupen/RetroArch-upstream/retroarch"
DEFAULT_CORE="/tmp/mupen64plus-libretro-nx-audit/mupen64plus_next_libretro.so"
DEFAULT_ROM_DIR="/home/auro/code/n64_roms"
DEFAULT_ROM_NAME="Paper Mario (USA).zip"
DEFAULT_RETROARCH_CFG="$HOME/.config/retroarch/retroarch.cfg"
DEFAULT_SYSTEM_DIR="$HOME/.config/retroarch/system"
DEFAULT_SCREENSHOT_DIR="$HOME/.config/retroarch/screenshots"

start_delay=20
post_delay=10
input_interval=5
button_hold_ms=140
max_presses=2
buttons_csv="start"
screenshot_at=27
netcmd_port=55355
capture_root="${TMPDIR:-/tmp}/parallel-n64-paper-mario-gliden64"
tag=""
retroarch_bin="${RETROARCH_BIN:-$DEFAULT_RETROARCH}"
core_path="${GLIDEN64_CORE:-$DEFAULT_CORE}"
rom_dir="$DEFAULT_ROM_DIR"
rom_path="$DEFAULT_ROM_NAME"
retroarch_cfg_src="$DEFAULT_RETROARCH_CFG"
system_dir="$DEFAULT_SYSTEM_DIR"
force_fullscreen="${RUN_N64_FULLSCREEN:-1}"
enable_maximize="${RUN_N64_MAXIMIZE:-1}"
vpad_socket="/tmp/parallel-n64-vpad-gliden64.sock"
hires_mode="on"
vpad_device_name="parallel-n64 Virtual Pad"

capture_dir=""
xdg_root=""
retroarch_cfg=""
core_options_file=""
log_file=""
stamp_file=""
run_pid=""
vpad_pid=""
vpad_log=""
gliden64_cache_dir=""
gliden64_cache_path=""
gliden64_cache_backup=""
gliden64_pack_source=""
declare -a buttons=()

usage() {
  cat <<'EOF_USAGE'
Usage:
  run-paper-mario-gliden64-capture.sh [options]

Options:
  --tag NAME              Capture subdirectory name (default: timestamp)
  --capture-root PATH     Root directory for screenshots/logs
  --retroarch PATH        RetroArch binary path
  --core PATH             GLideN64-capable libretro core path
  --rom PATH              ROM path passed to RetroArch (default: Paper Mario (USA).zip)
  --rom-dir PATH          ROM base directory (default: /home/auro/code/n64_roms)
  --retroarch-cfg PATH    RetroArch config template to copy into temp XDG root
  --system-dir PATH       RetroArch system directory with hi-res pack
  --start-delay SEC       Seconds before input automation begins (default: 20)
  --post-delay SEC        Seconds to keep running after first input tick (default: 10)
  --interval SEC          Seconds between input ticks (default: 5)
  --button-hold-ms MS     Milliseconds to hold each button tap (default: 140)
  --buttons CSV           Buttons to tap each tick (default: start)
  --max-presses N         Cap total input ticks (default: 2)
  --screenshot-at SEC     Seconds after launch to send SCREENSHOT (default: 27)
  --port PORT             RetroArch network command UDP port (default: 55355)
  --hires-on              Enable GLideN64 hi-res textures and staged cache (default)
  --hires-off             Disable GLideN64 hi-res textures for native-scaling captures
  --fullscreen            Force fullscreen launch (default)
  --no-fullscreen         Disable fullscreen launch
  -h, --help              Show this help

Behavior:
  - Uses an isolated XDG config root for RetroArch config and per-core options.
  - Always enables GLideN64 4x native resolution.
  - `--hires-on` enables hi-res textures and stages GLideN64's hi-res storage file under system/Mupen64plus/cache/.
  - `--hires-off` disables hi-res texture replacement and skips pack staging.
  - Drives Paper Mario with the virtual pad path: boot, wait 20s, press start, wait 5s, press start, wait 2s.
  - Sends RetroArch's SCREENSHOT command at --screenshot-at seconds and stores the result in the capture directory.
EOF_USAGE
}

is_nonnegative_number() {
  [[ "$1" =~ ^([0-9]+([.][0-9]+)?|[.][0-9]+)$ ]]
}

is_positive_int() {
  [[ "$1" =~ ^[1-9][0-9]*$ ]]
}

split_buttons() {
  local raw="$1"
  local token=""
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

send_netcmd() {
  local cmd="$1"
  printf '%s\n' "$cmd" | nc -u -w1 127.0.0.1 "$netcmd_port" >/dev/null 2>&1
}

stop_stale_vpad_daemons() {
  pkill -f 'tools/virtual_gamepad.py daemon --socket /tmp/parallel-n64-vpad-smoke' >/dev/null 2>&1 || true
  pkill -f 'tools/virtual_gamepad.py daemon --socket /tmp/parallel-n64-vpad-gliden64' >/dev/null 2>&1 || true
}

derive_vpad_device_name() {
  local suffix="${vpad_socket##*/}"
  suffix="${suffix#parallel-n64-vpad-gliden64.}"
  suffix="${suffix%.sock}"
  vpad_device_name="parallel-n64 Virtual Pad glide-${suffix}"
}

ensure_retroarch_setting() {
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

start_maximize_helper() {
  if [[ "$force_fullscreen" != "0" || "$enable_maximize" == "0" ]]; then
    return
  fi

  if ! command -v xdotool >/dev/null 2>&1; then
    return
  fi

  local target_pid="$run_pid"
  (
    local tries=80
    local window_ids=""
    while (( tries > 0 )); do
      window_ids="$(xdotool search --onlyvisible --pid "$target_pid" --name "RetroArch" 2>/dev/null || true)"
      if [[ -n "$window_ids" ]]; then
        break
      fi
      sleep 0.1
      (( tries -= 1 ))
    done

    if [[ -z "$window_ids" ]]; then
      exit 0
    fi

    while read -r wid; do
      [[ -z "$wid" ]] && continue
      xdotool windowactivate "$wid" >/dev/null 2>&1 || true
      xdotool windowstate --add MAXIMIZED_VERT "$wid" >/dev/null 2>&1 || true
      xdotool windowstate --add MAXIMIZED_HORZ "$wid" >/dev/null 2>&1 || true
    done <<< "$window_ids"
  ) >/dev/null 2>&1 &
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
    sleep 0.1
    return 0
  done <<< "$window_ids"
}

start_vpad_daemon() {
  vpad_log="$(mktemp /tmp/parallel-n64-vpad-gliden64.XXXX.log)"
  stop_stale_vpad_daemons
  python3 "$VPAD_TOOL" stop --socket "$vpad_socket" >/dev/null 2>&1 || true
  python3 "$VPAD_TOOL" stop --socket "/tmp/parallel-n64-vpad-smoke.sock" >/dev/null 2>&1 || true
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

write_temp_retroarch_cfg() {
  mkdir -p "$(dirname "$retroarch_cfg")"
  cp "$retroarch_cfg_src" "$retroarch_cfg"
  ensure_retroarch_setting "$retroarch_cfg" "config_save_on_exit" "false"
  ensure_retroarch_setting "$retroarch_cfg" "global_core_options" "false"
  ensure_retroarch_setting "$retroarch_cfg" "core_options_path" ""
  ensure_retroarch_setting "$retroarch_cfg" "input_autodetect_enable" "false"
  ensure_retroarch_setting "$retroarch_cfg" "pause_nonactive" "false"
  ensure_retroarch_setting "$retroarch_cfg" "input_player1_joypad_index" "0"
  ensure_retroarch_setting "$retroarch_cfg" "input_player1_device_reservation_type" "0"
  ensure_retroarch_setting "$retroarch_cfg" "input_player1_reserved_device" "$vpad_device_name"
  ensure_retroarch_setting "$retroarch_cfg" "input_player1_a_btn" "0"
  ensure_retroarch_setting "$retroarch_cfg" "input_player1_b_btn" "1"
  ensure_retroarch_setting "$retroarch_cfg" "input_player1_x_btn" "2"
  ensure_retroarch_setting "$retroarch_cfg" "input_player1_y_btn" "3"
  ensure_retroarch_setting "$retroarch_cfg" "input_player1_l_btn" "4"
  ensure_retroarch_setting "$retroarch_cfg" "input_player1_r_btn" "5"
  ensure_retroarch_setting "$retroarch_cfg" "input_player1_l2_btn" "6"
  ensure_retroarch_setting "$retroarch_cfg" "input_player1_r2_btn" "7"
  ensure_retroarch_setting "$retroarch_cfg" "input_player1_select_btn" "8"
  ensure_retroarch_setting "$retroarch_cfg" "input_player1_start_btn" "9"
  ensure_retroarch_setting "$retroarch_cfg" "input_player1_l3_btn" "11"
  ensure_retroarch_setting "$retroarch_cfg" "input_player1_r3_btn" "12"
  ensure_retroarch_setting "$retroarch_cfg" "input_player1_up_btn" "13"
  ensure_retroarch_setting "$retroarch_cfg" "input_player1_down_btn" "14"
  ensure_retroarch_setting "$retroarch_cfg" "input_player1_left_btn" "15"
  ensure_retroarch_setting "$retroarch_cfg" "input_player1_right_btn" "16"
  ensure_retroarch_setting "$retroarch_cfg" "screenshot_directory" "$capture_dir"
  ensure_retroarch_setting "$retroarch_cfg" "screenshots_in_content_dir" "false"
  ensure_retroarch_setting "$retroarch_cfg" "sort_screenshots_by_content_enable" "false"
  ensure_retroarch_setting "$retroarch_cfg" "notification_show_screenshot" "false"
  ensure_retroarch_setting "$retroarch_cfg" "notification_show_screenshot_flash" "0"
  ensure_retroarch_setting "$retroarch_cfg" "notification_show_save_state" "false"
  ensure_retroarch_setting "$retroarch_cfg" "network_cmd_enable" "true"
  ensure_retroarch_setting "$retroarch_cfg" "network_cmd_port" "$netcmd_port"
}

write_gliden64_core_options() {
  mkdir -p "$(dirname "$core_options_file")"
  local hires_enabled="False"
  if [[ "$hires_mode" == "on" ]]; then
    hires_enabled="True"
  fi
  cat >"$core_options_file" <<EOF_OPT
mupen64plus-rdp-plugin = "gliden64"
mupen64plus-aspect = "4:3"
mupen64plus-EnableNativeResFactor = "4"
mupen64plus-EnableTextureCache = "True"
mupen64plus-EnableEnhancedTextureStorage = "$hires_enabled"
mupen64plus-txHiresEnable = "$hires_enabled"
mupen64plus-txHiresFullAlphaChannel = "$hires_enabled"
mupen64plus-EnableHiResAltCRC = "$hires_enabled"
mupen64plus-EnableEnhancedHighResStorage = "$hires_enabled"
mupen64plus-txCacheCompression = "False"
mupen64plus-MaxHiResTxVramLimit = "0"
mupen64plus-MaxTxCacheSize = "8000"
mupen64plus-GLideN64IniBehaviour = "late"
EOF_OPT
}

stage_gliden64_hires_cache() {
  mkdir -p "$gliden64_cache_dir"

  if [[ -e "$gliden64_cache_path" || -L "$gliden64_cache_path" ]]; then
    gliden64_cache_backup="$(mktemp /tmp/parallel-n64-gliden64-cache.XXXXXX)"
    mv "$gliden64_cache_path" "$gliden64_cache_backup"
  fi

  ln -s "$(readlink -f "$gliden64_pack_source")" "$gliden64_cache_path"
}

restore_gliden64_hires_cache() {
  rm -f "$gliden64_cache_path" || true

  if [[ -n "$gliden64_cache_backup" && -e "$gliden64_cache_backup" ]]; then
    mv "$gliden64_cache_backup" "$gliden64_cache_path"
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
  restore_gliden64_hires_cache

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
    --retroarch)
      shift
      retroarch_bin="${1:-}"
      ;;
    --core)
      shift
      core_path="${1:-}"
      ;;
    --rom)
      shift
      rom_path="${1:-}"
      ;;
    --rom-dir)
      shift
      rom_dir="${1:-}"
      ;;
    --retroarch-cfg)
      shift
      retroarch_cfg_src="${1:-}"
      ;;
    --system-dir)
      shift
      system_dir="${1:-}"
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
    --hires-on)
      hires_mode="on"
      ;;
    --hires-off)
      hires_mode="off"
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
    *)
      echo "Unknown option: $1" >&2
      usage >&2
      exit 2
      ;;
  esac
  shift
done

if [[ ! -x "$retroarch_bin" ]]; then
  echo "RetroArch binary not executable: $retroarch_bin" >&2
  exit 1
fi

if [[ ! -f "$core_path" ]]; then
  echo "Core file not found: $core_path" >&2
  exit 1
fi

if [[ ! -f "$retroarch_cfg_src" ]]; then
  echo "RetroArch config file not found: $retroarch_cfg_src" >&2
  exit 1
fi

if [[ "$hires_mode" == "on" && ! -f "$system_dir/PAPER MARIO_HIRESTEXTURES.hts" ]]; then
  echo "Paper Mario hi-res pack not found in system dir: $system_dir" >&2
  exit 1
fi

if [[ ! -x "$VPAD_TOOL" ]]; then
  echo "Virtual gamepad tool is missing or not executable: $VPAD_TOOL" >&2
  exit 1
fi

if ! command -v python3 >/dev/null 2>&1; then
  echo "python3 is required for virtual gamepad automation." >&2
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

if ! split_buttons "$buttons_csv"; then
  exit 1
fi

if [[ "$vpad_socket" == "/tmp/parallel-n64-vpad-gliden64.sock" ]]; then
  vpad_socket="$(mktemp -u /tmp/parallel-n64-vpad-gliden64.XXXX.sock)"
fi
derive_vpad_device_name

if [[ -z "$tag" ]]; then
  tag="$(date +%Y%m%d-%H%M%S)"
fi

capture_dir="$capture_root/$tag"
xdg_root="$capture_dir/xdg"
retroarch_cfg="$xdg_root/retroarch/retroarch.cfg"
core_options_file="$xdg_root/retroarch/config/Mupen64Plus-Next/Mupen64Plus-Next.opt"
gliden64_cache_dir="$system_dir/Mupen64plus/cache"
gliden64_cache_path="$gliden64_cache_dir/PAPER MARIO_HIRESTEXTURES.hts"
gliden64_pack_source="$system_dir/PAPER MARIO_HIRESTEXTURES.hts"
log_file="$capture_dir/run.log"
mkdir -p "$capture_dir"
stamp_file="$(mktemp /tmp/parallel-n64-gliden64-shot-stamp.XXXX)"

write_temp_retroarch_cfg
write_gliden64_core_options
if [[ "$hires_mode" == "on" ]]; then
  stage_gliden64_hires_cache
fi

trap cleanup EXIT
start_vpad_daemon

resolved_rom="$rom_path"
if [[ ! -f "$resolved_rom" ]]; then
  resolved_rom="$rom_dir/$rom_path"
fi

if [[ ! -f "$resolved_rom" ]]; then
  echo "ROM not found: $rom_path" >&2
  exit 1
fi

declare -a cmd=()
cmd+=("$retroarch_bin" "--config" "$retroarch_cfg" "--log-file" "$log_file" "--verbose" "-L" "$core_path")
if [[ "$force_fullscreen" != "0" ]]; then
  cmd+=("-f")
fi
cmd+=("$resolved_rom")

echo "Capture dir: $capture_dir"
echo "Log file: $log_file"
echo "Temp XDG root: $xdg_root"
echo "Core options: $core_options_file"
echo "GLide hires mode: $hires_mode"
if [[ "$hires_mode" == "on" ]]; then
  echo "GLide hires storage: enabled"
  echo "GLide hires cache: $gliden64_cache_path -> $(readlink -f "$gliden64_pack_source")"
else
  echo "GLide hires storage: disabled"
fi
echo "GLide native res factor: 4x"

(
  export XDG_CONFIG_HOME="$xdg_root"
  export LIBRETRO_SYSTEM_DIRECTORY="$system_dir"
  "${cmd[@]}"
) &
run_pid="$!"

start_maximize_helper

(
  elapsed=0
  while (( elapsed < start_delay )); do
    if ! kill -0 "$run_pid" 2>/dev/null; then
      exit 0
    fi
    sleep 1
    (( elapsed += 1 ))
  done

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
    (( elapsed_post += input_interval ))
    (( tick += 1 ))
  done

  while (( elapsed_post < post_delay )); do
    if ! kill -0 "$run_pid" 2>/dev/null; then
      exit 0
    fi
    sleep 1
    (( elapsed_post += 1 ))
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
wait "$helper_pid" || true

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
