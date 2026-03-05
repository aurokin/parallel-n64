#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="$ROOT_DIR/build/tests/m3"
CXX="${CXX:-g++}"

INCLUDE_DIR="$ROOT_DIR/mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp"
KEYING_TEST_SRC="$ROOT_DIR/tests/m3/texture_keying_test.cpp"
REPLACEMENT_TEST_SRC="$ROOT_DIR/tests/m3/texture_replacement_provider_test.cpp"
REPLACEMENT_IMPL_SRC="$ROOT_DIR/mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp/texture_replacement.cpp"

CACHE_DIR="${M3_HIRES_CACHE_DIR:-$ROOT_DIR}"

mkdir -p "$BUILD_DIR"

echo "[m3-tests] building texture_keying_test"
"$CXX" -std=c++17 -O2 -Wall -Wextra -pedantic \
  -I"$INCLUDE_DIR" \
  "$KEYING_TEST_SRC" \
  -o "$BUILD_DIR/texture_keying_test"

echo "[m3-tests] running texture_keying_test"
"$BUILD_DIR/texture_keying_test"

echo "[m3-tests] building texture_replacement_provider_test"
"$CXX" -std=c++17 -O2 -Wall -Wextra -pedantic \
  -I"$INCLUDE_DIR" \
  "$REPLACEMENT_TEST_SRC" "$REPLACEMENT_IMPL_SRC" \
  -lz \
  -o "$BUILD_DIR/texture_replacement_provider_test"

echo "[m3-tests] running texture_replacement_provider_test (cache dir: $CACHE_DIR)"
"$BUILD_DIR/texture_replacement_provider_test" "$CACHE_DIR"

echo "[m3-tests] PASS"
