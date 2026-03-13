#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
COMPARE_RUNNER="$SCRIPT_DIR/run-paper-mario-hires-zoom-compare.sh"
CAPTURE_ROOT="/tmp/parallel-n64-paper-mario-captures"
COMPARE_ROOT="/tmp/parallel-n64-paper-mario-hires-compare"
DEFAULT_BASELINE="$CAPTURE_ROOT/intro22-state-baseline"
DEFAULT_OUTPUT="$COMPARE_ROOT/latest-intro22-probe"

candidate=""
tag=""
baseline=""
output_dir="$DEFAULT_OUTPUT"

usage() {
  cat <<'EOF_USAGE'
Usage:
  run-paper-mario-hires-intro22-probe-compare.sh [options]

Options:
  --candidate PNG   Candidate/probe screenshot to compare
  --tag NAME        Resolve the newest PNG under /tmp/parallel-n64-paper-mario-captures/NAME
  --baseline PNG    Explicit baseline PNG (defaults to newest PNG under intro22-state-baseline)
  --output-dir DIR  Output directory (default: /tmp/parallel-n64-paper-mario-hires-compare/latest-intro22-probe)
  -h, --help        Show this help

Behavior:
  - Compares a probe against the canonical intro22-state baseline, not GLide
  - Uses the same intro22 crop profile and exact-diff reporting as the oracle compare
  - This is the first gate for any renderer proof; use GLide only after the probe shows a real baseline diff
EOF_USAGE
}

latest_png_in_dir() {
  local dir="$1"
  find "$dir" -maxdepth 1 -type f -name '*.png' -printf '%T@ %p\n' 2>/dev/null | sort -nr | head -n 1 | cut -d' ' -f2-
}

while (($#)); do
  case "$1" in
    --candidate)
      shift
      candidate="${1:-}"
      ;;
    --tag)
      shift
      tag="${1:-}"
      ;;
    --baseline)
      shift
      baseline="${1:-}"
      ;;
    --output-dir)
      shift
      output_dir="${1:-}"
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "Unknown argument: $1" >&2
      usage >&2
      exit 1
      ;;
  esac
  shift
done

if [[ -n "$candidate" && -n "$tag" ]]; then
  echo "Use only one of --candidate or --tag." >&2
  exit 1
fi

if [[ -n "$tag" ]]; then
  candidate="$(latest_png_in_dir "$CAPTURE_ROOT/$tag")"
fi

if [[ -z "$candidate" || ! -f "$candidate" ]]; then
  echo "Candidate screenshot not found: ${candidate:-<empty>}" >&2
  exit 1
fi

if [[ -z "$baseline" ]]; then
  baseline="$(latest_png_in_dir "$DEFAULT_BASELINE")"
fi

if [[ -z "$baseline" || ! -f "$baseline" ]]; then
  echo "Baseline screenshot not found: ${baseline:-<empty>}" >&2
  exit 1
fi

mkdir -p "$output_dir"

exec "$COMPARE_RUNNER" \
  --candidate "$candidate" \
  --oracle "$baseline" \
  --profile intro22 \
  --latest-alias intro22-probe \
  --output-dir "$output_dir" \
  --candidate-label probe \
  --oracle-label baseline \
  --fixed-boxes
