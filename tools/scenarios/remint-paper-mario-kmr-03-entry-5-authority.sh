#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "$SCRIPT_DIR/../.." && pwd)"
source "$SCRIPT_DIR/lib/common.sh"
RUNTIME_ENV="${RUNTIME_ENV_OVERRIDE:-$SCRIPT_DIR/paper-mario-kmr-03-entry-5.runtime.env}"

EXPECTED_SCREENSHOT_SHA256="35213195f6ffca55458f76821198390880f135f4331efbca2cb139885b3ad879"
OUTPUT_PATH="$REPO_ROOT/assets/states/paper-mario-kmr-03-entry-5/ParaLLEl N64/Paper Mario (USA).state"
BUNDLE_ROOT=""

usage() {
  cat <<'EOF'
Usage:
  tools/scenarios/remint-paper-mario-kmr-03-entry-5-authority.sh [options]

Options:
  --output-path PATH  Write the reminted authoritative state here
  --bundle-root PATH  Directory for bootstrap/verify evidence bundles
  -h, --help          Show this help

Notes:
  - This script intentionally remints the authoritative Paper Mario `kmr_03 ENTRY_5` state.
  - The historical title -> settle 3 -> step 960 -> save bootstrap is scene-unqualified.
  - It verifies the staged candidate's intended scene and pinned feature-off capture before publishing.
EOF
}

while (($#)); do
  case "$1" in
    --output-path)
      shift
      OUTPUT_PATH="${1:-}"
      if [[ -z "$OUTPUT_PATH" ]]; then
        echo "--output-path requires a value." >&2
        exit 2
      fi
      ;;
    --bundle-root)
      shift
      BUNDLE_ROOT="${1:-}"
      if [[ -z "$BUNDLE_ROOT" ]]; then
        echo "--bundle-root requires a value." >&2
        exit 2
      fi
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

scenario_source_runtime_env "$RUNTIME_ENV"

if [[ -z "${BOOTSTRAP_STATE_PATH:-}" || ! -f "${BOOTSTRAP_STATE_PATH:-}" ]]; then
  echo "Bootstrap state not found: ${BOOTSTRAP_STATE_PATH:-missing}" >&2
  exit 1
fi

if [[ -z "$BUNDLE_ROOT" ]]; then
  timestamp="$(date +"%Y%m%d-%H%M%S")"
  BUNDLE_ROOT="$REPO_ROOT/artifacts/paper-mario-kmr-03-entry-5/remint/$timestamp"
fi

BOOTSTRAP_BUNDLE="$BUNDLE_ROOT/bootstrap"
VERIFY_BUNDLE="$BUNDLE_ROOT/verify"
BOOTSTRAP_STATE_COPY="$BOOTSTRAP_BUNDLE/states/ParaLLEl N64/Paper Mario (USA).state"
CANDIDATE_STATE_PATH="$BOOTSTRAP_BUNDLE/authority-candidate.state"

mkdir -p "$BOOTSTRAP_BUNDLE/states/ParaLLEl N64"
cp "$BOOTSTRAP_STATE_PATH" "$BOOTSTRAP_STATE_COPY"

echo "[remint] bootstrap bundle: $BOOTSTRAP_BUNDLE"
echo "[remint] verify bundle: $VERIFY_BUNDLE"
echo "[remint] output path: $OUTPUT_PATH"

declare -a ADAPTER_ARGS=(
  --bundle-dir "$BOOTSTRAP_BUNDLE"
  --mode off
  --retroarch-bin "$RETROARCH_BIN"
  --base-config "$RETROARCH_BASE_CONFIG"
  --core "$CORE_PATH"
  --rom "$ROM_PATH"
  --startup-wait "$STARTUP_WAIT"
  --command "WAIT_COMMAND_READY 120"
  --command "LOAD_STATE_SLOT_PAUSED 0"
  --command "STEP_FRAME ${POST_LOAD_SETTLE_FRAMES}"
  --command "WAIT_STATUS_FRAME PAUSED ${POST_LOAD_SETTLE_FRAMES} 10"
)

frame_cursor="$POST_LOAD_SETTLE_FRAMES"

append_chunked_step_commands() {
  local frame_count="$1"
  local chunk_limit="$2"
  local timeout_seconds="$3"
  local remaining="$frame_count"
  local chunk=""
  while (( remaining > 0 )); do
    if (( remaining > chunk_limit )); then
      chunk="$chunk_limit"
    else
      chunk="$remaining"
    fi
    frame_cursor=$(( frame_cursor + chunk ))
    ADAPTER_ARGS+=(--command "STEP_FRAME ${chunk}")
    ADAPTER_ARGS+=(--command "WAIT_STATUS_FRAME PAUSED ${frame_cursor} ${timeout_seconds}")
    remaining=$(( remaining - chunk ))
  done
}

append_chunked_step_commands "${TIMEOUT_STEP_FRAMES:-960}" "${TIMEOUT_STEP_CHUNK_FRAMES:-120}" 90

ADAPTER_ARGS+=(
  --command "SAVE_STATE"
  --command "WAIT_SAVE_STATE"
  --command "QUIT"
)

PARALLEL_N64_GFX_PLUGIN_OVERRIDE="parallel" \
"$REPO_ROOT/tools/adapters/retroarch_stdin_session.sh" \
  "${ADAPTER_ARGS[@]}"

# Keep the exact candidate independently of the mutable bootstrap save slot.
# Exclusive creation prevents a reused bundle from rewriting prior evidence.
python3 - "$BOOTSTRAP_STATE_COPY" "$CANDIDATE_STATE_PATH" <<'PY'
from pathlib import Path
import shutil
import sys

source, candidate = map(Path, sys.argv[1:])
with source.open('rb') as incoming, candidate.open('xb') as outgoing:
    shutil.copyfileobj(incoming, outgoing)
candidate.chmod(0o444)
PY
CANDIDATE_SHA256="$(scenario_sha256_file "$CANDIDATE_STATE_PATH")"

AUTHORITATIVE_STATE_PATH="$CANDIDATE_STATE_PATH" \
RUNTIME_ENV_OVERRIDE="$RUNTIME_ENV" \
"$SCRIPT_DIR/paper-mario-kmr-03-entry-5.sh" \
  --mode off \
  --authority-mode authoritative \
  --bundle-dir "$VERIFY_BUNDLE" \
  --run

# A custom runtime profile can override the caller's state path. Verify the
# canonical scenario selected these candidate bytes before admitting its result.
python3 - "$VERIFY_BUNDLE/bundle.json" "$CANDIDATE_SHA256" <<'PY'
import json
from pathlib import Path
import sys

authority = json.loads(Path(sys.argv[1]).read_text()).get('fixture_authority', {})
if (authority.get('authority_mode_used') != 'authoritative'
        or authority.get('active_state_sha256') != sys.argv[2]):
    raise SystemExit('[remint] verification did not use the staged authoritative candidate.')
PY

VERIFY_CAPTURE="$(find "$VERIFY_BUNDLE/captures" -maxdepth 1 -type f | head -n1)"
if [[ -z "$VERIFY_CAPTURE" ]]; then
  echo "[remint] verification capture missing." >&2
  exit 1
fi

VERIFY_SHA256="$(scenario_sha256_file "$VERIFY_CAPTURE")"

echo "[remint] candidate state sha256: $CANDIDATE_SHA256"
echo "[remint] verify capture sha256: $VERIFY_SHA256"

if [[ "$VERIFY_SHA256" != "$EXPECTED_SCREENSHOT_SHA256" ]]; then
  echo "[remint] verification capture hash mismatch." >&2
  echo "[remint] expected: $EXPECTED_SCREENSHOT_SHA256" >&2
  echo "[remint] actual:   $VERIFY_SHA256" >&2
  exit 1
fi

# Publish only the verified bytes. A temporary beside the destination permits
# one atomic replacement and preserves the old authority on any earlier error.
python3 - "$CANDIDATE_STATE_PATH" "$OUTPUT_PATH" "$CANDIDATE_SHA256" "$BOOTSTRAP_STATE_COPY" <<'PY'
import hashlib
import os
from pathlib import Path
import shutil
import stat
import sys
import tempfile

candidate, output = map(Path, sys.argv[1:3])
expected_sha256 = sys.argv[3]
bootstrap = Path(sys.argv[4])
output.parent.mkdir(parents=True, exist_ok=True)
descriptor, temporary_path = tempfile.mkstemp(prefix=f'.{output.name}.', dir=output.parent)
temporary = Path(temporary_path)
try:
    with os.fdopen(descriptor, 'wb') as outgoing, candidate.open('rb') as incoming:
        shutil.copyfileobj(incoming, outgoing)
    if hashlib.sha256(temporary.read_bytes()).hexdigest() != expected_sha256:
        raise SystemExit('[remint] candidate changed after staging; authority not published.')
    permissions = stat.S_IMODE((output if output.exists() else bootstrap).stat().st_mode)
    temporary.chmod(permissions)
    os.replace(temporary, output)
finally:
    temporary.unlink(missing_ok=True)
PY

echo "[remint] authoritative state sha256: $CANDIDATE_SHA256"
echo "[remint] kmr_03 entry 5 authority verified."
