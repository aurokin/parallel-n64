#!/usr/bin/env bash
# Capture a GlideN64 (mupen64plus-next) reference screenshot of a scene with
# the legacy .hts hi-res pack loaded.
#
# Purpose: side-by-side CONTENT review against paraLLEl hi-res captures —
# "does the pack content appear, on the right surfaces, at hi-res detail?"
# The output is for human/agent visual review only. It must never feed a
# numeric image-similarity metric or digest gate (see
# tests/emulator_behavior/support/emu_gliden64_reference_guardrails.sh).
#
# GlideN64 reaches scenes by boot + timed input, not by savestate: paraLLEl
# savestates do not load into mupen64plus-next, so scene framing is
# approximate by design.
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "$SCRIPT_DIR/../.." && pwd)"

usage() {
  cat <<'EOF'
Usage:
  gliden64-reference-capture.sh --bundle-dir PATH [options]

Required:
  --bundle-dir PATH   Output bundle directory

Options:
  --scene NAME        Scene recipe: title (default: title)
  --rom PATH          ROM (default: assets/Paper Mario (USA).zip)
  --core PATH         mupen64plus-next core
                      (default: /home/auro/code/cores/mupen64plus_next_libretro.so)
  --pack PATH         Legacy .hts pack
                      (default: assets/PAPER MARIO_HIRESTEXTURES.hts)
  --hires on|off      Enable hi-res textures (default: on)
  -h, --help          Show this help
EOF
}

BUNDLE_DIR=""
SCENE="title"
ROM_PATH="$REPO_ROOT/assets/Paper Mario (USA).zip"
CORE_PATH="/home/auro/code/cores/mupen64plus_next_libretro.so"
PACK_PATH="$REPO_ROOT/assets/PAPER MARIO_HIRESTEXTURES.hts"
HIRES="on"

while (($#)); do
  case "$1" in
    --bundle-dir) shift; BUNDLE_DIR="${1:-}" ;;
    --scene) shift; SCENE="${1:-}" ;;
    --rom) shift; ROM_PATH="${1:-}" ;;
    --core) shift; CORE_PATH="${1:-}" ;;
    --pack) shift; PACK_PATH="${1:-}" ;;
    --hires) shift; HIRES="${1:-}" ;;
    -h|--help) usage; exit 0 ;;
    *) echo "Unknown option: $1" >&2; usage >&2; exit 2 ;;
  esac
  shift
done

if [[ -z "$BUNDLE_DIR" ]]; then
  echo "--bundle-dir is required." >&2
  exit 2
fi
if [[ ! -f "$PACK_PATH" ]]; then
  echo "Pack not found: $PACK_PATH" >&2
  exit 1
fi
if [[ "$HIRES" != "on" && "$HIRES" != "off" ]]; then
  echo "--hires must be on or off." >&2
  exit 2
fi

mkdir -p "$BUNDLE_DIR"

# GlideN64's EnhancedHighResStorage reads <system>/Mupen64plus/cache/<NAME>_HIRESTEXTURES.hts.
# Stage a per-bundle system dir (symlink: packs are hundreds of MB, read-only use).
SYSTEM_DIR="$BUNDLE_DIR/system"
mkdir -p "$SYSTEM_DIR/Mupen64plus/cache"
ln -sf "$(readlink -f "$PACK_PATH")" "$SYSTEM_DIR/Mupen64plus/cache/$(basename "$PACK_PATH")"

TX_ENABLE="True"
if [[ "$HIRES" == "off" ]]; then
  TX_ENABLE="False"
fi

CORE_OPTIONS_TEMPLATE="$BUNDLE_DIR/core-options.template.opt"
cat > "$CORE_OPTIONS_TEMPLATE" <<EOF
mupen64plus-rdp-plugin = "gliden64"
mupen64plus-rsp-plugin = "hle"
mupen64plus-43screensize = "1280x960"
mupen64plus-txHiresEnable = "$TX_ENABLE"
mupen64plus-EnableEnhancedHighResStorage = "$TX_ENABLE"
mupen64plus-txCacheCompression = "True"
mupen64plus-txHiresFullAlphaChannel = "True"
EOF

# GLideN64 is an OpenGL renderer; override the adapter's vulkan default and
# point the session at the staged per-bundle system dir.
EXTRA_APPEND="$BUNDLE_DIR/retroarch.extra.cfg"
cat > "$EXTRA_APPEND" <<EOF
video_driver = "gl"
system_directory = "$SYSTEM_DIR"
EOF

declare -a SCENE_COMMANDS=()
case "$SCENE" in
  title)
    # Same recipe the paraLLEl title authority was minted from: boot, settle,
    # press Start, settle on the title screen.
    SCENE_COMMANDS=(
      "WAIT_COMMAND_READY 120"
      "WAIT 20"
      "SET_INPUT_PORT 0 0x8"
      "WAIT 0.2"
      "CLEAR_INPUT_PORT 0"
      "WAIT 5"
      "SCREENSHOT"
      "WAIT_NEW_CAPTURE 10"
      "QUIT"
    )
    ;;
  *)
    echo "Unknown scene: $SCENE (supported: title)" >&2
    exit 2
    ;;
esac

declare -a CMD_ARGS=()
for cmd in "${SCENE_COMMANDS[@]}"; do
  CMD_ARGS+=(--command "$cmd")
done

# GlideN64 gates txDump behind this env var (nx commit 8865474). The dump
# set IS the miss-set oracle (ADR-0012); without the export it is silently
# empty and a "dump set = miss set" proof would pass vacuously.
export GLN64_TXDUMP=1

exec "$REPO_ROOT/tools/adapters/retroarch_stdin_session.sh" \
  --bundle-dir "$BUNDLE_DIR" \
  --rom "$ROM_PATH" \
  --core "$CORE_PATH" \
  --mode "$HIRES" \
  --core-options-template "$CORE_OPTIONS_TEMPLATE" \
  --extra-append-config "$EXTRA_APPEND" \
  "${CMD_ARGS[@]}"
