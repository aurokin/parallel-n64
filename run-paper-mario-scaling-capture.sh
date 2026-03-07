#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
RUNNER="$SCRIPT_DIR/run-paper-mario-hires-capture.sh"

tag=""
declare -a passthrough=()

usage() {
  cat <<'EOF_USAGE'
Usage:
  run-paper-mario-scaling-capture.sh [options] [-- RUNNER_ARGS...]

Options:
  --tag NAME   Capture subdirectory name
  -h, --help   Show this help

Behavior:
  - Uses same-core state-mode capture for Paper Mario.
  - Forces HIRES off for scaling work.
  - Forces ParaLLEl 4x upscaling with downscaling disabled.
  - Leaves other capture behavior to run-paper-mario-hires-capture.sh.
EOF_USAGE
}

while (($#)); do
  case "$1" in
    --tag)
      shift
      tag="${1:-}"
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    --)
      shift
      passthrough+=("$@")
      break
      ;;
    *)
      passthrough+=("$1")
      ;;
  esac
  shift
done

if [[ ! -x "$RUNNER" ]]; then
  echo "run-paper-mario-hires-capture.sh is missing or not executable: $RUNNER" >&2
  exit 1
fi

declare -a cmd
cmd=(
  "$RUNNER"
  --smoke-mode state
  --core-option parallel-n64-parallel-rdp-hirestex=disabled
  --core-option parallel-n64-parallel-rdp-upscaling=4x
  --core-option parallel-n64-parallel-rdp-downscaling=disable
)

if [[ -n "$tag" ]]; then
  cmd+=(--tag "$tag")
fi

cmd+=("${passthrough[@]}")

exec "${cmd[@]}"
