#!/usr/bin/env bash
set -euo pipefail

# OoT hi-res savestate fixture conformance test.
# Validates the fixed baseline-off title boot state through the shared
# hi-res savestate evidence-bundle validator.

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "$SCRIPT_DIR/../../.." && pwd)"

CACHE_PATH_DEFAULT="$REPO_ROOT/artifacts/hts2phrb/oot-reloaded/package.phrb"
CACHE_PATH="${EMU_RUNTIME_OOT_PHRB:-$CACHE_PATH_DEFAULT}"
ROM_PATH_DEFAULT="$REPO_ROOT/assets/Legend of Zelda, The - Ocarina of Time (USA).zip"
ROM_PATH="${EMU_RUNTIME_OOT_ROM:-$ROM_PATH_DEFAULT}"
STATE_PATH_DEFAULT="$REPO_ROOT/assets/states/oot-title-boot/ParaLLEl N64/Legend of Zelda, The - Ocarina of Time (USA).state"
STATE_PATH="${EMU_RUNTIME_OOT_TITLE_STATE:-$STATE_PATH_DEFAULT}"
BUNDLE_DIR="${EMU_RUNTIME_OOT_TITLE_FIXTURE_BUNDLE_DIR:-}"
EXPECTED_STATE_SHA256="${EMU_RUNTIME_OOT_TITLE_STATE_SHA256:-c56658a8bdf738fec5e275d1a932366b538d5ed4ac14dd13d68a9ed10922afe0}"

if [[ "${EMU_ENABLE_RUNTIME_CONFORMANCE:-0}" != "1" ]]; then
  echo "SKIP: set EMU_ENABLE_RUNTIME_CONFORMANCE=1 to run OoT hi-res title fixture conformance."
  exit 77
fi

if [[ ! -f "$CACHE_PATH" ]]; then
  echo "SKIP: OoT PHRB package not found at $CACHE_PATH (set EMU_RUNTIME_OOT_PHRB to override)."
  exit 77
fi

if [[ ! -f "$ROM_PATH" ]]; then
  echo "SKIP: OoT ROM not found at $ROM_PATH (set EMU_RUNTIME_OOT_ROM to override)."
  exit 77
fi

if [[ ! -f "$STATE_PATH" ]]; then
  echo "SKIP: OoT title savestate not found at $STATE_PATH (set EMU_RUNTIME_OOT_TITLE_STATE or remint with tools/scenarios/remint-cross-game-boot-state.sh)."
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
  --game-id oot-title-boot
  --rom "$ROM_PATH"
  --cache-path "$CACHE_PATH"
  --state-path "$STATE_PATH"
  --expected-state-sha256 "$EXPECTED_STATE_SHA256"
  --min-entries 40000
  --min-compat-draw-hits 1
  --min-ci-attempts 1
  --min-ci-hits 1
  --ci-compat
  --expected-entry-class compat-only
)
if [[ -n "$BUNDLE_DIR" ]]; then
  args+=(--bundle-dir "$BUNDLE_DIR")
fi

CORE_PATH="$CORE_PATH" \
RETROARCH_BIN="$RETROARCH_BIN" \
RETROARCH_BASE_CONFIG="$RETROARCH_BASE_CONFIG" \
  "$REPO_ROOT/tools/scenarios/cross-game-hires-savestate-fixture-validation.sh" "${args[@]}"

echo "emu_conformance_oot_hires_title_fixture: PASS ($CACHE_PATH)"
