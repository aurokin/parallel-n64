#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "$SCRIPT_DIR/../../.." && pwd)"
SELF="$SCRIPT_DIR/$(basename "$0")"
HITS_FILE="$(mktemp)"
trap 'rm -f "$HITS_FILE"' EXIT

declare -a scan_files=(
  "$REPO_ROOT/AGENTS.md"
  "$REPO_ROOT/README.md"
  "$REPO_ROOT/docs/EMU_TESTING.md"
  "$REPO_ROOT/docs/PROJECT_STATE.md"
  "$REPO_ROOT/docs/WORKSPACE_PATHS.md"
  "$REPO_ROOT/docs/plans/PHASE_OVERVIEW.md"
  "$REPO_ROOT/docs/plans/hires_runtime_primary_plan.md"
  "$REPO_ROOT/PROJECT_NOTES.md"
  "$REPO_ROOT/run-tests.sh"
  "$REPO_ROOT/tests/emulator_behavior/CMakeLists.txt"
)

while IFS= read -r relpath; do
  path="$REPO_ROOT/$relpath"
  if [[ "$path" == "$SELF" ]]; then
    continue
  fi
  scan_files+=("$path")
done < <(
  cd "$REPO_ROOT"
  rg --files \
    tools/scenarios \
    tests/emulator_behavior/support \
    tools \
    | rg '\.(sh|py|env|md|json)$'
)

declare -a forbidden_fixed=(
  "EXPECTED_SCREENSHOT_SHA256_ON"
  "EMU_RUNTIME_PM64_SELECTED_TIMEOUT_ON_HASH"
  "EXPECTED_ON_HASH"
  "matches_off"
  "matches_legacy"
  "selected_hash"
  "legacy_hash"
  "on_hash"
  "off_hash"
  "lavapipe_frame_hash"
  "lavapipe_vi_filters_hash"
  "lavapipe_vi_filters_mixed_hash"
  "lavapipe_vi_downscale_hash"
  "lavapipe_sm64_frame_hash"
)

for pattern in "${forbidden_fixed[@]}"; do
  if rg -n --fixed-strings -- "$pattern" "${scan_files[@]}" >"$HITS_FILE"; then
    echo "FAIL: active runtime/test tooling must not reintroduce hi-res checksum correctness field '$pattern'." >&2
    cat "$HITS_FILE" >&2
    exit 1
  fi
done

declare -a policy_text_forbidden=(
  "byte-identical"
  "hash exactly"
  "strict title hash"
  "matches off exactly"
  "screenshots remain"
  "image remains unchanged"
  "image-remains-unchanged"
  "images remain unchanged"
  "images-remain-unchanged"
  "hash-neutral"
  "capture hash"
  "capture-hash"
  "screenshot hash"
  "screenshot-hash"
)

declare -a policy_scan_files=(
  "$REPO_ROOT/tools/hires_pack_transport_policy.json"
  "$REPO_ROOT/AGENTS.md"
  "$REPO_ROOT/README.md"
  "$REPO_ROOT/docs/README.md"
  "$REPO_ROOT/docs/EMU_TESTING.md"
  "$REPO_ROOT/docs/PROJECT_STATE.md"
  "$REPO_ROOT/docs/WORKSPACE_PATHS.md"
  "$REPO_ROOT/docs/PAPER_MARIO_RUNTIME_RESEARCH.md"
  "$REPO_ROOT/docs/plans/README.md"
  "$REPO_ROOT/docs/plans/PHASE_OVERVIEW.md"
  "$REPO_ROOT/docs/plans/hires_runtime_primary_plan.md"
  "$REPO_ROOT/tools/scenarios/README.md"
  "$REPO_ROOT/tools/scenarios/MODEL.md"
  "$REPO_ROOT/PROJECT_NOTES.md"
)

for pattern in "${policy_text_forbidden[@]}"; do
  if rg -n -i --fixed-strings -- "$pattern" "${policy_scan_files[@]}" >"$HITS_FILE"; then
    echo "FAIL: active policy/docs must not use checksum-shaped hi-res correctness rationale '$pattern'." >&2
    cat "$HITS_FILE" >&2
    exit 1
  fi
done

echo "emu_hires_checksum_gate_policy_contract: PASS"
