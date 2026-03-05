#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "$SCRIPT_DIR/../../.." && pwd)"
RUNNER="$REPO_ROOT/run-n64.sh"

if [[ ! -f "$RUNNER" ]]; then
  echo "FAIL: missing run-n64.sh at $RUNNER" >&2
  exit 1
fi

require_pattern() {
  local pattern="$1"
  local message="$2"
  if ! rg -n --fixed-strings -- "$pattern" "$RUNNER" >/dev/null; then
    echo "FAIL: $message" >&2
    exit 1
  fi
}

# Usage/CLI surface.
require_pattern "run-n64.sh [options] [ROM_PATH] [-- RETROARCH_ARGS...]" \
  "usage text missing ROM/arg passthrough contract"
require_pattern "--reference         Use reference core build" \
  "usage text missing --reference option"
require_pattern "--core PATH         Use an explicit core path" \
  "usage text missing --core option"
require_pattern "--retroarch PATH    Use an explicit RetroArch binary path" \
  "usage text missing --retroarch option"
require_pattern "--rom-dir PATH      ROM base directory for relative ROM paths" \
  "usage text missing --rom-dir option"
require_pattern "--menu              Launch RetroArch menu without content" \
  "usage text missing --menu option"
require_pattern "--list-cores        Print discovered non-reference core builds" \
  "usage text missing --list-cores option"

# Defaults and selection behavior.
require_pattern 'DEFAULT_ROM_NAME="Paper Mario (USA).zip"' \
  "default ROM contract missing"
require_pattern 'REFERENCE_CORE="$SCRIPT_DIR/builds/parallel_n64_libretro.reference.so"' \
  "reference core path contract missing"
require_pattern "! -name '*reference*'" \
  "list-cores exclusion for reference builds missing"
require_pattern 'if [[ -n "$explicit_core" ]]; then' \
  "explicit core priority branch missing"
require_pattern 'elif (( use_reference )); then' \
  "reference core branch missing"
require_pattern 'if ! core_path="$(pick_latest_core)"; then' \
  "latest-core fallback branch missing"

# Runtime validation and error messaging.
require_pattern 'RetroArch binary not executable: $retroarch_bin' \
  "retroarch executable guard missing"
require_pattern 'No non-reference parallel core builds found.' \
  "missing latest-core error message"
require_pattern 'Core file not found: $core_path' \
  "missing core file error message"
require_pattern 'ROM not found: $rom_path' \
  "missing ROM error message"
require_pattern 'Unknown option: $1' \
  "unknown-option guard missing"

# Final launch handoff contract.
require_pattern 'cmd=("$retroarch_bin" -L "$core_path")' \
  "retroarch/core command wiring missing"
require_pattern 'cmd+=("${passthrough_args[@]}")' \
  "passthrough args handoff missing"
require_pattern 'echo "Using core: $core_path"' \
  "core selection summary output missing"
require_pattern 'exec "${cmd[@]}"' \
  "final exec handoff missing"

echo "emu_run_n64_contract: PASS"
