#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
COMPARE_TOOL="$SCRIPT_DIR/tools/paper_mario_scaling_compare.py"
DEFAULT_ORACLE="/home/auro/code/parallel-n64-paper-mario-backups/20260306-hires-audit/scaling/oracle-gliden64-4x-hires-off-2/Paper Mario (USA)-260306-212123.png"
CAPTURE_ROOT="/tmp/parallel-n64-paper-mario-captures"
OUTPUT_ROOT="/tmp/parallel-n64-paper-mario-scaling-compare"

candidate=""
tag=""
oracle="$DEFAULT_ORACLE"
output_dir=""

usage() {
  cat <<'EOF_USAGE'
Usage:
  run-paper-mario-scaling-compare.sh [options]

Options:
  --candidate PNG   Candidate screenshot to compare
  --tag NAME        Resolve the newest PNG under /tmp/parallel-n64-paper-mario-captures/NAME
  --oracle PNG      Override the default GLide oracle
  --output-dir DIR  Override the output directory
  -h, --help        Show this help

Behavior:
  - Aligns a same-core Paper Mario scaling capture to the saved GLide 4x HIRES-off oracle.
  - Writes summary metrics and visual diffs into /tmp/parallel-n64-paper-mario-scaling-compare/<name>.
  - Prints the same summary to stdout for quick iteration.
EOF_USAGE
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
    --oracle)
      shift
      oracle="${1:-}"
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

if [[ -z "$candidate" && -z "$tag" ]]; then
  echo "Either --candidate or --tag is required." >&2
  usage >&2
  exit 1
fi

if [[ -n "$candidate" && -n "$tag" ]]; then
  echo "Use only one of --candidate or --tag." >&2
  usage >&2
  exit 1
fi

if [[ -n "$tag" ]]; then
  candidate="$(find "$CAPTURE_ROOT/$tag" -maxdepth 1 -type f -name '*.png' | sort | tail -n 1)"
fi

if [[ -z "$candidate" || ! -f "$candidate" ]]; then
  echo "Candidate screenshot not found: ${candidate:-<empty>}" >&2
  exit 1
fi

if [[ ! -f "$oracle" ]]; then
  echo "Oracle screenshot not found: $oracle" >&2
  exit 1
fi

if [[ ! -f "$COMPARE_TOOL" ]]; then
  echo "Compare tool missing: $COMPARE_TOOL" >&2
  exit 1
fi

if [[ -z "$output_dir" ]]; then
  name="${tag:-$(basename "${candidate%.*}")}"
  output_dir="$OUTPUT_ROOT/$name"
fi

mkdir -p "$output_dir"

exec python3 "$COMPARE_TOOL" \
  --candidate "$candidate" \
  --oracle "$oracle" \
  --output-dir "$output_dir"
