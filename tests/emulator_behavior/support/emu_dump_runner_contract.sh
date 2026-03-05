#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "$SCRIPT_DIR/../../.." && pwd)"
RUNNER="$REPO_ROOT/run-dump-tests.sh"

if [[ ! -f "$RUNNER" ]]; then
  echo "FAIL: missing run-dump-tests.sh at $RUNNER" >&2
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

# Help/usage and maintained options.
require_pattern "--strict-composition Enable strict manifest composition gate" \
  "usage text missing --strict-composition option"
require_pattern "--required-tags CSV  Override required manifest tags" \
  "usage text missing --required-tags option"
require_pattern "./run-dump-tests.sh --strict-composition --required-tags smoke,sync" \
  "usage examples missing strict composition invocation"

# Required argument guards.
require_pattern "--validator requires a path." \
  "--validator missing empty-value guard"
require_pattern "--dump-dir requires a path." \
  "--dump-dir missing empty-value guard"
require_pattern "--capture-output requires a path." \
  "--capture-output missing empty-value guard"
require_pattern "--capture-rom requires a path." \
  "--capture-rom missing empty-value guard"
require_pattern "--capture-frames requires a value." \
  "--capture-frames missing empty-value guard"
require_pattern "--required-tags requires a CSV value." \
  "--required-tags missing empty-value guard"

# Environment export and defaulting behavior.
require_pattern 'export RDP_DUMP_CORPUS_DIR="$dump_dir"' \
  "corpus export wiring missing"
require_pattern 'if (( strict_composition )); then' \
  "strict composition guard missing"
require_pattern 'export RDP_DUMP_STRICT_COMPOSITION=1' \
  "strict composition env export missing"
require_pattern 'if [[ -n "$required_tags_csv" ]]; then' \
  "required-tags guard missing"
require_pattern 'export RDP_DUMP_REQUIRED_TAGS="$required_tags_csv"' \
  "required-tags env export missing"
require_pattern 'capture_output="${RDP_DUMP_CORPUS_DIR}/local/paper_mario_smoke.rdp"' \
  "default capture-output remap missing"

# Tool handoff behavior.
require_pattern 'RDP_VALIDATE_DUMP_BIN="$("$SCRIPT_DIR/tools/provision-rdp-validate-dump.sh")"' \
  "validator provisioning handoff missing"
require_pattern '"$SCRIPT_DIR/tools/capture-rdp-dump.sh" \' \
  "capture helper invocation missing"
require_pattern '"$SCRIPT_DIR/run-tests.sh" -R emu.dump "${passthrough_args[@]}"' \
  "run-tests handoff missing"

echo "emu_dump_runner_contract: PASS"
