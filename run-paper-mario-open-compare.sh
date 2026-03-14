#!/usr/bin/env bash
set -euo pipefail

SESSION_NAME="paper-mario-compare"
COMPARE_ROOT="/tmp/parallel-n64-paper-mario-hires-compare"
OUTPUT_ROOT="/tmp/parallel-n64-paper-mario-captures"
COMPARE_RUNNER="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)/run-paper-mario-hires-zoom-compare.sh"

profile=""
image=""
rebuild="0"

usage() {
  cat <<'EOF_USAGE'
Usage:
  run-paper-mario-open-compare.sh [options]

Options:
  --profile NAME  Prefer the latest compare output for a specific profile/tag prefix
  --image PATH    Open an explicit image instead of auto-resolving the latest compare
  --rebuild       Force regeneration of the compare before opening
  -h, --help      Show this help

Behavior:
  - Reuses tmux session `paper-mario-compare`.
  - Defaults to the newest `summary.png` under /tmp/parallel-n64-paper-mario-hires-compare.
  - For `--profile intro22`, it rebuilds into canonical `latest-intro22`.
  - For `--profile intro22-probe`, it opens canonical `latest-intro22-probe` without rebuilding.
EOF_USAGE
}

latest_image() {
  local root="$1"
  local prefix="${2:-}"

  if [[ ! -d "$root" ]]; then
    return 0
  fi

  if [[ -n "$prefix" ]]; then
    find "$root" -mindepth 2 -maxdepth 2 -type f -name 'summary.png' -path "*/${prefix}*/summary.png" -printf '%T@ %p\n' 2>/dev/null | sort -nr | head -n 1 | cut -d' ' -f2-
    return 0
  fi

  find "$root" -mindepth 2 -maxdepth 2 -type f -name 'summary.png' -printf '%T@ %p\n' 2>/dev/null | sort -nr | head -n 1 | cut -d' ' -f2-
}

latest_profile_capture_png() {
  local profile="$1"
  local prefix=""
  local capture_dir=""

  case "$profile" in
    intro22)
      prefix="intro22"
      ;;
    noinput16)
      prefix="noinput16"
      ;;
    *)
      return 0
      ;;
  esac

  capture_dir="$(find "$OUTPUT_ROOT" -mindepth 1 -maxdepth 1 -type d -name "${prefix}*" -printf '%T@ %p\n' 2>/dev/null | sort -nr | head -n 1 | cut -d' ' -f2-)"
  if [[ -z "$capture_dir" ]]; then
    return 0
  fi

  find "$capture_dir" -maxdepth 1 -type f -name '*.png' -printf '%T@ %p\n' 2>/dev/null | sort -nr | head -n 1 | cut -d' ' -f2-
}

while (($#)); do
  case "$1" in
    --profile)
      shift
      profile="${1:-}"
      ;;
    --image)
      shift
      image="${1:-}"
      ;;
    --rebuild)
      rebuild="1"
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

if [[ "$profile" == "intro22-probe" && -z "$image" ]]; then
  image="$COMPARE_ROOT/latest-intro22-probe/summary.png"
elif [[ -n "$profile" && -z "$image" && -x "$COMPARE_RUNNER" ]]; then
  canonical_dir="$COMPARE_ROOT/latest-$profile"
  candidate_png="$(latest_profile_capture_png "$profile")"
  if [[ -n "$candidate_png" && -f "$candidate_png" ]]; then
    "$COMPARE_RUNNER" --profile "$profile" --candidate "$candidate_png" --output-dir "$canonical_dir" >/dev/null
  else
    "$COMPARE_RUNNER" --profile "$profile" --output-dir "$canonical_dir" >/dev/null
  fi
  image="$canonical_dir/summary.png"
elif [[ -z "$image" ]]; then
  image="$(latest_image "$COMPARE_ROOT" "$profile")"
fi

if [[ "$rebuild" == "1" && -n "$profile" && "$profile" != "intro22-probe" && -z "$image" && -x "$COMPARE_RUNNER" ]]; then
  canonical_dir="$COMPARE_ROOT/latest-$profile"
  "$COMPARE_RUNNER" --profile "$profile" --output-dir "$canonical_dir" >/dev/null
  image="$canonical_dir/summary.png"
fi

if [[ -z "$image" && -n "$profile" && "$profile" != "intro22-probe" && -x "$COMPARE_RUNNER" ]]; then
  "$COMPARE_RUNNER" --profile "$profile" >/dev/null
  image="$(latest_image "$COMPARE_ROOT" "$profile")"
fi

if [[ -z "$image" ]]; then
  image="$(latest_image "$COMPARE_ROOT")"
fi

if [[ -z "$image" || ! -f "$image" ]]; then
  echo "Latest compare image not found." >&2
  exit 1
fi

tmux kill-session -t "$SESSION_NAME" 2>/dev/null || true
tmux new-session -d -s "$SESSION_NAME" "sh -lc '
  feh --auto-zoom \"$image\" &
  feh_pid=\$!
  if command -v xdotool >/dev/null 2>&1; then
    tries=60
    while [ \$tries -gt 0 ]; do
      wid=\$(xdotool search --onlyvisible --pid \$feh_pid 2>/dev/null | head -n 1 || true)
      if [ -n \"\$wid\" ]; then
        xdotool windowactivate \"\$wid\" >/dev/null 2>&1 || true
        xdotool windowstate --add MAXIMIZED_VERT \"\$wid\" >/dev/null 2>&1 || true
        xdotool windowstate --add MAXIMIZED_HORZ \"\$wid\" >/dev/null 2>&1 || true
        break
      fi
      sleep 0.1
      tries=\$((tries - 1))
    done
  fi
  wait \$feh_pid
'"

echo "Opened: $image"
echo "To monitor:"
echo "  tmux attach -t $SESSION_NAME"
echo "  tmux capture-pane -p -J -t ${SESSION_NAME}:0.0 -S -20"
