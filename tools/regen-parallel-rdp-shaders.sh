#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd -- "$SCRIPT_DIR/.." && pwd)"
SHADER_DIR="$REPO_DIR/mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp/shaders"
SHADER_HEADER="$SHADER_DIR/slangmosh.hpp"
DEFAULT_SLANGMOSH="$HOME/code/mupen/parallel-rdp-upstream/build/Granite/slangmosh/slangmosh"

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

(
  cd "$SHADER_DIR"
  "$SLANGMOSH_BIN" slangmosh.json -O --strip --output slangmosh.hpp
)
