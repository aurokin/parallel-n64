#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
RUNNER="$SCRIPT_DIR/run-paper-mario-hires-intro22-state-capture.sh"
CANONICAL_ROOT="/tmp/parallel-n64-paper-mario-captures"
CANONICAL_TAG="intro22-state-baseline"

tag="$CANONICAL_TAG"
capture_root="$CANONICAL_ROOT"
declare -a extra_args=()

usage() {
  cat <<'EOF_USAGE'
Usage:
  run-paper-mario-hires-intro22-baseline-capture.sh [options] [-- EXTRA_ARGS...]

Options:
  --tag NAME          Override the default canonical tag (default: intro22-state-baseline)
  --capture-root DIR  Override the capture root (default: /tmp/parallel-n64-paper-mario-captures)
  -h, --help          Show this help

Behavior:
  - Captures the canonical intro22-state + 1f baseline
  - Reuses the standardized seeded-state helper
  - Writes to /tmp/parallel-n64-paper-mario-captures/intro22-state-baseline by default
EOF_USAGE
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

exec "$RUNNER" --tag "$tag" --capture-root "$capture_root" "${extra_args[@]}"
