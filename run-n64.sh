#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
DEFAULT_RETROARCH="/home/auro/code/mupen/RetroArch-upstream/retroarch"
DEFAULT_ROM_DIR="/home/auro/code/n64_roms"
DEFAULT_ROM_NAME="Paper Mario (USA).zip"
REFERENCE_CORE="$SCRIPT_DIR/builds/parallel_n64_libretro.reference.so"
DEFAULT_CORE_OPTIONS_FILE="$HOME/.config/retroarch/config/ParaLLEl N64/ParaLLEl N64.opt"

use_reference=0
menu_mode=0
retroarch_bin="${RETROARCH_BIN:-$DEFAULT_RETROARCH}"
rom_dir="${ROM_DIR:-$DEFAULT_ROM_DIR}"
explicit_core=""
rom_path=""
force_fullscreen="${RUN_N64_FULLSCREEN:-1}"
core_options_file="${RUN_N64_CORE_OPTIONS_FILE:-$DEFAULT_CORE_OPTIONS_FILE}"
declare -a passthrough_args=()

start_maximize_helper() {
  local maximize="${RUN_N64_MAXIMIZE:-1}"
  if [[ "$maximize" == "0" ]]; then
    return
  fi

  if ! command -v xdotool >/dev/null 2>&1; then
    return
  fi

  local target_pid="$$"
  (
    local tries=60
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

usage() {
  cat <<'EOF_HELP'
Usage:
  run-n64.sh [options] [ROM_PATH] [-- RETROARCH_ARGS...]

Options:
  --reference         Use reference core build (builds/parallel_n64_libretro.reference.so)
  --core PATH         Use an explicit core path
  --retroarch PATH    Use an explicit RetroArch binary path
  --rom-dir PATH      ROM base directory for relative ROM paths (default: /home/auro/code/n64_roms)
  --menu              Launch RetroArch menu without content
  --no-fullscreen     Do not force RetroArch fullscreen (`-f`)
  --list-cores        Print discovered non-reference core builds
  -h, --help          Show this help

Behavior:
  - Default core: newest non-reference parallel_n64_libretro*.so under this repo/builds.
  - If ROM_PATH is omitted, defaults to "Paper Mario (USA).zip" in ROM dir.
  - Launches RetroArch fullscreen by default for consistent screenshot geometry.
    Set RUN_N64_FULLSCREEN=0 or pass --no-fullscreen to disable.
  - When not fullscreen, attempts to maximize via `xdotool` when available.
    Set RUN_N64_MAXIMIZE=0 to disable maximize helper.
  - Enforces core options each launch: `parallel-n64-gfxplugin=parallel`
    and `parallel-n64-parallel-rdp-upscaling=4x`.
EOF_HELP
}

list_non_reference_cores() {
  local -a roots=("$SCRIPT_DIR")
  if [[ -d "$SCRIPT_DIR/builds" ]]; then
    roots+=("$SCRIPT_DIR/builds")
  fi

  find "${roots[@]}" -maxdepth 2 -type f -name 'parallel_n64_libretro*.so' \
    ! -name '*reference*' -printf '%T@ %p\n' 2>/dev/null | sort -nr | awk '{ $1=""; sub(/^ /, ""); print }'
}

pick_latest_core() {
  local selected
  selected="$(list_non_reference_cores | head -n 1 || true)"
  if [[ -z "$selected" ]]; then
    return 1
  fi
  printf '%s\n' "$selected"
}

resolve_rom_path() {
  local input="$1"
  local base_dir="$2"

  if [[ -f "$input" ]]; then
    printf '%s\n' "$input"
    return 0
  fi

  if [[ -f "$base_dir/$input" ]]; then
    printf '%s\n' "$base_dir/$input"
    return 0
  fi

  return 1
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

enforce_parallel_defaults() {
  ensure_core_option "$core_options_file" "parallel-n64-gfxplugin" "parallel"
  ensure_core_option "$core_options_file" "parallel-n64-parallel-rdp-upscaling" "4x"
}

while (($#)); do
  case "$1" in
    --reference)
      use_reference=1
      ;;
    --core)
      shift
      explicit_core="${1:-}"
      ;;
    --retroarch)
      shift
      retroarch_bin="${1:-}"
      ;;
    --rom-dir)
      shift
      rom_dir="${1:-}"
      ;;
    --menu)
      menu_mode=1
      ;;
    --no-fullscreen)
      force_fullscreen=0
      ;;
    --list-cores)
      list_non_reference_cores
      exit 0
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    --)
      shift
      passthrough_args+=("$@")
      break
      ;;
    -*)
      echo "Unknown option: $1" >&2
      usage >&2
      exit 2
      ;;
    *)
      if [[ -z "$rom_path" ]]; then
        rom_path="$1"
      else
        passthrough_args+=("$1")
      fi
      ;;
  esac
  shift
done

if [[ ! -x "$retroarch_bin" ]]; then
  echo "RetroArch binary not executable: $retroarch_bin" >&2
  exit 1
fi

if [[ -n "$explicit_core" ]]; then
  core_path="$explicit_core"
elif (( use_reference )); then
  core_path="$REFERENCE_CORE"
else
  if ! core_path="$(pick_latest_core)"; then
    echo "No non-reference parallel core builds found." >&2
    exit 1
  fi
fi

if [[ ! -f "$core_path" ]]; then
  echo "Core file not found: $core_path" >&2
  exit 1
fi

enforce_parallel_defaults

if [[ -z "$rom_path" && "$menu_mode" -eq 0 ]]; then
  rom_path="$DEFAULT_ROM_NAME"
fi

declare -a cmd
cmd=("$retroarch_bin" -L "$core_path")

if [[ "$force_fullscreen" != "0" ]]; then
  cmd+=(-f)
fi

if (( menu_mode )); then
  cmd+=(--menu)
fi

if [[ -n "$rom_path" ]]; then
  if ! resolved_rom="$(resolve_rom_path "$rom_path" "$rom_dir")"; then
    echo "ROM not found: $rom_path" >&2
    exit 1
  fi
  cmd+=("$resolved_rom")
fi

cmd+=("${passthrough_args[@]}")

echo "Using core: $core_path"
echo "Enforced core options: parallel-n64-gfxplugin=parallel, parallel-n64-parallel-rdp-upscaling=4x"
if [[ "$force_fullscreen" == "0" ]]; then
  start_maximize_helper
fi


# RetroArch can rewrite core options on shutdown. Start a detached watcher
# that re-applies pinned defaults after this process (replaced via exec) exits.
(
  target_pid=$$
  while kill -0 "$target_pid" 2>/dev/null; do
    sleep 0.25
  done
  ensure_core_option "$core_options_file" "parallel-n64-gfxplugin" "parallel"
  ensure_core_option "$core_options_file" "parallel-n64-parallel-rdp-upscaling" "4x"
) >/dev/null 2>&1 &

exec "${cmd[@]}"
