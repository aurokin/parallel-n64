#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "$SCRIPT_DIR/../../.." && pwd)"
RUNNER="$REPO_ROOT/tools/scenarios/paper-mario-phrb-authority-validation.sh"
REFRESH_RUNNER="$REPO_ROOT/tests/emulator_behavior/support/emu_conformance_paper_mario_full_cache_phrb_authorities_refresh.sh"

if [[ ! -f "$RUNNER" ]]; then
  echo "FAIL: missing paper-mario-phrb-authority-validation.sh at $RUNNER" >&2
  exit 1
fi
if [[ ! -f "$REFRESH_RUNNER" ]]; then
  echo "FAIL: missing full-cache refresh conformance wrapper at $REFRESH_RUNNER" >&2
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

require_pattern 'source "$SCRIPT_DIR/lib/common.sh"' \
  "shared PHRB authority runner should source the shared scenario helpers"
require_pattern 'if ! scenario_require_phrb_runtime_cache "$CACHE_PATH"; then' \
  "shared PHRB authority runner should use the shared case-insensitive PHRB validator"
require_pattern 'f"- Expected source mode: `{expected_source_mode}`"' \
  "shared PHRB authority markdown summary should include expected source mode"
require_pattern 'if source_mode != expected_source_mode:' \
  "shared PHRB authority runner should revalidate source mode while summarizing reused bundles"
require_pattern 'native_sampled_entry_count_int < min_native_sampled_count' \
  "shared PHRB authority runner should revalidate native sampled floors while summarizing reused bundles"
require_pattern 'source_phrb_count_int < 1' \
  "shared PHRB authority runner should require PHRB-backed source evidence while summarizing reused bundles"
require_pattern 'f"- Descriptor detail: native checksum exact `' \
  "shared PHRB authority markdown summary should include native checksum descriptor detail"
require_pattern '("title-screen", "paper-mario-title-screen")' \
  "shared PHRB authority runner should keep the title-screen fixture"
require_pattern '("file-select", "paper-mario-file-select")' \
  "shared PHRB authority runner should keep the file-select fixture"
require_pattern '("kmr-03-entry-5", "paper-mario-kmr-03-entry-5")' \
  "shared PHRB authority runner should keep the kmr-03-entry-5 fixture"

for pattern in \
  'PROMOTED_CONTEXT_ROOT="${EMU_RUNTIME_PM64_PROMOTED_CONTEXT_ROOT:-}"' \
  'if [[ -n "$PROMOTED_CONTEXT_ROOT" ]]; then' \
  'PROMOTED_CONTEXT_ROOT="$REPO_ROOT/$PROMOTED_CONTEXT_ROOT"' \
  'PROMOTED_CONTEXT_ROOT="$REPO_ROOT/artifacts/paper-mario-probes/validation/$PROMOTED_CONTEXT_ROOT"' \
  'SKIP: promoted Paper Mario sampled-probe context root not found'; do
  if ! rg -n --fixed-strings -- "$pattern" "$REFRESH_RUNNER" >/dev/null; then
    echo "FAIL: full-cache refresh wrapper should honor explicit promoted context root override: missing $pattern" >&2
    exit 1
  fi
done
if rg -n --fixed-strings -- '*-selected-package-authorities' "$REFRESH_RUNNER" >/dev/null; then
  echo "FAIL: full-cache refresh wrapper must not auto-select the latest selected-package authority artifact." >&2
  exit 1
fi

echo "emu_paper_mario_phrb_authority_validation_contract: PASS"
