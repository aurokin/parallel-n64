#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
COMPARE_TOOL="$SCRIPT_DIR/tools/paper_mario_hires_zoom_compare.py"
DEFAULT_ORACLE="/home/auro/code/parallel-n64-paper-mario-backups/20260306-hires-audit/hires/oracle-gliden64-4x-hires-on-noinput-16s-1/Paper Mario (USA)-260308-011300.png"
CAPTURE_ROOT="/tmp/parallel-n64-paper-mario-captures"
OUTPUT_ROOT="/tmp/parallel-n64-paper-mario-hires-compare"

candidate=""
tag=""
oracle="$DEFAULT_ORACLE"
output_dir=""

usage() {
  cat <<'EOF_USAGE'
Usage:
  run-paper-mario-hires-zoom-compare.sh [options]

Options:
  --candidate PNG   Candidate screenshot to compare
  --tag NAME        Resolve the newest PNG under /tmp/parallel-n64-paper-mario-captures/NAME
  --oracle PNG      Override the default GLide HIRES oracle
  --output-dir DIR  Override the output directory
  -h, --help        Show this help

Behavior:
  - Defaults to the newest PNG under /tmp/parallel-n64-paper-mario-captures when no candidate is given.
  - Compares the candidate against the saved GLideN64 4x HIRES no-input 16s oracle.
  - Emits focused zoom crops for top banner, today text, bottom stage grid, and left stage grid.
EOF_USAGE
}

latest_candidate() {
  local line=""
  local latest_dir=""
  local latest_png=""

  while IFS= read -r line; do
    latest_dir="$line"
    break
  done < <(find "$CAPTURE_ROOT" -mindepth 1 -maxdepth 1 -type d -printf '%T@ %p\n' 2>/dev/null | sort -nr)

  if [[ -z "$latest_dir" ]]; then
    return 0
  fi

  latest_dir="${latest_dir#* }"
  while IFS= read -r line; do
    latest_png="$line"
    break
  done < <(find "$latest_dir" -maxdepth 1 -type f -name '*.png' -printf '%T@ %p\n' 2>/dev/null | sort -nr)

  if [[ -n "$latest_png" ]]; then
    printf '%s\n' "${latest_png#* }"
  fi
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

if [[ -n "$candidate" && -n "$tag" ]]; then
  echo "Use only one of --candidate or --tag." >&2
  usage >&2
  exit 1
fi

if [[ -n "$tag" ]]; then
  candidate="$(find "$CAPTURE_ROOT/$tag" -maxdepth 1 -type f -name '*.png' | sort | tail -n 1)"
elif [[ -z "$candidate" ]]; then
  candidate="$(latest_candidate)"
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
