#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
COMPARE_TOOL="$SCRIPT_DIR/tools/paper_mario_hires_zoom_compare.py"
CAPTURE_ROOT="/tmp/parallel-n64-paper-mario-captures"
OUTPUT_ROOT="/tmp/parallel-n64-paper-mario-hires-compare"

candidate=""
tag=""
profile="intro22"
oracle=""
output_dir=""
candidate_label=""
oracle_label=""
latest_alias=""
fixed_boxes="0"

usage() {
  cat <<'EOF_USAGE'
Usage:
  run-paper-mario-hires-zoom-compare.sh [options]

Options:
  --candidate PNG   Candidate screenshot to compare
  --tag NAME        Resolve the newest PNG under /tmp/parallel-n64-paper-mario-captures/NAME
  --profile NAME    Compare profile: intro22|noinput16 (default: intro22)
  --oracle PNG      Override the profile's default GLide HIRES oracle
  --candidate-label LABEL  Override the candidate column label
  --oracle-label LABEL     Override the oracle/reference column label
  --latest-alias NAME      Override the latest symlink alias (default: profile name)
  --fixed-boxes            Compare using identical boxes with no alignment search
  --output-dir DIR  Override the output directory
  -h, --help        Show this help

Behavior:
  - Defaults to the newest PNG under /tmp/parallel-n64-paper-mario-captures when no candidate is given.
  - `intro22` compares against the saved matched GLideN64 4x HIRES intro22 oracle.
  - `noinput16` keeps the older saved GLideN64 4x HIRES no-input 16s oracle.
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
    --profile)
      shift
      profile="${1:-}"
      ;;
    --oracle)
      shift
      oracle="${1:-}"
      ;;
    --candidate-label)
      shift
      candidate_label="${1:-}"
      ;;
    --oracle-label)
      shift
      oracle_label="${1:-}"
      ;;
    --latest-alias)
      shift
      latest_alias="${1:-}"
      ;;
    --fixed-boxes)
      fixed_boxes="1"
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

if [[ "$profile" != "intro22" && "$profile" != "noinput16" ]]; then
  echo "Unknown --profile: $profile" >&2
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

if [[ ! -f "$COMPARE_TOOL" ]]; then
  echo "Compare tool missing: $COMPARE_TOOL" >&2
  exit 1
fi

if [[ -z "$output_dir" ]]; then
  name="${tag:-$(basename "${candidate%.*}")}"
  output_dir="$OUTPUT_ROOT/$name"
fi

mkdir -p "$output_dir"

declare -a cmd=(
  python3 "$COMPARE_TOOL"
  --candidate "$candidate"
  --profile "$profile"
  --output-dir "$output_dir"
)

if [[ -n "$oracle" ]]; then
  cmd+=(--oracle "$oracle")
fi

if [[ -n "$candidate_label" ]]; then
  cmd+=(--candidate-label "$candidate_label")
fi

if [[ -n "$oracle_label" ]]; then
  cmd+=(--oracle-label "$oracle_label")
fi

if [[ "$fixed_boxes" == "1" ]]; then
  cmd+=(--fixed-boxes)
fi

"${cmd[@]}"

if [[ -z "$latest_alias" ]]; then
  latest_alias="$profile"
fi

latest_link="$OUTPUT_ROOT/latest-$latest_alias"
ln -sfn "$output_dir" "$latest_link"

echo "Latest link: $latest_link"
