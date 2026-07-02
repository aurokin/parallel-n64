#!/usr/bin/env bash
# Interactive agent-play RetroArch session.
#
# The batch adapter (retroarch_stdin_session.sh) runs a fixed command list and
# tears down. This adapter keeps one session alive so an agent can look at a
# capture, decide the next input, and send it - the play loop the project
# needs to reach mid-game scenes.
#
# No-daemon hardening:
#   - the session holds the same runtime flock as the batch adapter, so the
#     one-emulator-at-a-time rule still holds across both adapters
#   - RetroArch runs under `timeout`, so a forgotten session kills itself
#     after --ttl-seconds (default 3600); a grace watchdog sends QUIT
#     shortly before that hard kill so core end-of-run summaries flush,
#     and records the reason in logs/session.end-reason
#   - the whole chain lives in its own setsid process group recorded in
#     session.pid; `stop` QUITs politely, then kills the group
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "$SCRIPT_DIR/../.." && pwd)"
LOCK_FILE="${TMPDIR:-/tmp}/parallel-n64-retroarch-runtime.lock"

usage() {
  cat <<'EOF'
Usage:
  retroarch_interactive_session.sh start --bundle-dir D --rom R --core C [options]
  retroarch_interactive_session.sh send --bundle-dir D --command "CMD" [--ack-timeout SEC]
  retroarch_interactive_session.sh input --bundle-dir D --mask HEX [--port N] [--hold-seconds SEC | --frames N] [--analog "lx ly rx ry"]
  retroarch_interactive_session.sh screenshot --bundle-dir D
  retroarch_interactive_session.sh status --bundle-dir D
  retroarch_interactive_session.sh save-slot --bundle-dir D --slot N   (N in 1..9)
  retroarch_interactive_session.sh load-slot --bundle-dir D --slot N [--paused]
  retroarch_interactive_session.sh stop --bundle-dir D
  retroarch_interactive_session.sh doctor [--reap]   (find/kill leftover sessions; run BEFORE start)

start options:
  --mode off|on                 Hi-res mode label for generated core options (default: off)
  --core-options-template PATH  Use PATH as core options instead of parallel-n64 defaults
  --extra-append-config PATH    Extra appendconfig lines (last value wins)
  --state-source DIR            Copy DIR's contents into the bundle states dir
  --savefile-source PATH        Copy a .srm file or savefile directory into the bundle savefiles dir
  --ttl-seconds SEC             Hard session lifetime; timeout kills RetroArch (default: 3600)
  --retroarch-bin PATH          RetroArch executable
  --base-config PATH            Base RetroArch config

RetroPad mask bits for input: B=0x1 Y=0x2 SELECT=0x4 START=0x8 UP=0x10
DOWN=0x20 LEFT=0x40 RIGHT=0x80 A=0x100 X=0x200 L=0x400 R=0x800
Z/L2=0x1000 R2=0x2000 L3=0x4000 R3=0x8000

Agent-play note: the game runs in REAL TIME while you think. For
deterministic play, keep the session paused and use `input --frames N`
(TAS-style: input held for exactly N stepped frames, session stays
paused). Pause with `send --command "SET_PAUSE ON"`; the command accepts
ON, OFF, or TOGGLE. `--hold-seconds` is wall-clock and only suited to
menus and title screens.

Capture note: after LOAD_STATE_SLOT_PAUSED, step at least one frame
before `screenshot` — the loaded frame has not been presented yet and
the capture comes back black. The adapter decodes each capture and
warns (without failing) when it is uniformly black.
EOF
}

sha256_file() {
  if command -v sha256sum >/dev/null 2>&1; then
    sha256sum "$1" | awk '{print $1}'
  else
    shasum -a 256 "$1" | awk '{print $1}'
  fi
}

log_size_bytes() {
  if [[ -f "$RA_LOG" ]]; then
    wc -c < "$RA_LOG"
  else
    echo 0
  fi
}

file_size_bytes() {
  local path="$1"
  if command -v stat >/dev/null 2>&1 && stat -c %s "$path" >/dev/null 2>&1; then
    stat -c %s "$path"
  elif command -v gstat >/dev/null 2>&1; then
    gstat -c %s "$path"
  else
    python3 - "$path" <<'PY'
import os
import sys
try:
    print(os.path.getsize(sys.argv[1]))
except OSError:
    print(0)
PY
  fi
}

newest_capture_path() {
  local captures_dir="$1"
  python3 - "$captures_dir" <<'PY'
import sys
from pathlib import Path

captures_dir = Path(sys.argv[1])
files = [p for p in captures_dir.iterdir() if p.is_file()]
if not files:
    raise SystemExit(1)
print(max(files, key=lambda p: (p.stat().st_mtime_ns, p.name)))
PY
}

running_retroarch_processes() {
  ps -axo pid=,stat=,comm=,command= | awk '
    $2 !~ /^Z/ && $3 ~ /(^|\/)(RetroArch|retroarch)$/ { print }
  '
}

is_darwin() {
  [[ "$(uname -s)" == "Darwin" ]]
}

default_retroarch_bin() {
  if is_darwin; then
    local mvk141_bin="${RETROARCH_MVK141_BIN:-$REPO_ROOT/artifacts/external/RetroArch-MVK141.app/Contents/MacOS/RetroArch}"
    if [[ -x "$mvk141_bin" ]]; then
      echo "$mvk141_bin"
      return
    fi
    if [[ -x "/Applications/RetroArch.app/Contents/MacOS/RetroArch" ]]; then
      echo "/Applications/RetroArch.app/Contents/MacOS/RetroArch"
      return
    fi
  fi
  echo "/home/auro/code/RetroArch/retroarch"
}

prefer_macos_hires_retroarch_bin() {
  local mode="${1:-off}"
  local retroarch_bin="${2:-}"
  local explicit="${3:-0}"
  if ! is_darwin || [[ "$mode" != "on" || "$explicit" == "1" ]]; then
    echo "$retroarch_bin"
    return
  fi

  local mvk141_bin="${RETROARCH_MVK141_BIN:-$REPO_ROOT/artifacts/external/RetroArch-MVK141.app/Contents/MacOS/RetroArch}"
  if [[ -x "$mvk141_bin" ]]; then
    echo "$mvk141_bin"
    return
  fi
  echo "$retroarch_bin"
}

default_base_config() {
  local mac_config="${HOME:-}/code/RetroArch/retroarch.cfg"
  if is_darwin && [[ -f "$mac_config" ]]; then
    echo "$mac_config"
  else
    echo "/home/auro/code/RetroArch/retroarch.cfg"
  fi
}

apply_macos_runtime_defaults() {
  if ! is_darwin; then
    return
  fi

  local mode="${1:-off}"
  local retroarch_bin="${2:-}"
  local mvk141_bin="${RETROARCH_MVK141_BIN:-$REPO_ROOT/artifacts/external/RetroArch-MVK141.app/Contents/MacOS/RetroArch}"
  local argument_buffers_default="0"
  # Recognize the MVK141 bundle by the launched path too, not only by the
  # REPO_ROOT-rooted default: when this script runs as a copy inside an
  # exported eval workspace, REPO_ROOT is the workspace parent and the
  # equality can never hold — hi-res then silently loses argument buffers
  # and the compute pipeline fails ("bind texture 0-65535 above limit 128";
  # found via the pilot-gpt55 replay, 2026-07-02).
  if [[ "$mode" == "on" && ( "$retroarch_bin" == "$mvk141_bin" \
        || "$retroarch_bin" == *"/RetroArch-MVK141.app/"* ) ]]; then
    argument_buffers_default="1"
  fi

  export MVK_CONFIG_USE_METAL_ARGUMENT_BUFFERS="${MVK_CONFIG_USE_METAL_ARGUMENT_BUFFERS:-$argument_buffers_default}"
  if [[ "$mode" != "on" ]]; then
    export PARALLEL_RDP_DISABLE_HIRES_SHADER="${PARALLEL_RDP_DISABLE_HIRES_SHADER:-1}"
  fi
}

start_session_leader() {
  local pid_file="$1" lock_file="$2" ttl_seconds="$3" retroarch_bin="$4"
  local base_config="$5" append_config="$6" core_path="$7" rom_path="$8"
  local fifo_path="$9" ra_log="${10}"

  if command -v setsid >/dev/null 2>&1; then
    setsid bash -c '
      echo "$$" > "$1"
      exec flock -n "$2" timeout --signal=TERM "$3" "$4" \
        --verbose --config "$5" --appendconfig "$6" -L "$7" "$8" \
        0<> "$9" >> "${10}" 2>&1
    ' _ "$pid_file" "$lock_file" "$ttl_seconds" "$retroarch_bin" \
        "$base_config" "$append_config" "$core_path" "$rom_path" \
        "$fifo_path" "$ra_log" &
  else
    python3 - "$pid_file" "$lock_file" "$ttl_seconds" "$retroarch_bin" \
        "$base_config" "$append_config" "$core_path" "$rom_path" \
        "$fifo_path" "$ra_log" <<'PY' &
import os
import sys
from pathlib import Path

(
    pid_file,
    lock_file,
    ttl_seconds,
    retroarch_bin,
    base_config,
    append_config,
    core_path,
    rom_path,
    fifo_path,
    ra_log,
) = sys.argv[1:]

os.setsid()
Path(pid_file).write_text(f"{os.getpid()}\n")

fifo_fd = os.open(fifo_path, os.O_RDWR)
log_fd = os.open(ra_log, os.O_WRONLY | os.O_CREAT | os.O_APPEND, 0o666)
os.dup2(fifo_fd, 0)
os.dup2(log_fd, 1)
os.dup2(log_fd, 2)
for fd in (fifo_fd, log_fd):
    if fd > 2:
        os.close(fd)

os.execvp(
    "flock",
    [
        "flock",
        "-n",
        lock_file,
        "timeout",
        "--signal=TERM",
        ttl_seconds,
        retroarch_bin,
        "--verbose",
        "--config",
        base_config,
        "--appendconfig",
        append_config,
        "-L",
        core_path,
        rom_path,
    ],
)
PY
  fi
}

wait_for_log_pattern_after() {
  local start_bytes="$1" pattern="$2" timeout_seconds="$3"
  local deadline=$(( $(date +%s) + timeout_seconds ))
  while (( $(date +%s) < deadline )); do
    if [[ -f "$RA_LOG" ]] && tail -c +"$((start_bytes + 1))" "$RA_LOG" 2>/dev/null | rg -F -q -- "$pattern"; then
      return 0
    fi
    sleep 0.2
  done
  return 1
}

send_fifo() {
  printf '%s\n' "$1" > "$FIFO_PATH"
  printf '%s\n' "$1" >> "$BUNDLE_DIR/logs/interactive.commands.log"
}

session_paths() {
  FIFO_PATH="$BUNDLE_DIR/retroarch.stdin"
  RA_LOG="$BUNDLE_DIR/logs/retroarch.log"
  SESSION_ENV="$BUNDLE_DIR/retroarch.session.env"
  PID_FILE="$BUNDLE_DIR/session.pid"
  SLOT_FILE="$BUNDLE_DIR/session.state-slot"
  END_REASON_FILE="$BUNDLE_DIR/logs/session.end-reason"
}

record_end_reason() {
  printf '%s %s\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)" "$1" >> "$END_REASON_FILE"
}

# Evidence-contract hardening: a TTL kill used to be a silent hard stop
# that lost the core's end-of-run summaries. This watchdog (outside the
# session process group, so group liveness still means "RetroArch is
# alive") asks RetroArch to QUIT shortly before the hard `timeout` kill
# and records the reason, turning the TTL into an explicit, reported
# fallback. If the QUIT doesn't land, the original hard kill still fires.
start_ttl_grace_watchdog() {
  local pgid="$1" launch_epoch="$2" ttl_seconds="$3"
  local grace="${RETROARCH_TTL_GRACE_SECONDS:-30}"
  (( ttl_seconds <= 2 * grace )) && grace=$(( ttl_seconds / 3 ))
  (( grace < 3 )) && return 0
  local fire_at=$(( launch_epoch + ttl_seconds - grace ))
  (
    while (( $(date +%s) < fire_at )); do
      sleep 5
      kill -0 -- "-$pgid" 2>/dev/null || exit 0
    done
    kill -0 -- "-$pgid" 2>/dev/null || exit 0
    record_end_reason "ttl-grace-quit: sent QUIT ${grace}s before the hard TTL kill (ttl=${ttl_seconds}s)"
    if [[ -p "$FIFO_PATH" ]]; then
      timeout 10 bash -c 'printf "QUIT\n" > "$1"' _ "$FIFO_PATH" 2>/dev/null || true
      printf 'QUIT\n' >> "$BUNDLE_DIR/logs/interactive.commands.log"
    fi
  ) </dev/null >/dev/null 2>&1 &
  disown 2>/dev/null || true
}

require_live_session() {
  session_paths
  if [[ ! -f "$PID_FILE" ]]; then
    echo "No session.pid in $BUNDLE_DIR - is the session started?" >&2
    exit 1
  fi
  PGID="$(cat "$PID_FILE")"
  if ! kill -0 -- "-$PGID" 2>/dev/null; then
    echo "Session process group $PGID is not running (TTL expired or stopped)." >&2
    exit 1
  fi
  if [[ ! -p "$FIFO_PATH" ]]; then
    echo "Session FIFO missing: $FIFO_PATH" >&2
    exit 1
  fi
}

ack_for_command() {
  # Map a verb to its log ack pattern; empty means fire-and-forget.
  case "$1" in
    PING*) echo "PING OK" ;;
    GET_STATUS*) echo "GET_STATUS " ;;
    SET_PAUSE*) echo "SET_PAUSE " ;;
    STEP_FRAME*) echo "STEP_FRAME " ;;
    SET_INPUT_PORT*) echo "SET_INPUT_PORT " ;;
    CLEAR_INPUT_PORT*) echo "CLEAR_INPUT_PORT " ;;
    GET_INPUT_PORT*) echo "GET_INPUT_PORT " ;;
    READ_CORE_MEMORY*) echo "READ_CORE_MEMORY " ;;
    LOAD_STATE_SLOT*) echo "[State] Loading state" ;;
    WAIT_SAVE_STATE*) echo "WAIT_SAVE_STATE DONE" ;;
    SAVE_STATE*) echo "[State] Saving state" ;;
    *) echo "" ;;
  esac
}

cmd_start() {
  local DEFAULT_RETROARCH_BIN DEFAULT_BASE_CONFIG
  DEFAULT_RETROARCH_BIN="$(default_retroarch_bin)"
  DEFAULT_BASE_CONFIG="$(default_base_config)"

  local MODE="off" RETROARCH_BIN="${RETROARCH_BIN:-$DEFAULT_RETROARCH_BIN}"
  local BASE_CONFIG="${BASE_CONFIG:-$DEFAULT_BASE_CONFIG}"
  local ROM_PATH="" CORE_PATH="" CORE_OPTIONS_TEMPLATE="" EXTRA_APPEND_CONFIG=""
  local STATE_SOURCE="" SAVEFILE_SOURCE="" TTL_SECONDS=3600 RETROARCH_BIN_EXPLICIT=0

  while (($#)); do
    case "$1" in
      --bundle-dir) shift; BUNDLE_DIR="${1:-}" ;;
      --rom) shift; ROM_PATH="${1:-}" ;;
      --core) shift; CORE_PATH="${1:-}" ;;
      --mode) shift; MODE="${1:-}" ;;
      --core-options-template) shift; CORE_OPTIONS_TEMPLATE="${1:-}" ;;
      --extra-append-config) shift; EXTRA_APPEND_CONFIG="${1:-}" ;;
      --state-source) shift; STATE_SOURCE="${1:-}" ;;
      --savefile-source) shift; SAVEFILE_SOURCE="${1:-}" ;;
      --ttl-seconds) shift; TTL_SECONDS="${1:-}" ;;
      --retroarch-bin) shift; RETROARCH_BIN="${1:-}"; RETROARCH_BIN_EXPLICIT=1 ;;
      --base-config) shift; BASE_CONFIG="${1:-}" ;;
      *) echo "Unknown start option: $1" >&2; exit 2 ;;
    esac
    shift
  done

  if [[ -z "${BUNDLE_DIR:-}" || -z "$ROM_PATH" || -z "$CORE_PATH" ]]; then
    echo "start requires --bundle-dir, --rom, and --core." >&2
    exit 2
  fi
  RETROARCH_BIN="$(prefer_macos_hires_retroarch_bin "$MODE" "$RETROARCH_BIN" "$RETROARCH_BIN_EXPLICIT")"
  for f in "$ROM_PATH" "$CORE_PATH" "$BASE_CONFIG"; do
    if [[ ! -f "$f" ]]; then
      echo "Not found: $f" >&2
      exit 1
    fi
  done
  if [[ -n "$CORE_OPTIONS_TEMPLATE" && ! -f "$CORE_OPTIONS_TEMPLATE" ]]; then
    echo "Core options template not found: $CORE_OPTIONS_TEMPLATE" >&2
    exit 1
  fi
  if [[ -n "$EXTRA_APPEND_CONFIG" && ! -f "$EXTRA_APPEND_CONFIG" ]]; then
    echo "Extra append config not found: $EXTRA_APPEND_CONFIG" >&2
    exit 1
  fi
  apply_macos_runtime_defaults "$MODE" "$RETROARCH_BIN"

  # Same singleton rule as the batch adapter.
  local matches
  matches="$(running_retroarch_processes || true)"
  if [[ -n "$matches" ]]; then
    echo "Another RetroArch process is already running:" >&2
    printf '%s\n' "$matches" >&2
    exit 1
  fi

  session_paths
  mkdir -p \
    "$BUNDLE_DIR"/captures \
    "$BUNDLE_DIR"/logs \
    "$BUNDLE_DIR"/playlists/builtin \
    "$BUNDLE_DIR"/playlists/logs \
    "$BUNDLE_DIR"/records \
    "$BUNDLE_DIR"/records_config \
    "$BUNDLE_DIR"/states \
    "$BUNDLE_DIR"/savefiles \
    "$BUNDLE_DIR"/system

  if [[ -n "$STATE_SOURCE" ]]; then
    if [[ ! -d "$STATE_SOURCE" ]]; then
      echo "State source not found: $STATE_SOURCE" >&2
      exit 1
    fi
    if [[ "$(basename "$STATE_SOURCE")" == "ParaLLEl N64" ]] &&
        find "$STATE_SOURCE" -maxdepth 1 -type f -name '*.state*' -print -quit | rg -q .; then
      mkdir -p "$BUNDLE_DIR/states/$(basename "$STATE_SOURCE")"
      cp -r "$STATE_SOURCE"/. "$BUNDLE_DIR/states/$(basename "$STATE_SOURCE")/"
    else
      cp -r "$STATE_SOURCE"/. "$BUNDLE_DIR/states/"
    fi
  fi
  if [[ -n "$SAVEFILE_SOURCE" ]]; then
    if [[ -f "$SAVEFILE_SOURCE" ]]; then
      mkdir -p "$BUNDLE_DIR/savefiles/ParaLLEl N64"
      cp "$SAVEFILE_SOURCE" "$BUNDLE_DIR/savefiles/ParaLLEl N64/$(basename "${ROM_PATH%.*}").srm"
    elif [[ -d "$SAVEFILE_SOURCE" ]]; then
      if [[ "$(basename "$SAVEFILE_SOURCE")" == "ParaLLEl N64" ]] &&
          find "$SAVEFILE_SOURCE" -maxdepth 1 -type f -name '*.srm' -print -quit | rg -q .; then
        mkdir -p "$BUNDLE_DIR/savefiles/$(basename "$SAVEFILE_SOURCE")"
        cp -r "$SAVEFILE_SOURCE"/. "$BUNDLE_DIR/savefiles/$(basename "$SAVEFILE_SOURCE")/"
      else
        cp -r "$SAVEFILE_SOURCE"/. "$BUNDLE_DIR/savefiles/"
      fi
    else
      echo "Savefile source not found: $SAVEFILE_SOURCE" >&2
      exit 1
    fi
  fi

  local APPEND_CONFIG="$BUNDLE_DIR/retroarch.append.cfg"
  local CORE_OPTIONS_FILE="$BUNDLE_DIR/core-options.opt"
  local VIDEO_FULLSCREEN_DEFAULT="true"
  local VIDEO_WINDOWED_FULLSCREEN_DEFAULT="true"
  local VIDEO_WINDOW_SIZE_CONFIG_DEFAULT="false"
  if is_darwin; then
    VIDEO_FULLSCREEN_DEFAULT="false"
    VIDEO_WINDOWED_FULLSCREEN_DEFAULT="false"
    VIDEO_WINDOW_SIZE_CONFIG_DEFAULT="true"
  fi
  local VIDEO_DRIVER_VALUE="${RETROARCH_VIDEO_DRIVER_OVERRIDE:-vulkan}"
  local VIDEO_FULLSCREEN_VALUE="${RETROARCH_VIDEO_FULLSCREEN_OVERRIDE:-$VIDEO_FULLSCREEN_DEFAULT}"
  local VIDEO_WINDOWED_FULLSCREEN_VALUE="${RETROARCH_VIDEO_WINDOWED_FULLSCREEN_OVERRIDE:-$VIDEO_WINDOWED_FULLSCREEN_DEFAULT}"
  local VIDEO_WINDOW_SIZE_CONFIG_VALUE="${RETROARCH_VIDEO_WINDOW_SIZE_CONFIG_OVERRIDE:-$VIDEO_WINDOW_SIZE_CONFIG_DEFAULT}"
  local VIDEO_WINDOW_WIDTH_VALUE="${RETROARCH_VIDEO_WINDOW_WIDTH_OVERRIDE:-1920}"
  local VIDEO_WINDOW_HEIGHT_VALUE="${RETROARCH_VIDEO_WINDOW_HEIGHT_OVERRIDE:-1080}"

  # Mirrors the batch adapter's tracked-session profile (see its README notes).
  cat > "$APPEND_CONFIG" <<EOF
core_options_path = "$CORE_OPTIONS_FILE"
global_core_options = "true"
game_specific_options = "false"
config_save_on_exit = "false"
stdin_cmd_enable = "true"
network_cmd_enable = "false"
confirm_quit = "false"
pause_nonactive = "false"
state_slot = "0"
savestate_directory = "$BUNDLE_DIR/states"
savefile_directory = "$BUNDLE_DIR/savefiles"
screenshot_directory = "$BUNDLE_DIR/captures"
playlist_directory = "$BUNDLE_DIR/playlists"
content_favorites_path = "$BUNDLE_DIR/playlists/builtin/content_favorites.lpl"
content_history_path = "$BUNDLE_DIR/playlists/builtin/content_history.lpl"
content_image_history_path = "$BUNDLE_DIR/playlists/builtin/content_image_history.lpl"
content_music_history_path = "$BUNDLE_DIR/playlists/builtin/content_music_history.lpl"
content_video_history_path = "$BUNDLE_DIR/playlists/builtin/content_video_history.lpl"
runtime_log_directory = "$BUNDLE_DIR/playlists/logs"
system_directory = "$BUNDLE_DIR/system"
log_dir = "$BUNDLE_DIR/logs"
recording_config_directory = "$BUNDLE_DIR/records_config"
recording_output_directory = "$BUNDLE_DIR/records"
savestate_thumbnail_enable = "false"
menu_enable_widgets = "false"
notification_show_save_state = "false"
notification_show_screenshot = "false"
notification_show_screenshot_flash = "0"
video_driver = "$VIDEO_DRIVER_VALUE"
video_fullscreen = "$VIDEO_FULLSCREEN_VALUE"
video_windowed_fullscreen = "$VIDEO_WINDOWED_FULLSCREEN_VALUE"
video_fullscreen_x = "0"
video_fullscreen_y = "0"
EOF
  if [[ "$VIDEO_WINDOW_SIZE_CONFIG_VALUE" == "true" ]]; then
    cat >> "$APPEND_CONFIG" <<EOF
aspect_ratio_index = "22"
video_aspect_ratio_auto = "false"
video_force_aspect = "true"
custom_viewport_width = "1440"
custom_viewport_height = "1080"
custom_viewport_x = "240"
custom_viewport_y = "0"
video_window_custom_size_enable = "true"
video_windowed_position_width = "$VIDEO_WINDOW_WIDTH_VALUE"
video_windowed_position_height = "$VIDEO_WINDOW_HEIGHT_VALUE"
video_window_auto_width_max = "$VIDEO_WINDOW_WIDTH_VALUE"
video_window_auto_height_max = "$VIDEO_WINDOW_HEIGHT_VALUE"
EOF
  fi
  if [[ -n "$EXTRA_APPEND_CONFIG" ]]; then
    cat "$EXTRA_APPEND_CONFIG" >> "$APPEND_CONFIG"
  fi

  if [[ -n "$CORE_OPTIONS_TEMPLATE" ]]; then
    cp "$CORE_OPTIONS_TEMPLATE" "$CORE_OPTIONS_FILE"
  else
    local HIRES_VALUE="disabled"
    [[ "$MODE" == "on" ]] && HIRES_VALUE="enabled"
    local GFXPLUGIN_VALUE="${PARALLEL_N64_GFX_PLUGIN_OVERRIDE:-${PARALLEL_N64_GFXPLUGIN_OVERRIDE:-parallel}}"
    local UPSCALING_VALUE="${PARALLEL_RDP_UPSCALING_OVERRIDE:-4x}"
    local NATIVE_TEXRECT_VALUE="${PARALLEL_RDP_NATIVE_TEXRECT_OVERRIDE:-enabled}"
    local CPUCORE_VALUE="${PARALLEL_N64_CPUCORE_OVERRIDE:-}"
    local RSPPLUGIN_VALUE="${PARALLEL_N64_RSPPLUGIN_OVERRIDE:-}"
    if [[ -n "$CPUCORE_VALUE" ]]; then
      printf 'parallel-n64-cpucore = "%s"\n' "$CPUCORE_VALUE" > "$CORE_OPTIONS_FILE"
    else
      : > "$CORE_OPTIONS_FILE"
    fi
    cat >> "$CORE_OPTIONS_FILE" <<EOF
parallel-n64-gfxplugin = "$GFXPLUGIN_VALUE"
parallel-n64-parallel-rdp-upscaling = "$UPSCALING_VALUE"
parallel-n64-parallel-rdp-hirestex = "$HIRES_VALUE"
parallel-n64-parallel-rdp-native-tex-rect = "$NATIVE_TEXRECT_VALUE"
parallel-n64-parallel-rdp-native-texture-lod = "enabled"
EOF
    if [[ -n "$RSPPLUGIN_VALUE" ]]; then
      printf 'parallel-n64-rspplugin = "%s"\n' "$RSPPLUGIN_VALUE" >> "$CORE_OPTIONS_FILE"
    fi
  fi
  local CORE_OPTIONS_LAUNCH_FILE="$BUNDLE_DIR/core-options.launch.opt"
  cp "$CORE_OPTIONS_FILE" "$CORE_OPTIONS_LAUNCH_FILE"
  local BASE_CONFIG_SHA256 APPEND_CONFIG_SHA256 CORE_OPTIONS_FILE_SHA256
  local HIRES_CACHE_PATH HIRES_CACHE_SHA256
  BASE_CONFIG_SHA256="$(sha256_file "$BASE_CONFIG")"
  APPEND_CONFIG_SHA256="$(sha256_file "$APPEND_CONFIG")"
  CORE_OPTIONS_FILE_SHA256="$(sha256_file "$CORE_OPTIONS_LAUNCH_FILE")"
  HIRES_CACHE_PATH="${PARALLEL_RDP_HIRES_CACHE_PATH:-}"
  HIRES_CACHE_SHA256="${PARALLEL_RDP_HIRES_CACHE_SHA256:-}"
  if [[ "$MODE" == "on" ]]; then
    if [[ -z "$HIRES_CACHE_PATH" ]]; then
      echo "--mode on requires PARALLEL_RDP_HIRES_CACHE_PATH to point at a staged .phrb package." >&2
      exit 2
    fi
    if [[ ! -f "$HIRES_CACHE_PATH" ]]; then
      echo "PARALLEL_RDP_HIRES_CACHE_PATH does not exist: $HIRES_CACHE_PATH" >&2
      exit 2
    fi
  fi
  if [[ -n "$HIRES_CACHE_PATH" && -f "$HIRES_CACHE_PATH" && -z "$HIRES_CACHE_SHA256" ]]; then
    HIRES_CACHE_SHA256="$(sha256_file "$HIRES_CACHE_PATH")"
  fi

  rm -f "$FIFO_PATH" "$PID_FILE"
  mkfifo "$FIFO_PATH"
  echo 0 > "$SLOT_FILE"

  # Session leader writes its own pid (== PGID of the whole chain), holds the
  # runtime lock for the session lifetime, and dies on its own after TTL.
  local LAUNCH_EPOCH
  LAUNCH_EPOCH="$(date +%s)"
  start_session_leader "$PID_FILE" "$LOCK_FILE" "$TTL_SECONDS" "$RETROARCH_BIN" \
      "$BASE_CONFIG" "$APPEND_CONFIG" "$CORE_PATH" "$ROM_PATH" \
      "$FIFO_PATH" "$RA_LOG"

  local deadline=$(( $(date +%s) + 10 ))
  while [[ ! -s "$PID_FILE" ]] && (( $(date +%s) < deadline )); do
    sleep 0.1
  done
  if [[ ! -s "$PID_FILE" ]]; then
    echo "Session leader did not start." >&2
    exit 1
  fi
  local PGID
  PGID="$(cat "$PID_FILE")"

  cat > "$SESSION_ENV" <<EOF
SESSION_KIND=interactive
SESSION_PGID=$PGID
RETROARCH_BIN=$RETROARCH_BIN
BASE_CONFIG=$BASE_CONFIG
APPEND_CONFIG=$APPEND_CONFIG
CORE_OPTIONS_FILE=$CORE_OPTIONS_FILE
CORE_OPTIONS_LAUNCH_FILE=$CORE_OPTIONS_LAUNCH_FILE
BASE_CONFIG_SHA256=$BASE_CONFIG_SHA256
APPEND_CONFIG_SHA256=$APPEND_CONFIG_SHA256
CORE_OPTIONS_FILE_SHA256=$CORE_OPTIONS_FILE_SHA256
STDIN_FIFO=$FIFO_PATH
ROM_PATH=$ROM_PATH
CORE_PATH=$CORE_PATH
ROM_SHA256=$(sha256_file "$ROM_PATH")
CORE_SHA256=$(sha256_file "$CORE_PATH")
HIRES_CACHE_PATH=$HIRES_CACHE_PATH
HIRES_CACHE_SHA256=$HIRES_CACHE_SHA256
SAVEFILE_SOURCE=$SAVEFILE_SOURCE
SAVEFILE_SOURCE_SHA256=$(if [[ -n "$SAVEFILE_SOURCE" && -f "$SAVEFILE_SOURCE" ]]; then sha256_file "$SAVEFILE_SOURCE"; fi)
MVK_CONFIG_USE_METAL_ARGUMENT_BUFFERS=${MVK_CONFIG_USE_METAL_ARGUMENT_BUFFERS:-}
PARALLEL_RDP_DISABLE_HIRES_SHADER=${PARALLEL_RDP_DISABLE_HIRES_SHADER:-}
MODE=$MODE
TTL_SECONDS=$TTL_SECONDS
EOF

  # Session boundary marker for trace consumers (replay verification, the
  # recordings/stitching lane): the commands log accumulates across restarts
  # in the same bundle, and a restart resets per-process state (savestate
  # slot counter), so replaying a multi-session trace as one session forks.
  # Not sent to the FIFO — this is not a frontend command.
  printf 'SESSION_START pgid=%s\n' "$PGID" \
    >> "$BUNDLE_DIR/logs/interactive.commands.log"

  local start_bytes=0
  deadline=$(( $(date +%s) + 120 ))
  while (( $(date +%s) < deadline )); do
    if ! kill -0 -- "-$PGID" 2>/dev/null; then
      echo "Session died during startup; see $RA_LOG" >&2
      exit 1
    fi
    start_bytes="$(log_size_bytes)"
    send_fifo "PING"
    if wait_for_log_pattern_after "$start_bytes" "PING OK" 2; then
      start_ttl_grace_watchdog "$PGID" "$LAUNCH_EPOCH" "$TTL_SECONDS"
      echo "[interactive] session ready: pgid=$PGID bundle=$BUNDLE_DIR ttl=${TTL_SECONDS}s"
      exit 0
    fi
    sleep 0.5
  done
  echo "Session did not answer PING within 120s." >&2
  exit 1
}

cmd_send() {
  local COMMAND="" ACK_TIMEOUT=10
  while (($#)); do
    case "$1" in
      --bundle-dir) shift; BUNDLE_DIR="${1:-}" ;;
      --command) shift; COMMAND="${1:-}" ;;
      --ack-timeout) shift; ACK_TIMEOUT="${1:-}" ;;
      *) echo "Unknown send option: $1" >&2; exit 2 ;;
    esac
    shift
  done
  if [[ -z "${BUNDLE_DIR:-}" || -z "$COMMAND" ]]; then
    echo "send requires --bundle-dir and --command." >&2
    exit 2
  fi
  require_live_session

  local ack start_bytes
  ack="$(ack_for_command "$COMMAND")"
  # Serialize the send+ack unit per bundle: the ack matcher takes the LAST
  # pattern match after start_bytes, so two concurrent same-verb sends (e.g.
  # a scorer polling READ_CORE_MEMORY while the agent probes RAM) can each
  # collect the other's reply. flock releases on process exit.
  exec 9>"$BUNDLE_DIR/logs/send.lock"
  if ! flock -w "$((ACK_TIMEOUT + 15))" 9; then
    echo "Timed out waiting for the bundle send lock." >&2
    exit 1
  fi
  start_bytes="$(log_size_bytes)"
  send_fifo "$COMMAND"
  if [[ -n "$ack" ]]; then
    if ! wait_for_log_pattern_after "$start_bytes" "$ack" "$ACK_TIMEOUT"; then
      echo "No ack for: $COMMAND (expected log pattern: $ack)" >&2
      exit 1
    fi
    # Print the matched reply line for the agent.
    tail -c +"$((start_bytes + 1))" "$RA_LOG" | rg -o -e "${ack}[^\r\n]*" | tail -n1 || true
  fi
}

current_status_frame() {
  local start_bytes status_line
  start_bytes="$(log_size_bytes)"
  send_fifo "GET_STATUS"
  if ! wait_for_log_pattern_after "$start_bytes" "GET_STATUS " 5; then
    return 1
  fi
  status_line="$(tail -c +"$((start_bytes + 1))" "$RA_LOG" | rg -o "GET_STATUS [^\r\n]*" | tail -n1)"
  if [[ "$status_line" =~ frame=([0-9]+)$ ]]; then
    printf '%s\n' "${BASH_REMATCH[1]}"
    return 0
  fi
  return 1
}

wait_for_paused_frame() {
  local min_frame="$1" timeout_seconds="$2"
  local deadline=$(( $(date +%s) + timeout_seconds ))
  while (( $(date +%s) < deadline )); do
    local start_bytes status_line
    start_bytes="$(log_size_bytes)"
    send_fifo "GET_STATUS"
    if wait_for_log_pattern_after "$start_bytes" "GET_STATUS " 2; then
      status_line="$(tail -c +"$((start_bytes + 1))" "$RA_LOG" | rg -o "GET_STATUS [^\r\n]*" | tail -n1)"
      if [[ "$status_line" =~ ^GET_STATUS[[:space:]]+PAUSED.*,frame=([0-9]+)$ ]] && (( BASH_REMATCH[1] >= min_frame )); then
        return 0
      fi
    fi
    sleep 0.2
  done
  return 1
}

cmd_input() {
  local PORT=0 MASK="" HOLD_SECONDS="" FRAMES="" ANALOG=""
  while (($#)); do
    case "$1" in
      --bundle-dir) shift; BUNDLE_DIR="${1:-}" ;;
      --port) shift; PORT="${1:-}" ;;
      --mask) shift; MASK="${1:-}" ;;
      --hold-seconds) shift; HOLD_SECONDS="${1:-}" ;;
      --frames) shift; FRAMES="${1:-}" ;;
      --analog) shift; ANALOG="${1:-}" ;;
      *) echo "Unknown input option: $1" >&2; exit 2 ;;
    esac
    shift
  done
  if [[ -z "${BUNDLE_DIR:-}" || -z "$MASK" ]]; then
    echo "input requires --bundle-dir and --mask." >&2
    exit 2
  fi
  if [[ -n "$HOLD_SECONDS" && -n "$FRAMES" ]]; then
    echo "input takes --hold-seconds or --frames, not both." >&2
    exit 2
  fi
  [[ -z "$HOLD_SECONDS" && -z "$FRAMES" ]] && HOLD_SECONDS=0.5
  require_live_session

  local set_cmd="SET_INPUT_PORT $PORT $MASK"
  [[ -n "$ANALOG" ]] && set_cmd="$set_cmd $ANALOG"
  local start_bytes
  start_bytes="$(log_size_bytes)"
  send_fifo "$set_cmd"
  if ! wait_for_log_pattern_after "$start_bytes" "SET_INPUT_PORT " 5; then
    echo "SET_INPUT_PORT not acknowledged." >&2
    exit 1
  fi

  if [[ -n "$FRAMES" ]]; then
    # TAS-style: hold the input for exactly FRAMES stepped frames. The
    # session must be paused; STEP_FRAME leaves it paused again.
    local base_frame
    if ! base_frame="$(current_status_frame)"; then
      echo "Could not read current frame for --frames input." >&2
      exit 1
    fi
    send_fifo "STEP_FRAME $FRAMES"
    if ! wait_for_paused_frame "$(( base_frame + FRAMES ))" 60; then
      echo "STEP_FRAME $FRAMES did not complete (is the session paused?)." >&2
      exit 1
    fi
  else
    sleep "$HOLD_SECONDS"
  fi

  start_bytes="$(log_size_bytes)"
  send_fifo "CLEAR_INPUT_PORT $PORT"
  if ! wait_for_log_pattern_after "$start_bytes" "CLEAR_INPUT_PORT " 5; then
    echo "CLEAR_INPUT_PORT not acknowledged." >&2
    exit 1
  fi
  if [[ -n "$FRAMES" ]]; then
    echo "[interactive] held mask=$MASK on port $PORT for $FRAMES stepped frames"
  else
    echo "[interactive] held mask=$MASK on port $PORT for ${HOLD_SECONDS}s"
  fi
}

cmd_screenshot() {
  while (($#)); do
    case "$1" in
      --bundle-dir) shift; BUNDLE_DIR="${1:-}" ;;
      *) echo "Unknown screenshot option: $1" >&2; exit 2 ;;
    esac
    shift
  done
  [[ -z "${BUNDLE_DIR:-}" ]] && { echo "screenshot requires --bundle-dir." >&2; exit 2; }
  require_live_session

  local before
  before="$(find "$BUNDLE_DIR/captures" -maxdepth 1 -type f | wc -l)"
  send_fifo "SCREENSHOT"
  local deadline=$(( $(date +%s) + 15 ))
  local newest="" last_size=-1
  while (( $(date +%s) < deadline )); do
    local now
    now="$(find "$BUNDLE_DIR/captures" -maxdepth 1 -type f | wc -l)"
    if (( now > before )); then
      newest="$(newest_capture_path "$BUNDLE_DIR/captures")"
      # The screenshot task writes asynchronously: wait until the file is
      # non-empty and its size is stable across two polls.
      local size
      size="$(file_size_bytes "$newest")"
      if (( size > 0 && size == last_size )); then
        warn_if_black_capture "$newest"
        printf '%s\n' "$newest"
        return 0
      fi
      last_size="$size"
    fi
    sleep 0.2
  done
  echo "No completed capture appeared within 15s." >&2
  exit 1
}

# Uniformly-black captures are almost always tooling faults, not scenes:
# a screenshot before the first presented frame after
# LOAD_STATE_SLOT_PAUSED, or GPU-backbuffer screenshots while
# presentation is suspended. Warn (a black scene is legitimate during
# fades, so this must not fail) and leave a record in the bundle.
warn_if_black_capture() {
  local capture="$1" rc=0
  python3 "$SCRIPT_DIR/png_uniform_black.py" "$capture" 2>/dev/null || rc=$?
  if (( rc == 0 )); then
    echo "[interactive] warning: capture is uniformly black: $capture" >&2
    echo "  (likely no frame presented yet — step >=1 frame after a paused state load," >&2
    echo "   or set video_gpu_screenshot=false if the display/presentation is suspended)" >&2
    printf '%s black capture: %s\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)" "$capture" \
      >> "$BUNDLE_DIR/logs/capture.warnings.log"
  fi
}

cmd_status() {
  while (($#)); do
    case "$1" in
      --bundle-dir) shift; BUNDLE_DIR="${1:-}" ;;
      *) echo "Unknown status option: $1" >&2; exit 2 ;;
    esac
    shift
  done
  [[ -z "${BUNDLE_DIR:-}" ]] && { echo "status requires --bundle-dir." >&2; exit 2; }
  require_live_session

  local start_bytes
  start_bytes="$(log_size_bytes)"
  send_fifo "GET_STATUS"
  if ! wait_for_log_pattern_after "$start_bytes" "GET_STATUS " 5; then
    echo "GET_STATUS not acknowledged." >&2
    exit 1
  fi
  tail -c +"$((start_bytes + 1))" "$RA_LOG" | rg -o "GET_STATUS [^\r\n]*" | tail -n1
}

cmd_save_slot() {
  local SLOT=""
  while (($#)); do
    case "$1" in
      --bundle-dir) shift; BUNDLE_DIR="${1:-}" ;;
      --slot) shift; SLOT="${1:-}" ;;
      *) echo "Unknown save-slot option: $1" >&2; exit 2 ;;
    esac
    shift
  done
  if [[ -z "${BUNDLE_DIR:-}" || ! "$SLOT" =~ ^[1-9]$ ]]; then
    echo "save-slot requires --bundle-dir and --slot 1..9." >&2
    exit 2
  fi
  require_live_session

  # STATE_SLOT_PLUS/MINUS are silent keybind events: track the slot locally
  # and verify the SAVE_STATE log names the expected .state<N> file.
  local current
  current="$(cat "$SLOT_FILE" 2>/dev/null || echo 0)"
  local delta=$(( SLOT - current ))
  local verb="STATE_SLOT_PLUS"
  (( delta < 0 )) && verb="STATE_SLOT_MINUS"
  local i
  for (( i = 0; i < ${delta#-}; i++ )); do
    send_fifo "$verb"
    sleep 0.1
  done

  local start_bytes
  start_bytes="$(log_size_bytes)"
  send_fifo "SAVE_STATE"
  if ! wait_for_log_pattern_after "$start_bytes" "[State] Saving state" 15; then
    echo "SAVE_STATE not acknowledged." >&2
    exit 1
  fi
  send_fifo "WAIT_SAVE_STATE"
  if ! wait_for_log_pattern_after "$start_bytes" "WAIT_SAVE_STATE DONE" 30; then
    echo "WAIT_SAVE_STATE not acknowledged." >&2
    exit 1
  fi
  if ! tail -c +"$((start_bytes + 1))" "$RA_LOG" | rg -q "Saving state.*\.state${SLOT}[\".]"; then
    echo "Save landed in an unexpected slot; check $RA_LOG (slot tracking desynced?)." >&2
    exit 1
  fi
  echo "$SLOT" > "$SLOT_FILE"
  echo "[interactive] saved checkpoint slot $SLOT"
}

cmd_load_slot() {
  local SLOT="" PAUSED=0
  while (($#)); do
    case "$1" in
      --bundle-dir) shift; BUNDLE_DIR="${1:-}" ;;
      --slot) shift; SLOT="${1:-}" ;;
      --paused) PAUSED=1 ;;
      *) echo "Unknown load-slot option: $1" >&2; exit 2 ;;
    esac
    shift
  done
  if [[ -z "${BUNDLE_DIR:-}" || ! "$SLOT" =~ ^[0-9]$ ]]; then
    echo "load-slot requires --bundle-dir and --slot 0..9." >&2
    exit 2
  fi
  require_live_session

  local verb="LOAD_STATE_SLOT"
  (( PAUSED )) && verb="LOAD_STATE_SLOT_PAUSED"
  # State loads can transiently fail while another frontend task (e.g. a
  # screenshot) is still in flight; retry once before failing loudly.
  local attempt start_bytes
  for attempt in 1 2; do
    start_bytes="$(log_size_bytes)"
    send_fifo "$verb $SLOT"
    if wait_for_log_pattern_after "$start_bytes" "[State] Loading state" 15; then
      sleep 0.5
      if tail -c +"$((start_bytes + 1))" "$RA_LOG" | rg -q "\\[State\\] Failed to load state"; then
        (( attempt == 1 )) && sleep 1
        continue
      fi
      echo "[interactive] loaded slot $SLOT"
      return 0
    fi
    (( attempt == 1 )) && sleep 1
  done
  echo "$verb not acknowledged after retry." >&2
  exit 1
}

cmd_stop() {
  while (($#)); do
    case "$1" in
      --bundle-dir) shift; BUNDLE_DIR="${1:-}" ;;
      *) echo "Unknown stop option: $1" >&2; exit 2 ;;
    esac
    shift
  done
  [[ -z "${BUNDLE_DIR:-}" ]] && { echo "stop requires --bundle-dir." >&2; exit 2; }
  session_paths
  if [[ ! -f "$PID_FILE" ]]; then
    echo "No session.pid; nothing to stop." >&2
    exit 0
  fi
  local PGID
  PGID="$(cat "$PID_FILE")"

  if kill -0 -- "-$PGID" 2>/dev/null && [[ -p "$FIFO_PATH" ]]; then
    record_end_reason "stop: agent-requested stop (QUIT)"
    send_fifo "QUIT" || true
  fi
  local deadline=$(( $(date +%s) + 10 ))
  while (( $(date +%s) < deadline )); do
    if ! kill -0 -- "-$PGID" 2>/dev/null; then
      rm -f "$FIFO_PATH"
      echo "[interactive] session exited cleanly."
      return 0
    fi
    sleep 0.5
  done
  echo "[interactive] QUIT timed out; killing process group $PGID."
  record_end_reason "stop: QUIT timed out; killed process group $PGID"
  kill -TERM -- "-$PGID" 2>/dev/null || true
  sleep 2
  if kill -0 -- "-$PGID" 2>/dev/null; then
    kill -KILL -- "-$PGID" 2>/dev/null || true
  fi
  rm -f "$FIFO_PATH"
  echo "[interactive] session terminated."
}

session_chain_pids() {
  # Every process in an adapter-launched session chain carries one of these
  # on its command line: the runtime lock path (the flock leader) or the
  # "-L <core>" argument (timeout + frontend). The TTL watchdog does NOT
  # match (it carries "--core", not "-L ") — it self-exits once its session
  # dies and must not be group-killed: it may share the caller's group.
  {
    pgrep -f 'flock .*parallel-n64-retroarch-runtime\.lock' || true
    pgrep -f -- '-L .*parallel_n64_libretro' || true
  } | sort -un
}

cmd_doctor() {
  # Find and (optionally) reap leftover emulator session trees. Extra
  # RetroArch processes are a recurring failure mode (wedged sessions,
  # dead drivers, TTL not yet fired) that breaks the one-session-per-host
  # rule; run this BEFORE starting a session — during a live session it
  # will (correctly) report that session too. Unmanaged RetroArch
  # processes (e.g. the GUI app without our core) are reported, never
  # killed. Exit 0 = host clean (after reaping if requested), 1 = sessions
  # present (or reap incomplete).
  local reap=0
  while (($#)); do
    case "$1" in
      --reap) reap=1 ;;
      *) echo "Unknown doctor option: $1" >&2; exit 2 ;;
    esac
    shift
  done

  local own_pgid pid pgid
  own_pgid="$(ps -o pgid= -p $$ | tr -d ' ')"

  local found=0 pgids=""
  for pid in $(session_chain_pids); do
    pgid="$(ps -o pgid= -p "$pid" 2>/dev/null | tr -d ' ')"
    [[ -z "$pgid" || "$pgid" == "$own_pgid" ]] && continue
    found=1
    echo "SESSION pid=$pid pgid=$pgid cmd=$(ps -o args= -p "$pid" 2>/dev/null | cut -c1-160)"
    case " $pgids " in *" $pgid "*) ;; *) pgids="$pgids $pgid" ;; esac
  done

  local session_pids
  session_pids=" $(session_chain_pids | tr '\n' ' ') "
  for pid in $(pgrep -x RetroArch 2>/dev/null || true); do
    [[ "$session_pids" == *" $pid "* ]] && continue
    echo "UNMANAGED pid=$pid cmd=$(ps -o args= -p "$pid" 2>/dev/null | cut -c1-160)"
  done

  if (( ! found )); then
    echo "DOCTOR clean"
    return 0
  fi
  if (( ! reap )); then
    echo "DOCTOR sessions-found (use --reap to kill)"
    return 1
  fi
  for pgid in $pgids; do
    echo "REAP pgid=$pgid (TERM)"
    kill -TERM -- "-$pgid" 2>/dev/null || true
  done
  sleep 3
  for pgid in $pgids; do
    if kill -0 -- "-$pgid" 2>/dev/null; then
      echo "REAP pgid=$pgid (KILL)"
      kill -KILL -- "-$pgid" 2>/dev/null || true
    fi
  done
  sleep 1
  local remaining=0
  for pid in $(session_chain_pids); do
    pgid="$(ps -o pgid= -p "$pid" 2>/dev/null | tr -d ' ')"
    [[ -z "$pgid" || "$pgid" == "$own_pgid" ]] && continue
    remaining=1
    echo "REMAINS pid=$pid pgid=$pgid"
  done
  if (( remaining )); then
    echo "DOCTOR reap-incomplete"
    return 1
  fi
  echo "DOCTOR clean (reaped)"
  return 0
}

SUBCOMMAND="${1:-}"
shift || true
case "$SUBCOMMAND" in
  start) cmd_start "$@" ;;
  send) cmd_send "$@" ;;
  input) cmd_input "$@" ;;
  screenshot) cmd_screenshot "$@" ;;
  status) cmd_status "$@" ;;
  save-slot) cmd_save_slot "$@" ;;
  load-slot) cmd_load_slot "$@" ;;
  stop) cmd_stop "$@" ;;
  doctor) cmd_doctor "$@" ;;
  -h|--help|"") usage; [[ "$SUBCOMMAND" == "" ]] && exit 2 || exit 0 ;;
  *) echo "Unknown subcommand: $SUBCOMMAND" >&2; usage >&2; exit 2 ;;
esac
