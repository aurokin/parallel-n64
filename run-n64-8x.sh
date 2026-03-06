#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
RUNNER="$SCRIPT_DIR/run-n64.sh"
CORE_OPT_PATH="${RETROARCH_CORE_OPTIONS_PATH:-$HOME/.config/retroarch/config/ParaLLEl N64/ParaLLEl N64.opt}"
UPSCALE_KEY="parallel-n64-parallel-rdp-upscaling"
UPSCALE_VALUE="8x"

declare -a runner_args=("$@")
backup_opt=""
had_original_opt=0

usage() {
  cat <<'EOF_USAGE'
Usage:
  run-n64-8x.sh [RUN_N64_ARGS...]

Behavior:
  - Temporarily sets ParaLLEl core option `parallel-n64-parallel-rdp-upscaling` to `8x`.
  - Runs ./run-n64.sh with all forwarded arguments.
  - Restores the previous core-option file after exit.

Env:
  RETROARCH_CORE_OPTIONS_PATH  Override core options file path.

Examples:
  ./run-n64-8x.sh
  ./run-n64-8x.sh -- --verbose
  ./run-n64-8x.sh --reference -- --verbose
EOF_USAGE
}

restore_core_options() {
  if [[ -n "$backup_opt" && -f "$backup_opt" ]]; then
    if (( had_original_opt )); then
      mkdir -p "$(dirname -- "$CORE_OPT_PATH")"
      cp -f "$backup_opt" "$CORE_OPT_PATH"
    else
      rm -f "$CORE_OPT_PATH" || true
    fi
    rm -f "$backup_opt" || true
  fi
}

cleanup() {
  restore_core_options
}

set_core_option_value() {
  local path="$1"
  local key="$2"
  local value="$3"
  local tmp
  tmp="$(mktemp /tmp/parallel-n64-core-opt.XXXX.tmp)"

  if [[ -f "$path" ]]; then
    awk -v key="$key" -v value="$value" '
      BEGIN {
        line = key " = \"" value "\""
        seen = 0
      }
      {
        if ($0 ~ "^[[:space:]]*" key "[[:space:]]*=") {
          if (!seen)
            print line
          seen = 1
          next
        }
        print
      }
      END {
        if (!seen)
          print line
      }
    ' "$path" > "$tmp"
  else
    printf '%s = "%s"\n' "$key" "$value" > "$tmp"
  fi

  mkdir -p "$(dirname -- "$path")"
  mv "$tmp" "$path"
}

if [[ ! -x "$RUNNER" ]]; then
  echo "run-n64.sh is missing or not executable: $RUNNER" >&2
  exit 1
fi

if (($# > 0)); then
  case "$1" in
    -h|--help)
      usage
      exit 0
      ;;
  esac
fi

trap cleanup EXIT INT TERM

if [[ -f "$CORE_OPT_PATH" ]]; then
  had_original_opt=1
fi

backup_opt="$(mktemp /tmp/parallel-n64-core-opt-backup.XXXX.opt)"
if (( had_original_opt )); then
  cp -f "$CORE_OPT_PATH" "$backup_opt"
fi

set_core_option_value "$CORE_OPT_PATH" "$UPSCALE_KEY" "$UPSCALE_VALUE"

echo "8x launcher: forcing $UPSCALE_KEY=$UPSCALE_VALUE"
"$RUNNER" "${runner_args[@]}"
