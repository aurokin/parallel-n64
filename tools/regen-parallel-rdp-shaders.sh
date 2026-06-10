#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd -- "$SCRIPT_DIR/.." && pwd)"
SHADER_DIR="$REPO_DIR/mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp/shaders"
SHADER_HEADER="$SHADER_DIR/slangmosh.hpp"
# The fork vendors parallel-rdp at the commit in mupen64plus-video-paraLLEl/parallel-rdp/COMMIT
# (7a3e561e, 2020). slangmosh must be built from THAT vintage: modern Granite emits an
# incompatible header (interface split, reflection-bank constructor). Build recipe:
#   git -C ~/code/mupen/parallel-rdp-upstream checkout 7a3e561e89d35f7e221770d21e7efa970b496a1e
#   git -C ~/code/mupen/parallel-rdp-upstream submodule update --init --recursive  # astc-encoder pin is dead; skip it
#   apply tools/parallel-rdp-toolchain-patches/ to the Granite submodule
#   cmake -S ~/code/mupen/parallel-rdp-upstream -B ~/code/mupen/parallel-rdp-upstream/build-2020 \
#     -DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DGRANITE_ASTC_ENCODER_COMPRESSION=OFF
#   cmake --build ~/code/mupen/parallel-rdp-upstream/build-2020 --target slangmosh -j"$(nproc)"
DEFAULT_SLANGMOSH="$HOME/code/mupen/parallel-rdp-upstream/build-2020/Granite/tools/slangmosh"

usage() {
  cat <<'EOF'
Usage:
  regen-parallel-rdp-shaders.sh [--force]

Environment:
  SLANGMOSH=/path/to/slangmosh   Override the shader packer used for regeneration.

Notes:
  - Regenerates `parallel-rdp/shaders/slangmosh.hpp` when shader inputs are newer.
  - Falls back to the local upstream build at `~/code/mupen/.../slangmosh` if `slangmosh`
    is not in `PATH`.
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

  if [[ -x "$DEFAULT_SLANGMOSH" ]]; then
    printf '%s\n' "$DEFAULT_SLANGMOSH"
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
  echo "Set SLANGMOSH=/path/to/slangmosh or build the local upstream tool first." >&2
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
GRANITE_ASSETS="${GRANITE_DEFAULT_ASSET_DIRECTORY:-$HOME/code/mupen/parallel-rdp-upstream/Granite/assets}"
if [[ ! -d "$GRANITE_ASSETS/shaders/inc" ]]; then
  echo "Granite shader include dir not found: $GRANITE_ASSETS/shaders/inc" >&2
  echo "Set GRANITE_DEFAULT_ASSET_DIRECTORY to <parallel-rdp-upstream>/Granite/assets." >&2
  exit 1
fi

# --vk11 (subgroup ops need SPIR-V 1.3) and --namespace RDP match upstream's own
# regeneration recipe (parallel-rdp README / generate_standalone_codebase.sh).
(
  cd "$SHADER_DIR"
  trap 'rm -f slangmosh.regen.json' EXIT
  sed "s|\"../../Granite/assets/shaders/inc\"|\"$GRANITE_ASSETS/shaders/inc\"|" \
    slangmosh.json > slangmosh.regen.json
  "$SLANGMOSH_BIN" slangmosh.regen.json --vk11 -O --strip --namespace RDP --output slangmosh.hpp
)
