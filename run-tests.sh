#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${BUILD_DIR:-$SCRIPT_DIR/build/ctest}"
PARALLEL_JOBS="${PARALLEL_JOBS:-$(nproc)}"

clean_build=0
list_only=0
selected_profile="all"
has_regex_override=0
declare -a ctest_args
ctest_args=(--output-on-failure)

usage() {
  cat <<'EOF'
Usage:
  run-tests.sh [options] [-- CTEST_ARGS...]

Options:
  --clean               Remove build dir before configuring
  --list                List tests without running them
  --build-dir PATH      Override build dir (default: ./build/ctest)
  --profile NAME        Test profile: all|emu-required|emu-optional|emu-conformance|emu-dump
  -R REGEX              Pass test regex to ctest
  -h, --help            Show this help

Examples:
  ./run-tests.sh
  ./run-tests.sh --profile emu-required
  ./run-tests.sh --list
  ./run-tests.sh -R hires.texture_keying
  ./run-tests.sh -- --repeat until-fail:10
EOF
}

while (($#)); do
  case "$1" in
    --clean)
      clean_build=1
      ;;
    --list)
      list_only=1
      ;;
    --build-dir)
      shift
      BUILD_DIR="${1:-}"
      if [[ -z "$BUILD_DIR" ]]; then
        echo "--build-dir requires a path." >&2
        exit 2
      fi
      ;;
    --profile)
      shift
      selected_profile="${1:-}"
      if [[ -z "$selected_profile" ]]; then
        echo "--profile requires a value." >&2
        exit 2
      fi
      ;;
    -R)
      shift
      if [[ -z "${1:-}" ]]; then
        echo "-R requires a regex value." >&2
        exit 2
      fi
      has_regex_override=1
      ctest_args+=(-R "$1")
      ;;
    --)
      shift
      ctest_args+=("$@")
      break
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      ctest_args+=("$1")
      ;;
  esac
  shift
done

if (( has_regex_override )) && [[ "$selected_profile" != "all" ]]; then
  echo "--profile cannot be combined with -R." >&2
  exit 2
fi

case "$selected_profile" in
  all)
    ;;
  emu-required)
    ctest_args+=(-R "^emu\\.unit\\.")
    ;;
  emu-optional)
    ctest_args+=(-R "^emu\\.(conformance|dump)\\.")
    ;;
  emu-conformance)
    ctest_args+=(-R "^emu\\.conformance\\.")
    ;;
  emu-dump)
    ctest_args+=(-R "^emu\\.dump\\.")
    ;;
  *)
    echo "Unknown --profile value: $selected_profile" >&2
    exit 2
    ;;
esac

if (( clean_build )) && [[ -d "$BUILD_DIR" ]]; then
  rm -rf "$BUILD_DIR"
fi

echo "[tests] profile: $selected_profile"
echo "[tests] configure: $BUILD_DIR"
cmake -S "$SCRIPT_DIR" -B "$BUILD_DIR"

echo "[tests] build: $BUILD_DIR"
cmake --build "$BUILD_DIR" --parallel "$PARALLEL_JOBS"

if (( list_only )); then
  echo "[tests] list"
  ctest --test-dir "$BUILD_DIR" -N "${ctest_args[@]}"
else
  echo "[tests] run"
  ctest --test-dir "$BUILD_DIR" "${ctest_args[@]}"
fi
