#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "$SCRIPT_DIR/../../.." && pwd)"
RUNNER="$REPO_ROOT/run-paper-mario-gliden64-capture.sh"

if [[ ! -f "$RUNNER" ]]; then
  echo "FAIL: missing run-paper-mario-gliden64-capture.sh at $RUNNER" >&2
  exit 1
fi

require_pattern() {
  local pattern="$1"
  local message="$2"
  if ! rg -n --fixed-strings -- "$pattern" "$RUNNER" >/dev/null; then
    echo "FAIL: $message" >&2
    exit 1
  fi
}

require_pattern "run-paper-mario-gliden64-capture.sh [options]" \
  "usage text missing gliden64 capture invocation"
require_pattern 'DEFAULT_CORE="/tmp/mupen64plus-libretro-nx-audit/mupen64plus_next_libretro.so"' \
  "default GLide core path missing"
require_pattern 'mupen64plus-rdp-plugin = "gliden64"' "gliden64 plugin option missing"
require_pattern 'mupen64plus-EnableNativeResFactor = "4"' "native res factor option missing"
require_pattern 'mupen64plus-EnableTextureCache = "True"' "texture cache option missing"
require_pattern 'mupen64plus-EnableEnhancedTextureStorage = "True"' "enhanced texture storage option missing"
require_pattern 'mupen64plus-txHiresEnable = "True"' "hires enable option missing"
require_pattern 'mupen64plus-txHiresFullAlphaChannel = "True"' "full alpha option missing"
require_pattern 'mupen64plus-EnableHiResAltCRC = "True"' "alt CRC option missing"
require_pattern 'mupen64plus-EnableEnhancedHighResStorage = "True"' "enhanced hi-res storage option missing"
require_pattern 'mupen64plus-txCacheCompression = "False"' "cache compression option missing"
require_pattern 'python3 "$VPAD_TOOL" daemon --socket "$vpad_socket"' "virtual gamepad daemon contract missing"
require_pattern 'input_player1_reserved_device = "parallel-n64 Virtual Pad"' "reserved device override missing"
require_pattern 'pause_nonactive = "false"' "inactive pause override missing"
require_pattern 'global_core_options = "true"' "per-core options mode override missing"
require_pattern 'core_options_path = "$xdg_root/retroarch/config"' "isolated core options path override missing"
require_pattern 'DEFAULT_HOME_CORE_OPTIONS="$HOME/.config/retroarch/config/Mupen64Plus-Next/Mupen64Plus-Next.opt"' \
  "home core options path missing"
require_pattern 'cp "$core_options_file" "$home_core_options_file"' \
  "home core options staging missing"
require_pattern 'restore_home_core_options' "home core options cleanup missing"
require_pattern 'gliden64_cache_dir="$system_dir/Mupen64plus/cache"' \
  "GLide hi-res cache directory staging missing"
require_pattern 'ln -s "$(readlink -f "$gliden64_pack_source")" "$gliden64_cache_path"' \
  "GLide hi-res cache symlink staging missing"
require_pattern 'restore_gliden64_hires_cache' "GLide hi-res cache cleanup missing"
require_pattern 'send_netcmd "SCREENSHOT"' "RetroArch screenshot command missing"
require_pattern 'export XDG_CONFIG_HOME="$xdg_root"' "isolated XDG config export missing"
require_pattern 'export LIBRETRO_SYSTEM_DIRECTORY="$system_dir"' "system dir export missing"

echo "emu_run_paper_mario_gliden64_capture_contract: PASS"
