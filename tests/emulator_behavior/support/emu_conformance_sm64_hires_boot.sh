#!/usr/bin/env bash
set -euo pipefail

# SM64 hi-res boot conformance test.
# Runs through the shared evidence-bundle boot validator and asserts provider
# load plus GlideN64-compat draw-time hits.

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "$SCRIPT_DIR/../../.." && pwd)"

CACHE_PATH_DEFAULT="$REPO_ROOT/artifacts/hts2phrb/sm64-reloaded/package.phrb"
CACHE_PATH="${EMU_RUNTIME_SM64_PHRB:-$CACHE_PATH_DEFAULT}"
ROM_PATH_DEFAULT="$REPO_ROOT/assets/Super Mario 64 (USA).zip"
ROM_PATH="${EMU_RUNTIME_SM64_ROM:-$ROM_PATH_DEFAULT}"
BUNDLE_DIR="${EMU_RUNTIME_SM64_BUNDLE_DIR:-}"

if [[ "${EMU_ENABLE_RUNTIME_CONFORMANCE:-0}" != "1" ]]; then
  echo "SKIP: set EMU_ENABLE_RUNTIME_CONFORMANCE=1 to run SM64 hi-res boot conformance."
  exit 77
fi

if [[ ! -f "$CACHE_PATH" ]]; then
  echo "SKIP: SM64 PHRB package not found at $CACHE_PATH (set EMU_RUNTIME_SM64_PHRB to override)."
  exit 77
fi

if [[ ! -f "$ROM_PATH" ]]; then
  echo "SKIP: SM64 ROM not found at $ROM_PATH (set EMU_RUNTIME_SM64_ROM to override)."
  exit 77
fi

CORE_PATH="$REPO_ROOT/parallel_n64_libretro.so"
if [[ ! -f "$CORE_PATH" ]]; then
  echo "SKIP: libretro core not found at $CORE_PATH."
  exit 77
fi

RETROARCH_BIN="${RETROARCH_BIN:-$(command -v retroarch || true)}"
RETROARCH_BASE_CONFIG="${RETROARCH_BASE_CONFIG:-}"
if [[ ! -x "$RETROARCH_BIN" ]]; then
  echo "SKIP: RetroArch binary not found at $RETROARCH_BIN."
  exit 77
fi
if [[ ! -f "$RETROARCH_BASE_CONFIG" ]]; then
  echo "SKIP: RetroArch base config not found at $RETROARCH_BASE_CONFIG."
  exit 77
fi

args=(
  --game-id sm64
  --rom "$ROM_PATH"
  --cache-path "$CACHE_PATH"
  --seconds 30
  --min-entries 1
  --min-compat-draw-hits 1
  --expected-entry-class compat-only
)
if [[ -n "$BUNDLE_DIR" ]]; then
  args+=(--bundle-dir "$BUNDLE_DIR")
fi

CORE_PATH="$CORE_PATH" \
RETROARCH_BIN="$RETROARCH_BIN" \
RETROARCH_BASE_CONFIG="$RETROARCH_BASE_CONFIG" \
  "$REPO_ROOT/tools/scenarios/cross-game-hires-boot-validation.sh" "${args[@]}"

echo "emu_conformance_sm64_hires_boot: PASS ($CACHE_PATH)"
