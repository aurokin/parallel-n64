#!/usr/bin/env bash
set -euo pipefail

# SM64 hi-res savestate fixture conformance test.
# Validates the fixed baseline-off title boot state through the shared
# hi-res savestate evidence-bundle validator.

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "$SCRIPT_DIR/../../.." && pwd)"

CACHE_PATH_DEFAULT="$REPO_ROOT/artifacts/hts2phrb/sm64-reloaded/package.phrb"
CACHE_PATH="${EMU_RUNTIME_SM64_PHRB:-$CACHE_PATH_DEFAULT}"
ROM_PATH_DEFAULT="$REPO_ROOT/assets/Super Mario 64 (USA).zip"
ROM_PATH="${EMU_RUNTIME_SM64_ROM:-$ROM_PATH_DEFAULT}"
STATE_PATH_DEFAULT="$REPO_ROOT/assets/states/sm64-title-boot/ParaLLEl N64/Super Mario 64 (USA).state"
STATE_PATH="${EMU_RUNTIME_SM64_TITLE_STATE:-$STATE_PATH_DEFAULT}"
BUNDLE_DIR="${EMU_RUNTIME_SM64_TITLE_FIXTURE_BUNDLE_DIR:-}"
EXPECTED_STATE_SHA256="${EMU_RUNTIME_SM64_TITLE_STATE_SHA256:-8d56038ab4616c652632689b5d662e4fc66d686cf76a23f9670d0ec199c5ca03}"

if [[ "${EMU_ENABLE_RUNTIME_CONFORMANCE:-0}" != "1" ]]; then
  echo "SKIP: set EMU_ENABLE_RUNTIME_CONFORMANCE=1 to run SM64 hi-res title fixture conformance."
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

if [[ ! -f "$STATE_PATH" ]]; then
  echo "SKIP: SM64 title savestate not found at $STATE_PATH (set EMU_RUNTIME_SM64_TITLE_STATE or remint with tools/scenarios/remint-cross-game-boot-state.sh)."
  exit 77
fi

CORE_PATH="$REPO_ROOT/parallel_n64_libretro.so"
if [[ ! -f "$CORE_PATH" ]]; then
  echo "SKIP: libretro core not found at $CORE_PATH."
  exit 77
fi

RETROARCH_BIN="${RETROARCH_BIN:-/home/auro/code/RetroArch/retroarch}"
RETROARCH_BASE_CONFIG="${RETROARCH_BASE_CONFIG:-/home/auro/code/RetroArch/retroarch.cfg}"
if [[ ! -x "$RETROARCH_BIN" ]]; then
  echo "SKIP: RetroArch binary not found at $RETROARCH_BIN."
  exit 77
fi
if [[ ! -f "$RETROARCH_BASE_CONFIG" ]]; then
  echo "SKIP: RetroArch base config not found at $RETROARCH_BASE_CONFIG."
  exit 77
fi

args=(
  --game-id sm64-title-boot
  --rom "$ROM_PATH"
  --cache-path "$CACHE_PATH"
  --state-path "$STATE_PATH"
  --expected-state-sha256 "$EXPECTED_STATE_SHA256"
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
  "$REPO_ROOT/tools/scenarios/cross-game-hires-savestate-fixture-validation.sh" "${args[@]}"

echo "emu_conformance_sm64_hires_title_fixture: PASS ($CACHE_PATH)"
