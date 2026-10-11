#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "$SCRIPT_DIR/../../.." && pwd)"
source "$REPO_ROOT/tools/scenarios/lib/common.sh"

TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT

write_capture_bundle() {
  local bundle_dir="$1"
  local mode="$2"
  mkdir -p "$bundle_dir/captures" "$bundle_dir/traces"
  python3 - "$bundle_dir/captures/capture.png" <<'PY'
from pathlib import Path
import sys
from PIL import Image

Image.new('RGB', (4, 4), (20, 30, 40)).save(Path(sys.argv[1]))
PY
  cat > "$bundle_dir/bundle.json" <<EOF
{"mode":"$mode"}
EOF
  cat > "$bundle_dir/traces/paper-mario-game-status.json" <<'EOF'
{
  "paper_mario_us": {
    "cur_game_mode": {
      "init_symbol": "state_init_title_screen",
      "step_symbol": "state_step_title_screen"
    }
  }
}
EOF
}

PASS_BUNDLE="$TMPDIR/pass-on"
write_capture_bundle "$PASS_BUNDLE" "on"
cat > "$PASS_BUNDLE/traces/hires-evidence.json" <<'EOF'
{
  "summary": {
    "provider": "on",
    "source_mode": "phrb-only",
    "entry_count": 66,
    "native_sampled_entry_count": 0,
    "compat_entry_count": 66,
    "compat_draw_hits": 4,
    "sampled_index_count": 0,
    "sampled_family_count": 0,
    "compat_low32_family_count": 1,
    "source_counts": {
      "phrb": 66
    }
  },
  "provenance": {
    "available": true,
    "source_class_counts": {"authored-rdram": 4},
    "provenance_class_counts": {"loadtile": 4}
  },
  "draw_usage": {
    "available": true,
    "draw_class_counts": {"texrect": 4}
  },
  "sampler_usage": {"available": false},
  "sampled_object_probe": {"available": false}
}
EOF
# Hi-res-on bundles never use screenshot digests; digest checks are
# reserved for feature-off baseline parity below.
(
  export EXPECTED_HIRES_SUMMARY_PROVIDER_ON="on"
  export EXPECTED_HIRES_SUMMARY_SOURCE_MODE_ON="phrb-only"
  export EXPECTED_HIRES_MIN_SUMMARY_ENTRY_COUNT_ON="1"
  export EXPECTED_HIRES_COMPAT_DRAW_HITS_PRESENT_ON="1"
  export EXPECTED_HIRES_MIN_SUMMARY_SOURCE_PHRB_COUNT_ON="1"
  export EXPECTED_HIRES_PROVENANCE_AVAILABLE_ON="1"
  export EXPECTED_HIRES_DRAW_USAGE_AVAILABLE_ON="1"
  export EXPECTED_HIRES_SOURCE_CLASS_PRESENT_ON="authored-rdram"
  export EXPECTED_HIRES_PROVENANCE_CLASS_PRESENT_ON="loadtile"
  export EXPECTED_HIRES_DRAW_CLASS_PRESENT_ON="texrect"
  scenario_verify_paper_mario_fixture \
    "$PASS_BUNDLE" \
    "$PASS_BUNDLE/verification.json" \
    "paper-mario-title-screen" \
    "" \
    "state_init_title_screen" \
    "state_step_title_screen"
)

# Image identity is insufficient when bytes cannot be decoded.
for corruption in nonimage truncated; do
  BAD_CAPTURE_BUNDLE="$TMPDIR/bad-capture-$corruption"
  cp -R "$PASS_BUNDLE" "$BAD_CAPTURE_BUNDLE"
  python3 - "$BAD_CAPTURE_BUNDLE/captures/capture.png" "$corruption" <<'PY'
from pathlib import Path
import sys

path = Path(sys.argv[1])
data = path.read_bytes()
path.write_bytes(b'not an image\n' if sys.argv[2] == 'nonimage' else data[:len(data) // 2])
PY
  if scenario_verify_paper_mario_fixture \
    "$BAD_CAPTURE_BUNDLE" "$BAD_CAPTURE_BUNDLE/verification.json" \
    "paper-mario-title-screen" "" "state_init_title_screen" "state_step_title_screen"; then
    echo "expected undecodable capture to fail ($corruption)" >&2
    exit 1
  fi
  python3 - "$BAD_CAPTURE_BUNDLE/verification.json" <<'PY'
import json
from pathlib import Path
import sys

result = json.loads(Path(sys.argv[1]).read_text())
assert result['checks']['capture_decodes'] is False, result
assert any('Capture could not be decoded' in failure for failure in result['failures']), result
PY
done

# World callbacks occur in many maps. The kmr03 authority additionally locks
# the decoded scene, while unrelated world fixtures may define their own scene.
WORLD_BUNDLE="$TMPDIR/world"
cp -R "$PASS_BUNDLE" "$WORLD_BUNDLE"
python3 - "$WORLD_BUNDLE/traces/paper-mario-game-status.json" <<'PY'
import json
from pathlib import Path
import sys

path = Path(sys.argv[1])
data = json.loads(path.read_text())
data['paper_mario_us']['cur_game_mode'].update(init_symbol='state_init_world', step_symbol='state_step_world')
data['paper_mario_us']['game_status'] = {'area_id': 0, 'map_name_candidate': 'kmr_03', 'entry_id': 5}
path.write_text(json.dumps(data))
PY
for scene in matching wrongmap wrongarea wrongentry missing; do
  SCENE_BUNDLE="$TMPDIR/scene-$scene"
  cp -R "$WORLD_BUNDLE" "$SCENE_BUNDLE"
  python3 - "$SCENE_BUNDLE/traces/paper-mario-game-status.json" "$scene" <<'PY'
import json
from pathlib import Path
import sys

path = Path(sys.argv[1])
data = json.loads(path.read_text())
scene = data['paper_mario_us']['game_status']
case = sys.argv[2]
if case == 'wrongmap':
    scene['map_name_candidate'] = 'kmr_02'
elif case == 'wrongarea':
    scene['area_id'] = 5
elif case == 'wrongentry':
    scene['entry_id'] = 0
elif case == 'missing':
    del data['paper_mario_us']['game_status']
path.write_text(json.dumps(data))
PY
  # These scene-only cases use the feature-off contract to isolate map checks
  # from the independently tested replacement-class requirements.
  printf '{"mode":"off"}\n' > "$SCENE_BUNDLE/bundle.json"
  status=0
  (
    scenario_source_runtime_env "$REPO_ROOT/tools/scenarios/paper-mario-kmr-03-entry-5.runtime.env"
    unset EXPECTED_HIRES_PROVENANCE_AVAILABLE_OFF EXPECTED_HIRES_DRAW_USAGE_AVAILABLE_OFF
    scenario_verify_paper_mario_fixture \
      "$SCENE_BUNDLE" "$SCENE_BUNDLE/verification.json" \
      "paper-mario-kmr-03-entry-5" "" "state_init_world" "state_step_world"
  ) || status=$?
  python3 - "$SCENE_BUNDLE/verification.json" "$scene" "$status" <<'PY'
import json
from pathlib import Path
import sys

result = json.loads(Path(sys.argv[1]).read_text())
matching = sys.argv[2] == 'matching'
assert (int(sys.argv[3]) == 0) == matching, result
assert result['checks']['scene_identity_match'] is matching, result
assert result['checks']['capture_decodes'] is True, result
if not matching:
    assert any('Scene identity mismatch' in failure for failure in result['failures']), result
PY
done

# Optional scene expectations do not constrain generic world fixtures.
scenario_verify_paper_mario_fixture \
  "$TMPDIR/scene-wrongmap" "$TMPDIR/scene-wrongmap/generic-verification.json" \
  "generic-world" "" "state_init_world" "state_step_world"

# A loaded package and upload hits do not prove draw-time replacement. Keep
# those present while falsifying only the active draw-hit evidence.
for activity in zero missing invalid; do
  NO_DRAW_BUNDLE="$TMPDIR/no-draw-$activity"
  cp -R "$PASS_BUNDLE" "$NO_DRAW_BUNDLE"
  python3 - "$NO_DRAW_BUNDLE/traces/hires-evidence.json" "$activity" <<'PY'
import json
from pathlib import Path
import sys

path = Path(sys.argv[1])
data = json.loads(path.read_text())
data['summary']['hits'] = 99
if sys.argv[2] == 'missing':
    del data['summary']['compat_draw_hits']
else:
    data['summary']['compat_draw_hits'] = 0 if sys.argv[2] == 'zero' else '4'
path.write_text(json.dumps(data))
PY
  if (
    set -a
    source "$REPO_ROOT/tools/scenarios/paper-mario-title-screen.runtime.env"
    set +a
    scenario_verify_paper_mario_fixture \
      "$NO_DRAW_BUNDLE" "$NO_DRAW_BUNDLE/verification.json" \
      "paper-mario-title-screen" "" "state_init_title_screen" "state_step_title_screen"
  ); then
    echo "expected missing draw activity to fail ($activity)" >&2
    exit 1
  fi
  python3 - "$NO_DRAW_BUNDLE/verification.json" <<'PY'
import json
from pathlib import Path
import sys

result = json.loads(Path(sys.argv[1]).read_text())
assert result['checks']['hires_compat_draw_hits_present_match'] is False, result
assert any('compat draw-hit presence' in failure for failure in result['failures']), result
PY
done

PASS_OFF_BUNDLE="$TMPDIR/pass-off"
write_capture_bundle "$PASS_OFF_BUNDLE" "off"
cat > "$PASS_OFF_BUNDLE/traces/hires-evidence.json" <<'EOF'
{
  "summary": null,
  "provenance": null,
  "draw_usage": null,
  "sampler_usage": null,
  "sampled_object_probe": null
}
EOF
PASS_OFF_HASH="$(scenario_sha256_file "$PASS_OFF_BUNDLE/captures/capture.png")"
(
  export EXPECTED_HIRES_PROVENANCE_AVAILABLE_OFF="0"
  export EXPECTED_HIRES_DRAW_USAGE_AVAILABLE_OFF="0"
  scenario_verify_paper_mario_fixture \
    "$PASS_OFF_BUNDLE" \
    "$PASS_OFF_BUNDLE/verification.json" \
    "paper-mario-title-screen" \
    "$PASS_OFF_HASH" \
    "state_init_title_screen" \
    "state_step_title_screen"
)

FAIL_BUNDLE="$TMPDIR/fail-on"
write_capture_bundle "$FAIL_BUNDLE" "on"
cat > "$FAIL_BUNDLE/traces/hires-evidence.json" <<'EOF'
{
  "summary": {"provider": "on"},
  "provenance": {
    "available": true,
    "source_class_counts": {"authored-rdram": 4},
    "provenance_class_counts": {"copy-cycle": 4}
  },
  "draw_usage": {
    "available": true,
    "draw_class_counts": {"texrect": 4}
  },
  "sampler_usage": {"available": false},
  "sampled_object_probe": {"available": false}
}
EOF
set +e
(
  export EXPECTED_HIRES_SUMMARY_PROVIDER_ON="on"
  export EXPECTED_HIRES_PROVENANCE_AVAILABLE_ON="1"
  export EXPECTED_HIRES_DRAW_USAGE_AVAILABLE_ON="1"
  export EXPECTED_HIRES_SOURCE_CLASS_PRESENT_ON="authored-rdram"
  export EXPECTED_HIRES_PROVENANCE_CLASS_PRESENT_ON="loadtile"
  export EXPECTED_HIRES_DRAW_CLASS_PRESENT_ON="texrect"
  scenario_verify_paper_mario_fixture \
    "$FAIL_BUNDLE" \
    "$FAIL_BUNDLE/verification.json" \
    "paper-mario-title-screen" \
    "" \
    "state_init_title_screen" \
    "state_step_title_screen"
)
status=$?
set -e
if [[ "$status" -eq 0 ]]; then
  echo "expected semantic hi-res verification failure, but check passed" >&2
  exit 1
fi
