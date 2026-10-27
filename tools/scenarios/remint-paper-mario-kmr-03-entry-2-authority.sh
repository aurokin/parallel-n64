#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "$SCRIPT_DIR/../.." && pwd)"
source "$SCRIPT_DIR/lib/common.sh"
# Pins always come from the tracked fixture profile. RUNTIME_ENV_OVERRIDE may
# change frontend inputs for the verification run, but not these identities.
FIXTURE_RUNTIME_ENV="$SCRIPT_DIR/paper-mario-kmr-03-entry-2.runtime.env"
RUNTIME_ENV="${RUNTIME_ENV_OVERRIDE:-$FIXTURE_RUNTIME_ENV}"

SOURCE_STATE_PATH="${AUTHORITY_SOURCE_STATE_PATH:-}"
OUTPUT_PATH="$REPO_ROOT/assets/states/paper-mario-kmr-03-entry-2/ParaLLEish/Paper Mario (USA).state"
BUNDLE_ROOT=""

usage() {
  cat <<'EOF'
Usage:
  tools/scenarios/remint-paper-mario-kmr-03-entry-2-authority.sh --source-state PATH [options]

Options:
  --source-state PATH Imported savestate to stage (or AUTHORITY_SOURCE_STATE_PATH)
  --output-path PATH  Write the verified authoritative state here
  --bundle-root PATH  Directory for staging and verification evidence
  -h, --help          Show this help

Notes:
  - Imports the `landing-control` checkpoint of a qualified Linux progression
    recording as the Paper Mario `kmr_03 ENTRY_2` authority.
  - The staged state must match the fixture's pinned M64+SAVE body sha256;
    RetroArch RZIP/RASTATE containers are accepted.
  - Verification loads it paused, settles 3 frames and captures feature-off,
    then requires the qualified kmr_03/ENTRY_2 scene and the pinned capture
    digest before atomically publishing the staged bytes.
  - While the fixture digest is still pending, the helper reports the observed
    digest and refuses publication.
EOF
}

while (($#)); do
  case "$1" in
    --source-state)
      shift
      SOURCE_STATE_PATH="${1:-}"
      if [[ -z "$SOURCE_STATE_PATH" ]]; then
        echo "--source-state requires a value." >&2
        exit 2
      fi
      ;;
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

if [[ -z "$SOURCE_STATE_PATH" || ! -f "$SOURCE_STATE_PATH" ]]; then
  echo "[remint] source state not found: ${SOURCE_STATE_PATH:-unset} (use --source-state or AUTHORITY_SOURCE_STATE_PATH)." >&2
  exit 2
fi

mapfile -t pins < <(
  # shellcheck disable=SC1090
  source "$FIXTURE_RUNTIME_ENV"
  printf '%s\n' "${AUTHORITY_SOURCE_STATE_BODY_SHA256:-}" "${EXPECTED_SCREENSHOT_SHA256_OFF:-}" \
    "${EXPECTED_SCENE_AREA_ID:-}" "${EXPECTED_SCENE_MAP_NAME:-}" "${EXPECTED_SCENE_ENTRY_ID:-}"
)
EXPECTED_SOURCE_BODY_SHA256="${pins[0]:-}"
EXPECTED_SCREENSHOT_SHA256="${pins[1]:-}"
if [[ -z "$EXPECTED_SOURCE_BODY_SHA256" || -z "${pins[2]:-}" || -z "${pins[3]:-}" || -z "${pins[4]:-}" ]]; then
  echo "[remint] $FIXTURE_RUNTIME_ENV must pin the source state body and scene." >&2
  exit 2
fi

if [[ -z "$BUNDLE_ROOT" ]]; then
  timestamp="$(date +"%Y%m%d-%H%M%S")"
  BUNDLE_ROOT="$REPO_ROOT/artifacts/paper-mario-kmr-03-entry-2/remint/$timestamp"
fi

STAGE_DIR="$BUNDLE_ROOT/stage"
VERIFY_BUNDLE="$BUNDLE_ROOT/verify"
CANDIDATE_STATE_PATH="$STAGE_DIR/authority-candidate.state"

echo "[remint] source state: $SOURCE_STATE_PATH"
echo "[remint] verify bundle: $VERIFY_BUNDLE"
echo "[remint] output path: $OUTPUT_PATH"

# Stage an immutable copy, then check its container-independent body identity.
# Exclusive creation prevents a reused bundle from rewriting prior evidence.
mkdir -p "$STAGE_DIR"
python3 - "$SOURCE_STATE_PATH" "$CANDIDATE_STATE_PATH" "$EXPECTED_SOURCE_BODY_SHA256" "$REPO_ROOT/tools" <<'PY'
import hashlib
from pathlib import Path
import shutil
import sys

source, candidate = map(Path, sys.argv[1:3])
expected_body_sha256 = sys.argv[3]
sys.path.insert(0, sys.argv[4])
from savestate_bridge import MAGIC, BridgeError, unwrap

with source.open('rb') as incoming, candidate.open('xb') as outgoing:
    shutil.copyfileobj(incoming, outgoing)
candidate.chmod(0o444)
try:
    body = unwrap(candidate.read_bytes())
except (BridgeError, ValueError, OSError) as exc:
    raise SystemExit(f'[remint] staged state cannot be unwrapped: {exc}')
if body[:len(MAGIC)] != MAGIC:
    raise SystemExit('[remint] staged state is not an M64+SAVE savestate.')
body_sha256 = hashlib.sha256(body).hexdigest()
print(f'[remint] staged state body sha256: {body_sha256}')
if body_sha256 != expected_body_sha256:
    raise SystemExit(
        f'[remint] staged state body sha256 mismatch: expected {expected_body_sha256}, got {body_sha256}.'
    )
PY
CANDIDATE_SHA256="$(scenario_sha256_file "$CANDIDATE_STATE_PATH")"
echo "[remint] staged state file sha256: $CANDIDATE_SHA256"

# The canonical scenario performs load paused -> settle 3 -> feature-off capture.
AUTHORITATIVE_STATE_PATH="$CANDIDATE_STATE_PATH" \
RUNTIME_ENV_OVERRIDE="$RUNTIME_ENV" \
"$SCRIPT_DIR/paper-mario-kmr-03-entry-2.sh" \
  --mode off \
  --authority-mode authoritative \
  --bundle-dir "$VERIFY_BUNDLE" \
  --run

# A custom runtime profile can override the caller's state path or relax scene
# expectations. Admit only a run of these bytes that decoded the pinned scene.
python3 - "$VERIFY_BUNDLE" "$CANDIDATE_SHA256" "${pins[2]}" "${pins[3]}" "${pins[4]}" <<'PY'
import json
from pathlib import Path
import sys

bundle = Path(sys.argv[1])
authority = json.loads((bundle / 'bundle.json').read_text()).get('fixture_authority', {})
if (authority.get('authority_mode_used') != 'authoritative'
        or authority.get('active_state_sha256') != sys.argv[2]):
    raise SystemExit('[remint] verification did not use the staged authoritative candidate.')
verification = json.loads((bundle / 'traces/fixture-verification.json').read_text())
checks = verification.get('checks') if isinstance(verification.get('checks'), dict) else {}
actual = verification.get('actual') if isinstance(verification.get('actual'), dict) else {}
scene = (actual.get('scene_area_id'), actual.get('scene_map_name'), actual.get('scene_entry_id'))
expected = (int(sys.argv[3]), sys.argv[4], int(sys.argv[5]))
if (verification.get('passed') is not True or checks.get('scene_decoder_qualified') is not True
        or scene != expected):
    raise SystemExit(f'[remint] verification did not qualify scene {expected}; decoded {scene}.')
PY

VERIFY_CAPTURE="$(find "$VERIFY_BUNDLE/captures" -maxdepth 1 -type f | head -n1)"
if [[ -z "$VERIFY_CAPTURE" ]]; then
  echo "[remint] verification capture missing." >&2
  exit 1
fi

VERIFY_SHA256="$(scenario_sha256_file "$VERIFY_CAPTURE")"
echo "[remint] verify capture sha256: $VERIFY_SHA256"

if [[ -z "$EXPECTED_SCREENSHOT_SHA256" ]]; then
  echo "[remint] feature-off capture digest is pending; authority not published." >&2
  echo "[remint] after reviewing $VERIFY_CAPTURE, record $VERIFY_SHA256 as" >&2
  echo "[remint] EXPECTED_SCREENSHOT_SHA256_OFF in $FIXTURE_RUNTIME_ENV and as the" >&2
  echo "[remint] graph node's expected_capture_sha256, then rerun to publish." >&2
  exit 3
fi
if [[ "$VERIFY_SHA256" != "$EXPECTED_SCREENSHOT_SHA256" ]]; then
  echo "[remint] verification capture hash mismatch." >&2
  echo "[remint] expected: $EXPECTED_SCREENSHOT_SHA256" >&2
  echo "[remint] actual:   $VERIFY_SHA256" >&2
  exit 1
fi

# Publish only the verified bytes. A temporary beside the destination permits
# one atomic replacement and preserves the old authority on any earlier error.
python3 - "$CANDIDATE_STATE_PATH" "$OUTPUT_PATH" "$CANDIDATE_SHA256" <<'PY'
import hashlib
import os
from pathlib import Path
import shutil
import stat
import sys
import tempfile

candidate, output = map(Path, sys.argv[1:3])
expected_sha256 = sys.argv[3]
output.parent.mkdir(parents=True, exist_ok=True)
descriptor, temporary_path = tempfile.mkstemp(prefix=f'.{output.name}.', dir=output.parent)
temporary = Path(temporary_path)
try:
    with os.fdopen(descriptor, 'wb') as outgoing, candidate.open('rb') as incoming:
        shutil.copyfileobj(incoming, outgoing)
    if hashlib.sha256(temporary.read_bytes()).hexdigest() != expected_sha256:
        raise SystemExit('[remint] candidate changed after staging; authority not published.')
    permissions = stat.S_IMODE(output.stat().st_mode) if output.exists() else 0o644
    temporary.chmod(permissions)
    os.replace(temporary, output)
finally:
    temporary.unlink(missing_ok=True)
PY

echo "[remint] authoritative state sha256: $CANDIDATE_SHA256"
echo "[remint] kmr_03 entry 2 authority verified."
