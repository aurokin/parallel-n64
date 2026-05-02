#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "$SCRIPT_DIR/../.." && pwd)"
source "$SCRIPT_DIR/lib/common.sh"

FIXTURE_ID="paper-mario-title-block-family-probe"
MANIFEST="$REPO_ROOT/tools/fixtures/paper-mario-title-block-family-probe.yaml"
MODE="on"
DRY_RUN=1
BUNDLE_DIR=""
SOURCE_BUNDLE="${SOURCE_BUNDLE:-$REPO_ROOT/artifacts/paper-mario-title-screen/on/20260328-title-sampled-probe-v4}"
RUNTIME_ENV="${RUNTIME_ENV_OVERRIDE:-$SCRIPT_DIR/paper-mario-title-screen.runtime.env}"

usage() {
  cat <<'EOF'
Usage:
  tools/scenarios/paper-mario-title-block-family-probe.sh [options]

Options:
  --mode off|on            Scenario mode label (default: on)
  --source-bundle PATH     Source bundle used to derive the 2048x1 title probe plan
  --bundle-dir PATH        Output bundle directory
  --run                    Execute the live probe
  -h, --help               Show this help
EOF
}

while (($#)); do
  case "$1" in
    --mode)
      shift
      MODE="${1:-}"
      if [[ "$MODE" != "off" && "$MODE" != "on" ]]; then
        echo "--mode must be 'off' or 'on'." >&2
        exit 2
      fi
      ;;
    --source-bundle)
      shift
      SOURCE_BUNDLE="${1:-}"
      ;;
    --bundle-dir)
      shift
      BUNDLE_DIR="${1:-}"
      ;;
    --run)
      DRY_RUN=0
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "Unknown option: $1" >&2
      usage >&2
      exit 2
      ;;
  esac
  shift
done

if [[ -z "$BUNDLE_DIR" ]]; then
  BUNDLE_DIR="$(scenario_default_bundle_dir "$REPO_ROOT" "$FIXTURE_ID" "$MODE")"
fi

scenario_prepare_bundle_dirs "$BUNDLE_DIR"
scenario_print_header "$FIXTURE_ID" "$MODE" "$BUNDLE_DIR" "$MANIFEST"

PLAN_JSON="$BUNDLE_DIR/traces/hires-block-family-plan.json"
REPORT_JSON="$BUNDLE_DIR/traces/hires-block-family-report.json"
REPORT_MD="$BUNDLE_DIR/traces/hires-block-family-report.md"

python3 "$REPO_ROOT/tools/hires_block_family_probe.py" plan   --source-bundle "$SOURCE_BUNDLE"   --mode block   --outcome miss   --formatsize 515   --width 2048   --height 1   --tile 7   --output "$PLAN_JSON"

PROBE_MIN_ADDR="$(python3 - <<'PY2' "$PLAN_JSON"
import json, sys
plan = json.load(open(sys.argv[1]))
print(plan["snapshot"]["min_addr"])
PY2
)"
PROBE_SPAN_BYTES="$(python3 - <<'PY2' "$PLAN_JSON"
import json, sys
plan = json.load(open(sys.argv[1]))
print(plan["snapshot"]["span_bytes"])
PY2
)"

ROM_PATH="$REPO_ROOT/assets/Paper Mario (USA).zip"
ANALYSIS_CACHE_PATH="${PARALLEL_RDP_HIRES_CACHE_PATH:-$(scenario_default_paper_mario_hires_cache "$REPO_ROOT")}"
PACK_PATH="$ANALYSIS_CACHE_PATH"
PACK_SHA256="$(scenario_sha256_file "$PACK_PATH")"
if [[ "$MODE" == "off" ]]; then
  PACK_PATH=""
  PACK_SHA256="missing"
fi
RETROARCH_PATH="/home/auro/code/RetroArch"
AUTHORITATIVE_STATE_PATH=""
AUTHORITATIVE_STATE_SHA256="missing"
EXPECTED_SCREENSHOT_SHA256=""

cat > "$BUNDLE_DIR/bundle.json" <<EOF
{
  "fixture_id": "$FIXTURE_ID",
  "mode": "$MODE",
  "manifest_path": "$MANIFEST",
  "bundle_dir": "$BUNDLE_DIR",
  "created_at": "$(date -Iseconds)",
  "probe_source_bundle": "$SOURCE_BUNDLE",
  "runtime_rules": {
    "internal_scale": "4x",
    "serial_execution": true,
    "display_required": true
  },
  "inputs": {
    "rom_path": "$ROM_PATH",
    "rom_sha256": "$(scenario_sha256_file "$ROM_PATH")",
    "hires_pack_path": "$PACK_PATH",
    "hires_pack_sha256": "$PACK_SHA256",
    "retroarch_path": "$RETROARCH_PATH"
  },
  "probe": {
    "plan_path": "$PLAN_JSON",
    "snapshot_min_addr": "$PROBE_MIN_ADDR",
    "snapshot_span_bytes": $PROBE_SPAN_BYTES
  },
  "status": {
    "scenario_state": "bundle_initialized",
    "runtime_executed": false
  }
}
EOF

cat > "$BUNDLE_DIR/config.env" <<EOF
FIXTURE_ID=$FIXTURE_ID
MODE=$MODE
ROM_PATH=$ROM_PATH
HIRES_PACK_PATH=$PACK_PATH
RETROARCH_PATH=$RETROARCH_PATH
EOF

if (( DRY_RUN )); then
  echo "[scenario] dry-run complete; runtime launch is intentionally deferred."
  exit 0
fi

scenario_source_runtime_env "$RUNTIME_ENV"

ANALYSIS_CACHE_PATH="${PARALLEL_RDP_HIRES_CACHE_PATH:-$ANALYSIS_CACHE_PATH}"
if [[ "$MODE" == "on" ]]; then
  PACK_PATH="$ANALYSIS_CACHE_PATH"
  scenario_require_phrb_runtime_cache "$PACK_PATH"
  PARALLEL_RDP_HIRES_SAMPLED_OBJECT_LOOKUP=1
  PARALLEL_RDP_HIRES_SAMPLED_OBJECT_PROBE=1
  export PARALLEL_RDP_HIRES_SAMPLED_OBJECT_LOOKUP PARALLEL_RDP_HIRES_SAMPLED_OBJECT_PROBE
  PACK_SHA256="$(scenario_sha256_file "$PACK_PATH")"
else
  PACK_PATH=""
  PACK_SHA256="missing"
fi
scenario_patch_file "$BUNDLE_DIR/bundle.json" 's|"hires_pack_path": "[^"]*"|"hires_pack_path": "'"${PACK_PATH}"'"|g; s|"hires_pack_sha256": "[^"]*"|"hires_pack_sha256": "'"${PACK_SHA256}"'"|g'
scenario_patch_file "$BUNDLE_DIR/config.env" 's|HIRES_PACK_PATH=.*|HIRES_PACK_PATH='"${PACK_PATH}"'|g'

if [[ ! -f "${AUTHORITATIVE_STATE_PATH:-}" ]]; then
  echo "[scenario] authoritative title state is required." >&2
  exit 1
fi

AUTHORITATIVE_STATE_SHA256="$(scenario_sha256_file "$AUTHORITATIVE_STATE_PATH")"
VERIFY_SCREENSHOT_SHA256=""
if [[ "$MODE" == "off" ]]; then
  VERIFY_SCREENSHOT_SHA256="${EXPECTED_SCREENSHOT_SHA256_OFF:-${EXPECTED_SCREENSHOT_SHA256:-}}"
fi

mkdir -p "$BUNDLE_DIR/states/ParaLLEl N64"
cp "$AUTHORITATIVE_STATE_PATH" "$BUNDLE_DIR/states/ParaLLEl N64/Paper Mario (USA).state"

scenario_patch_file "$BUNDLE_DIR/bundle.json" 's|"scenario_state": "bundle_initialized"|"scenario_state": "runtime_prepared"|g'

ADAPTER_ARGS=(
  --bundle-dir "$BUNDLE_DIR"
  --mode "$MODE"
  --retroarch-bin "$RETROARCH_BIN"
  --base-config "$RETROARCH_BASE_CONFIG"
  --core "$CORE_PATH"
  --rom "$ROM_PATH"
  --startup-wait "$STARTUP_WAIT"
  --command "WAIT_COMMAND_READY 120"
  --command "LOAD_STATE_SLOT_PAUSED 0"
  --command "STEP_FRAME ${POST_LOAD_SETTLE_FRAMES}"
  --command "WAIT_STATUS_FRAME PAUSED ${POST_LOAD_SETTLE_FRAMES} 10"
  --command "SNAPSHOT_CORE_MEMORY paper-mario-gamestatus 800740aa 230"
  --command "SNAPSHOT_CORE_MEMORY paper-mario-curgamemode 80151700 20"
  --command "SNAPSHOT_CORE_MEMORY paper-mario-transition 800a0944 8"
  --command "SNAPSHOT_CORE_MEMORY paper-mario-title-block-family-span ${PROBE_MIN_ADDR} ${PROBE_SPAN_BYTES}"
  --command "SCREENSHOT"
  --command "WAIT_NEW_CAPTURE 10"
  --command "QUIT"
)

HIRES_ENV_UNSET=(
  -u RUNTIME_ENV_OVERRIDE
  -u PARALLEL_RDP_HIRES_BLOCK_SHAPE_PROBE
  -u PARALLEL_RDP_HIRES_CACHE_PATH
  -u PARALLEL_RDP_HIRES_CI_COMPAT
  -u PARALLEL_RDP_HIRES_CI_LOW32_FALLBACK
  -u PARALLEL_RDP_HIRES_CI_PALETTE_PROBE
  -u PARALLEL_RDP_HIRES_CI_SELECT
  -u PARALLEL_RDP_HIRES_DEBUG
  -u PARALLEL_RDP_HIRES_FILTER_ALLOW_BLOCK
  -u PARALLEL_RDP_HIRES_FILTER_ALLOW_TILE
  -u PARALLEL_RDP_HIRES_FILTER_SIGNATURES
  -u PARALLEL_RDP_HIRES_GLIDEN64_COMPAT_CRC
  -u PARALLEL_RDP_HIRES_GPU_BUDGET_MB
  -u PARALLEL_RDP_HIRES_PHRB_DEBUG
  -u PARALLEL_RDP_HIRES_SAMPLED_OBJECT_LOOKUP
  -u PARALLEL_RDP_HIRES_SAMPLED_OBJECT_PROBE
  -u HIRES_FILTER_ALLOW_TILE
  -u HIRES_FILTER_ALLOW_BLOCK
  -u HIRES_FILTER_SIGNATURES
)

if [[ "$MODE" == "on" ]]; then
  PARALLEL_N64_GFX_PLUGIN_OVERRIDE="parallel" \
  PARALLEL_RDP_HIRES_CACHE_PATH="$PACK_PATH" \
  PARALLEL_RDP_HIRES_DEBUG=1 \
  PARALLEL_RDP_HIRES_SAMPLED_OBJECT_LOOKUP=1 \
  PARALLEL_RDP_HIRES_SAMPLED_OBJECT_PROBE=1 \
  "$REPO_ROOT/tools/adapters/retroarch_stdin_session.sh" "${ADAPTER_ARGS[@]}"
else
  env "${HIRES_ENV_UNSET[@]}" \
  PARALLEL_N64_GFX_PLUGIN_OVERRIDE="parallel" \
  "$REPO_ROOT/tools/adapters/retroarch_stdin_session.sh" "${ADAPTER_ARGS[@]}"
fi
scenario_assert_adapter_runtime_success "$BUNDLE_DIR"

if [[ -f "$BUNDLE_DIR/traces/paper-mario-gamestatus.core-memory.txt" ]]; then
  scenario_decode_paper_mario_semantic_state     "$BUNDLE_DIR"     "$BUNDLE_DIR/traces/paper-mario-game-status.json"
fi

scenario_extract_hires_log_evidence   "$BUNDLE_DIR"   "$BUNDLE_DIR/traces/hires-evidence.json"

scenario_verify_paper_mario_fixture   "$BUNDLE_DIR"   "$BUNDLE_DIR/traces/fixture-verification.json"   "$FIXTURE_ID"   "$VERIFY_SCREENSHOT_SHA256"   "${EXPECTED_INIT_SYMBOL:-}"   "${EXPECTED_STEP_SYMBOL:-}"

python3 "$REPO_ROOT/tools/hires_block_family_probe.py" analyze   --plan "$PLAN_JSON"   --snapshot-trace "$BUNDLE_DIR/traces/paper-mario-title-block-family-span.core-memory.txt"   --cache "$ANALYSIS_CACHE_PATH"   --output-json "$REPORT_JSON"   --output-markdown "$REPORT_MD"

echo "[scenario] title block-family probe complete: $REPORT_MD"
