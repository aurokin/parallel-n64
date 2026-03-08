#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
CAPTURE_RUNNER="$SCRIPT_DIR/run-paper-mario-hires-intro22-capture.sh"
COMPARE_RUNNER="$SCRIPT_DIR/run-paper-mario-hires-intro22-compare.sh"
OPEN_RUNNER="$SCRIPT_DIR/run-paper-mario-open-compare.sh"

tag=""
open_only="0"
declare -a extra_args=()

usage() {
  cat <<'EOF_USAGE'
Usage:
  run-paper-mario-hires-intro22-refresh.sh [options] [-- EXTRA_CAPTURE_ARGS...]

Options:
  --tag NAME    Capture tag to pass through
  --open-only   Skip capture/compare and only reopen the latest intro22 compare
  -h, --help    Show this help

Behavior:
  - Runs the current intro22 `parallel` HIRES capture
  - Builds the intro22 compare output for that capture
  - Reopens the stable tmux/feh viewer session on the new summary
EOF_USAGE
}

while (($#)); do
  case "$1" in
    --tag)
      shift
      tag="${1:-}"
      ;;
    --open-only)
      open_only="1"
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    --)
      shift
      extra_args+=("$@")
      break
      ;;
    *)
      echo "Unknown argument: $1" >&2
      usage >&2
      exit 1
      ;;
  esac
  shift
done

if [[ "$open_only" == "1" ]]; then
  exec "$OPEN_RUNNER" --profile intro22
fi

if [[ -z "$tag" ]]; then
  tag="intro22-refresh-$(date +%Y%m%d-%H%M%S)"
fi

"$CAPTURE_RUNNER" --tag "$tag" "${extra_args[@]}"
"$COMPARE_RUNNER" --tag "$tag" >/dev/null
exec "$OPEN_RUNNER" --profile intro22
