#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
validator_bin="${RDP_VALIDATE_DUMP_BIN:-}"
dump_dir="${RDP_DUMP_CORPUS_DIR:-$SCRIPT_DIR/tests/rdp_dumps}"
declare -a passthrough_args=()

usage() {
  cat <<'USAGE'
Usage:
  run-dump-tests.sh [options] [-- CTEST_ARGS...]

Options:
  --validator PATH     Path to rdp-validate-dump binary
  --dump-dir PATH      Dump corpus directory (default: ./tests/rdp_dumps)
  -h, --help           Show this help

Examples:
  ./run-dump-tests.sh
  ./run-dump-tests.sh --validator /opt/parallel-rdp/rdp-validate-dump
  ./run-dump-tests.sh --dump-dir ./local_dumps -- --output-on-failure
USAGE
}

while (($#)); do
  case "$1" in
    --validator)
      shift
      validator_bin="${1:-}"
      if [[ -z "$validator_bin" ]]; then
        echo "--validator requires a path." >&2
        exit 2
      fi
      ;;
    --dump-dir)
      shift
      dump_dir="${1:-}"
      if [[ -z "$dump_dir" ]]; then
        echo "--dump-dir requires a path." >&2
        exit 2
      fi
      ;;
    --)
      shift
      passthrough_args+=("$@")
      break
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      passthrough_args+=("$1")
      ;;
  esac
  shift
done

if [[ -n "$validator_bin" ]]; then
  export RDP_VALIDATE_DUMP_BIN="$validator_bin"
fi
export RDP_DUMP_CORPUS_DIR="$dump_dir"

echo "[dump-tests] validator: ${RDP_VALIDATE_DUMP_BIN:-<auto>}" 
echo "[dump-tests] corpus: $RDP_DUMP_CORPUS_DIR"

"$SCRIPT_DIR/run-tests.sh" -R emu.dump "${passthrough_args[@]}"
