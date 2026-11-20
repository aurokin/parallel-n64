#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd -- "$SCRIPT_DIR/.." && pwd)"
SHADER_DIR="$REPO_DIR/mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp/shaders"
SHADER_HEADER="$SHADER_DIR/slangmosh.hpp"
# The vendored renderer is paraLLEl-RDP 1cecd042 (mupen64plus-video-paraLLEl/parallel-rdp/COMMIT).
# slangmosh must be built from the matching Granite toolchain; other revisions emit an
# incompatible header or different SPIR-V. See tools/parallel-rdp-toolchain-patches/README.md.

usage() {
  cat <<'EOF'
Usage:
  regen-parallel-rdp-shaders.sh [--force]

Environment:
  SLANGMOSH=/path/to/slangmosh   Override the shader packer used for regeneration.

Notes:
  - Regenerates `parallel-rdp/shaders/slangmosh.hpp` when shader inputs are newer.
  - Uses `slangmosh` from `PATH` when SLANGMOSH is unset.
EOF
}

log() {
  printf '[slangmosh] %s\n' "$*"
}

find_slangmosh() {
  if [[ -n "${SLANGMOSH:-}" ]]; then
    if [[ ! -x "$SLANGMOSH" ]]; then
      echo "SLANGMOSH is set but not executable: $SLANGMOSH" >&2
      exit 1
    fi
    printf '%s\n' "$SLANGMOSH"
    return 0
  fi

  if command -v slangmosh >/dev/null 2>&1; then
    command -v slangmosh
    return 0
  fi

  return 1
}

find_stale_input() {
  if [[ ! -f "$SHADER_HEADER" ]]; then
    printf '%s\n' "$SHADER_HEADER"
    return 0
  fi

  find "$SHADER_DIR" -maxdepth 1 -type f ! -name "$(basename "$SHADER_HEADER")" -newer "$SHADER_HEADER" -print | sort | head -n 1
}

FORCE_REGEN=0
while (($#)); do
  case "$1" in
    --force)
      FORCE_REGEN=1
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "Unknown argument: $1" >&2
      usage >&2
      exit 2
      ;;
  esac
  shift
done

STALE_INPUT=""
if (( FORCE_REGEN )); then
  STALE_INPUT="forced"
else
  STALE_INPUT="$(find_stale_input || true)"
fi

if [[ -z "$STALE_INPUT" ]]; then
  log "up to date: $SHADER_HEADER"
  exit 0
fi

if ! SLANGMOSH_BIN="$(find_slangmosh)"; then
  echo "Cannot regenerate $SHADER_HEADER: slangmosh not found." >&2
  echo "Set SLANGMOSH=/path/to/slangmosh; see tools/parallel-rdp-toolchain-patches/README.md." >&2
  echo "Stale input: $STALE_INPUT" >&2
  exit 1
fi

log "regenerating $SHADER_HEADER"
if [[ "$STALE_INPUT" != "forced" ]]; then
  log "stale input: $STALE_INPUT"
fi
log "tool: $SLANGMOSH_BIN"

# slangmosh.json declares the shared Granite include dir ("debug_channel.h" etc.)
# as ../../Granite/assets/shaders/inc, which only resolves inside the upstream
# parallel-rdp repo where Granite is a submodule. This fork does not vendor
# Granite, so rewrite the include path to a real Granite checkout at run time.
GRANITE_ASSETS="${GRANITE_DEFAULT_ASSET_DIRECTORY:-}"
if [[ -z "$GRANITE_ASSETS" || ! -d "$GRANITE_ASSETS/shaders/inc" ]]; then
  echo "Granite shader include dir not found: ${GRANITE_ASSETS:-<unset>}/shaders/inc" >&2
  echo "Set GRANITE_DEFAULT_ASSET_DIRECTORY to the assets/ of the Granite checkout that built slangmosh." >&2
  exit 1
fi

# The flags match upstream's own regeneration recipe (generate_standalone_codebase.sh
# at 1cecd042). This slangmosh always targets Vulkan 1.1 (SPIR-V 1.3) and has no --vk11.
(
  cd "$SHADER_DIR"
  trap 'rm -f slangmosh.regen.json' EXIT
  sed "s|\"../../Granite/assets/shaders/inc\"|\"$GRANITE_ASSETS/shaders/inc\"|" \
    slangmosh.json > slangmosh.regen.json
  "$SLANGMOSH_BIN" --strip -O --namespace RDP slangmosh.regen.json --output slangmosh.hpp
)
