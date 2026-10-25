#!/usr/bin/env bash
# Run one public conformance-ROM suite (tools/conformance/test_roms.json) on
# every selected CPU engine and fail on any failure the manifest does not list.
#
# Usage: emu_conformance_test_roms.sh <suite>
#
# Inputs (environment):
#   EMU_ENABLE_RUNTIME_CONFORMANCE=1   opt in (otherwise skip)
#   EMU_CONFORMANCE_ROMS               staged ROMs (default assets/conformance-roms;
#                                      stage with tools/conformance/fetch_test_roms.py)
#   EMU_CONFORMANCE_CORE               core (default ./parallel_n64_libretro.so)
#   EMU_CONFORMANCE_CPUCORES           engines (default: all three)
#   EMU_CONFORMANCE_RUNTIME_CLASS      runtime class label (default local-4x)
#   EMU_CONFORMANCE_OUT                output root (default artifacts/conformance-roms)
#   RETROARCH_BIN, RETROARCH_BASE_CONFIG, RETROARCH_EXTRA_APPEND_CONFIG (optional)
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "$SCRIPT_DIR/../../.." && pwd)"
SUITE="${1:?usage: $0 <suite>}"

if [[ "${EMU_ENABLE_RUNTIME_CONFORMANCE:-0}" != "1" ]]; then
  echo "SKIP: set EMU_ENABLE_RUNTIME_CONFORMANCE=1 to run the $SUITE conformance ROMs."
  exit 77
fi

ROMS="${EMU_CONFORMANCE_ROMS:-$REPO_ROOT/assets/conformance-roms}"
CORE_PATH="${EMU_CONFORMANCE_CORE:-$REPO_ROOT/parallel_n64_libretro.so}"
CPUCORES="${EMU_CONFORMANCE_CPUCORES:-dynamic_recompiler cached_interpreter pure_interpreter}"
RUNTIME_CLASS="${EMU_CONFORMANCE_RUNTIME_CLASS:-local-4x}"
OUT_ROOT="${EMU_CONFORMANCE_OUT:-$REPO_ROOT/artifacts/conformance-roms}"
RETROARCH_BIN="${RETROARCH_BIN:-$(command -v retroarch || true)}"
RETROARCH_BASE_CONFIG="${RETROARCH_BASE_CONFIG:-}"

# A wholly unstaged suite is an opt-out and skips; once anything is staged,
# missing runtime inputs are failures.
if [[ -z "$(find "$ROMS/$SUITE" -type f -print -quit 2>/dev/null)" ]]; then
  echo "SKIP: no $SUITE ROMs staged under $ROMS/$SUITE (stage with tools/conformance/fetch_test_roms.py)."
  exit 77
fi
if [[ ! -f "$CORE_PATH" ]]; then
  echo "FAIL: libretro core not found at $CORE_PATH (build it or set EMU_CONFORMANCE_CORE)." >&2
  exit 1
fi
if [[ ! -x "$RETROARCH_BIN" ]]; then
  echo "FAIL: RetroArch binary not found (set RETROARCH_BIN)." >&2
  exit 1
fi
if [[ ! -f "$RETROARCH_BASE_CONFIG" ]]; then
  echo "FAIL: RetroArch base config not found at '$RETROARCH_BASE_CONFIG' (set RETROARCH_BASE_CONFIG)." >&2
  exit 1
fi

extra=()
if [[ -n "${RETROARCH_EXTRA_APPEND_CONFIG:-}" ]]; then
  extra+=(--extra-append-config "$RETROARCH_EXTRA_APPEND_CONFIG")
fi

out="$OUT_ROOT/$SUITE-$(date -u +%Y%m%dT%H%M%SZ)"
ran=0
failed=0
for cpucore in $CPUCORES; do
  set +e
  python3 "$REPO_ROOT/tools/conformance/run_test_roms.py" \
    --roms "$ROMS" --select "suite=$SUITE" --cpucore "$cpucore" \
    --core "$CORE_PATH" --retroarch-bin "$RETROARCH_BIN" --base-config "$RETROARCH_BASE_CONFIG" \
    "${extra[@]}" --runtime-class "$RUNTIME_CLASS" --out "$out/$cpucore"
  status=$?
  set -e
  case "$status" in
    0) ran=1 ;;
    77)
      echo "SKIP: no $SUITE ROM is staged under $ROMS (see tools/conformance/README.md)."
      exit 77
      ;;
    1) ran=1; failed=1 ;;
    *) echo "FAIL: conformance runner exited with status $status." >&2; exit 1 ;;
  esac
done

if ((failed)); then
  echo "FAIL: $SUITE conformance ROMs have unlisted failures; reports under $out." >&2
  exit 1
fi
((ran)) && echo "emu_conformance_test_roms: PASS ($SUITE; reports under $out)"
