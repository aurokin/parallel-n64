#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "$SCRIPT_DIR/../.." && pwd)"
source "$SCRIPT_DIR/lib/common.sh"

CACHE_PATH="${PARALLEL_RDP_HIRES_CACHE_PATH:-}"
BUNDLE_ROOT=""
STEP_LIST="960 1200 1500"
MIN_NATIVE_SAMPLED_COUNT=195
RUN_PROBES=1
LOADER_MANIFEST_PATH=""
TRANSPORT_REVIEW_PATH=""
ALT_SOURCE_CACHE_PATH=""
CROSS_SCENE_GUARD_EVIDENCE=()
PACKAGE_MANIFEST_PATH=""
POOL_REGRESSION_SAMPLE_LOW32="1b8530fb"
POOL_REGRESSION_FLAT_SUMMARY=""
POOL_REGRESSION_DUAL_SUMMARY=""
POOL_REGRESSION_ORDERED_SUMMARY=""
POOL_REGRESSION_SURFACE_PACKAGE=""

usage() {
  cat <<'USAGE'
Usage:
  tools/scenarios/paper-mario-title-timeout-selected-package-validation.sh [options]

Options:
  --cache-path PATH     Selected PHRB package to validate (defaults to env PARALLEL_RDP_HIRES_CACHE_PATH)
  --bundle-root PATH    Root directory for emitted on/off probe bundles
  --loader-manifest PATH
                       Optional loader-manifest.json used to classify sampled families against the active package
  --transport-review PATH
                       Optional sampled transport review JSON used to classify whether legacy candidates exist
  --alternate-source-cache PATH
                       Optional legacy cache used to seed review-only alternate-source candidates
                       for candidate-free sampled families
  --package-manifest PATH
                       Optional selected package-manifest.json used to review duplicate families
  --cross-scene-guard-evidence LABEL=PATH
                       Optional guard-scene hires-evidence.json used to check whether
                       candidate-free families are still cross-scene shared before promotion.
                       Pass multiple times.
  --pool-regression-sampled-low32 HEX
                       sampled_low32 family for optional historical pool regression review
                       (default: 1b8530fb)
  --pool-regression-flat-summary PATH
                       Optional historical flat validation-summary.json for the pool regression review
  --pool-regression-dual-summary PATH
                       Optional historical flat+surface validation-summary.json for the pool regression review
  --pool-regression-ordered-summary PATH
                       Optional historical ordered-only validation-summary.json for the pool regression review
  --pool-regression-surface-package PATH
                       Optional historical surface-package.json for the pool regression review
  --steps "..."        Space-separated timeout checkpoints (default: "960 1200 1500")
  --min-native-sampled-count N
                       Minimum native sampled entries required in the on bundle (default: 195)
  --reuse               Reuse existing bundles instead of rerunning probes
  -h, --help            Show this help
USAGE
}

while (($#)); do
  case "$1" in
    --cache-path)
      shift
      CACHE_PATH="${1:-}"
      ;;
    --bundle-root)
      shift
      BUNDLE_ROOT="${1:-}"
      ;;
    --steps)
      shift
      STEP_LIST="${1:-}"
      ;;
    --min-native-sampled-count)
      shift
      MIN_NATIVE_SAMPLED_COUNT="${1:-}"
      ;;
    --loader-manifest)
      shift
      LOADER_MANIFEST_PATH="${1:-}"
      ;;
    --transport-review)
      shift
      TRANSPORT_REVIEW_PATH="${1:-}"
      ;;
    --alternate-source-cache)
      shift
      ALT_SOURCE_CACHE_PATH="${1:-}"
      ;;
    --package-manifest)
      shift
      PACKAGE_MANIFEST_PATH="${1:-}"
      ;;
    --cross-scene-guard-evidence)
      shift
      CROSS_SCENE_GUARD_EVIDENCE+=("${1:-}")
      ;;
    --pool-regression-sampled-low32)
      shift
      POOL_REGRESSION_SAMPLE_LOW32="${1:-}"
      ;;
    --pool-regression-flat-summary)
      shift
      POOL_REGRESSION_FLAT_SUMMARY="${1:-}"
      ;;
    --pool-regression-dual-summary)
      shift
      POOL_REGRESSION_DUAL_SUMMARY="${1:-}"
      ;;
    --pool-regression-ordered-summary)
      shift
      POOL_REGRESSION_ORDERED_SUMMARY="${1:-}"
      ;;
    --pool-regression-surface-package)
      shift
      POOL_REGRESSION_SURFACE_PACKAGE="${1:-}"
      ;;
    --reuse)
      RUN_PROBES=0
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

if [[ -z "$CACHE_PATH" ]]; then
  echo "--cache-path or PARALLEL_RDP_HIRES_CACHE_PATH is required." >&2
  exit 2
fi
if [[ ! -f "$CACHE_PATH" ]]; then
  echo "Selected package not found: $CACHE_PATH" >&2
  exit 2
fi
if ! scenario_require_phrb_runtime_cache "$CACHE_PATH"; then
  exit 2
fi
if [[ -n "$LOADER_MANIFEST_PATH" && ! -f "$LOADER_MANIFEST_PATH" ]]; then
  echo "Loader manifest not found: $LOADER_MANIFEST_PATH" >&2
  exit 2
fi
if [[ -n "$TRANSPORT_REVIEW_PATH" && ! -f "$TRANSPORT_REVIEW_PATH" ]]; then
  echo "Transport review not found: $TRANSPORT_REVIEW_PATH" >&2
  exit 2
fi
if [[ -n "$ALT_SOURCE_CACHE_PATH" && ! -f "$ALT_SOURCE_CACHE_PATH" ]]; then
  echo "Alternate-source cache not found: $ALT_SOURCE_CACHE_PATH" >&2
  exit 2
fi
if [[ -n "$PACKAGE_MANIFEST_PATH" && ! -f "$PACKAGE_MANIFEST_PATH" ]]; then
  echo "Package manifest not found: $PACKAGE_MANIFEST_PATH" >&2
  exit 2
fi
for required_path in \
  "$POOL_REGRESSION_FLAT_SUMMARY" \
  "$POOL_REGRESSION_DUAL_SUMMARY" \
  "$POOL_REGRESSION_ORDERED_SUMMARY" \
  "$POOL_REGRESSION_SURFACE_PACKAGE"; do
  if [[ -n "$required_path" && ! -f "$required_path" ]]; then
    echo "Pool regression input not found: $required_path" >&2
    exit 2
  fi
done
for labeled_path in "${CROSS_SCENE_GUARD_EVIDENCE[@]}"; do
  if [[ "$labeled_path" != *=* ]]; then
    echo "Expected LABEL=PATH for --cross-scene-guard-evidence, got: $labeled_path" >&2
    exit 2
  fi
  guard_path="${labeled_path#*=}"
  if [[ ! -f "$guard_path" ]]; then
    echo "Cross-scene guard evidence not found: $guard_path" >&2
    exit 2
  fi
done
if [[ -z "$BUNDLE_ROOT" ]]; then
  BUNDLE_ROOT="$REPO_ROOT/artifacts/paper-mario-probes/validation/$(date +"%Y%m%d-%H%M%S")-title-timeout-selected-package"
fi
mkdir -p "$BUNDLE_ROOT/on" "$BUNDLE_ROOT/off"

HIRES_ENV_UNSET=(
  -u RUNTIME_ENV_OVERRIDE
  -u PARALLEL_N64_GFX_PLUGIN_OVERRIDE
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

for step in $STEP_LIST; do
  on_bundle="$BUNDLE_ROOT/on/timeout-${step}"
  off_bundle="$BUNDLE_ROOT/off/timeout-${step}"
  if (( RUN_PROBES )); then
    env "${HIRES_ENV_UNSET[@]}" \
    DISABLE_SCREENSHOT_VERIFY=1 \
    "$SCRIPT_DIR/paper-mario-title-timeout-probe.sh" \
      --mode off \
      --step-frames "$step" \
      --step-chunk-frames "$step" \
      --probe-label "timeout-${step}-off-baseline" \
      --bundle-dir "$off_bundle" \
      --run

    env "${HIRES_ENV_UNSET[@]}" \
    PARALLEL_RDP_HIRES_CACHE_PATH="$CACHE_PATH" \
    PARALLEL_RDP_HIRES_SAMPLED_OBJECT_LOOKUP=1 \
    DISABLE_SCREENSHOT_VERIFY=1 \
    "$SCRIPT_DIR/paper-mario-title-timeout-probe.sh" \
      --mode on \
      --step-frames "$step" \
      --step-chunk-frames "$step" \
      --probe-label "timeout-${step}-selected-package" \
      --bundle-dir "$on_bundle" \
      --run
  fi
  if [[ ! -d "$on_bundle" || ! -d "$off_bundle" ]]; then
    echo "Missing bundles for step $step under $BUNDLE_ROOT" >&2
    exit 1
  fi
  if [[ -n "$LOADER_MANIFEST_PATH" || -n "$TRANSPORT_REVIEW_PATH" ]]; then
    review_cmd=(
      python3
      "$REPO_ROOT/tools/hires_sampled_selector_review.py"
      --bundle-dir "$on_bundle"
      --output "$on_bundle/traces/hires-sampled-selector-review.md"
      --output-json "$on_bundle/traces/hires-sampled-selector-review.json"
    )
    if [[ -n "$LOADER_MANIFEST_PATH" ]]; then
      review_cmd+=(--loader-manifest "$LOADER_MANIFEST_PATH")
    fi
    if [[ -n "$TRANSPORT_REVIEW_PATH" ]]; then
      review_cmd+=(--transport-review "$TRANSPORT_REVIEW_PATH")
    fi
    "${review_cmd[@]}"

    if [[ -n "$TRANSPORT_REVIEW_PATH" && -f "$on_bundle/traces/hires-sampled-selector-review.json" ]]; then
      if python3 - "$on_bundle/traces/hires-sampled-selector-review.json" "$POOL_REGRESSION_SAMPLE_LOW32" <<'PY'
import json
import sys
from pathlib import Path

review = json.loads(Path(sys.argv[1]).read_text())
target = str(sys.argv[2]).lower()
for row in review.get("pool_families", []):
    if str(row.get("sampled_low32") or "").lower() == target:
        raise SystemExit(0)
raise SystemExit(1)
PY
      then
        python3 "$REPO_ROOT/tools/hires_sampled_pool_review.py" \
          --bundle-dir "$on_bundle" \
          --selector-review "$on_bundle/traces/hires-sampled-selector-review.json" \
          --transport-review "$TRANSPORT_REVIEW_PATH" \
          --sampled-low32 "$POOL_REGRESSION_SAMPLE_LOW32" \
          --allow-missing-draw-sequence \
          --output "$on_bundle/traces/hires-sampled-pool-review-${POOL_REGRESSION_SAMPLE_LOW32}.md" \
          --output-json "$on_bundle/traces/hires-sampled-pool-review-${POOL_REGRESSION_SAMPLE_LOW32}.json"
      fi
    fi

    if [[ -n "$TRANSPORT_REVIEW_PATH" && -n "$ALT_SOURCE_CACHE_PATH" && -f "$on_bundle/traces/hires-sampled-selector-review.json" ]]; then
      python3 "$REPO_ROOT/tools/hires_seed_alternate_source_review.py" \
        --review "$TRANSPORT_REVIEW_PATH" \
        --selector-review "$on_bundle/traces/hires-sampled-selector-review.json" \
        --cache "$ALT_SOURCE_CACHE_PATH" \
        --output-json "$on_bundle/traces/hires-alternate-source-review.json" \
        --output-markdown "$on_bundle/traces/hires-alternate-source-review.md"
    fi

    if [[ ${#CROSS_SCENE_GUARD_EVIDENCE[@]} -gt 0 && -f "$on_bundle/traces/hires-sampled-selector-review.json" ]]; then
      mapfile -t cross_scene_families < <(
        python3 - "$on_bundle/traces/hires-sampled-selector-review.json" <<'PY'
import json
import sys
from pathlib import Path

review = json.loads(Path(sys.argv[1]).read_text())
seen = set()
for row in review.get("unresolved", []):
    if row.get("package_status") != "absent-from-package":
        continue
    if row.get("transport_status") != "legacy-transport-candidate-free":
        continue
    sampled_low32 = str(row.get("sampled_low32") or "").lower()
    if not sampled_low32 or sampled_low32 in seen:
        continue
    seen.add(sampled_low32)
    print(sampled_low32)
PY
      )
      if [[ ${#cross_scene_families[@]} -gt 0 ]]; then
        cross_scene_cmd=(
          python3
          "$REPO_ROOT/tools/hires_sampled_cross_scene_review.py"
          --evidence "timeout=$on_bundle/traces/hires-evidence.json"
          --target-label timeout
          --output-json "$on_bundle/traces/hires-sampled-cross-scene-review.json"
          --output-markdown "$on_bundle/traces/hires-sampled-cross-scene-review.md"
        )
        for labeled_path in "${CROSS_SCENE_GUARD_EVIDENCE[@]}"; do
          cross_scene_cmd+=(--evidence "$labeled_path")
          cross_scene_cmd+=(--guard-label "${labeled_path%%=*}")
        done
        for sampled_low32 in "${cross_scene_families[@]}"; do
          cross_scene_cmd+=(--sampled-low32 "$sampled_low32")
        done
        "${cross_scene_cmd[@]}"
      fi
    fi

    if [[ -f "$on_bundle/traces/hires-alternate-source-review.json" && -f "$on_bundle/traces/hires-sampled-cross-scene-review.json" ]]; then
      python3 "$REPO_ROOT/tools/hires_alternate_source_activation_review.py" \
        --alternate-source-review "$on_bundle/traces/hires-alternate-source-review.json" \
        --cross-scene-review "$on_bundle/traces/hires-sampled-cross-scene-review.json" \
        --output-json "$on_bundle/traces/hires-alternate-source-activation-review.json" \
        --output-markdown "$on_bundle/traces/hires-alternate-source-activation-review.md"
    fi

    if [[ -f "$on_bundle/traces/hires-sampled-selector-review.json" ]]; then
      seam_cmd=(
        python3
        "$REPO_ROOT/tools/hires_runtime_seam_register.py"
        --bundle-dir "$on_bundle"
        --selector-review "$on_bundle/traces/hires-sampled-selector-review.json"
        --output "$on_bundle/traces/hires-runtime-seam-register.md"
        --output-json "$on_bundle/traces/hires-runtime-seam-register.json"
      )
      if [[ -f "$on_bundle/traces/hires-alternate-source-review.json" ]]; then
        seam_cmd+=(--alternate-source-review "$on_bundle/traces/hires-alternate-source-review.json")
      fi
      if [[ -f "$on_bundle/traces/hires-sampled-cross-scene-review.json" ]]; then
        seam_cmd+=(--cross-scene-review "$on_bundle/traces/hires-sampled-cross-scene-review.json")
      fi
      if [[ -f "$on_bundle/traces/hires-alternate-source-activation-review.json" ]]; then
        seam_cmd+=(--alternate-source-activation-review "$on_bundle/traces/hires-alternate-source-activation-review.json")
      fi
      "${seam_cmd[@]}"
    fi

    if [[ -n "$PACKAGE_MANIFEST_PATH" && -f "$on_bundle/traces/hires-runtime-seam-register.json" ]]; then
      while IFS=$'\t' read -r sampled_low32 selector; do
        [[ -n "$sampled_low32" ]] || continue
        python3 "$REPO_ROOT/tools/hires_sampled_duplicate_review.py" \
          --runtime-seam-register "$on_bundle/traces/hires-runtime-seam-register.json" \
          --package-manifest "$PACKAGE_MANIFEST_PATH" \
          --sampled-low32 "$sampled_low32" \
          --selector "$selector" \
          --output "$on_bundle/traces/hires-sampled-duplicate-review-${sampled_low32}.md" \
          --output-json "$on_bundle/traces/hires-sampled-duplicate-review-${sampled_low32}.json"
      done < <(
        python3 - "$on_bundle/traces/hires-runtime-seam-register.json" <<'PY'
import json
import sys
from pathlib import Path

register = json.loads(Path(sys.argv[1]).read_text())
for row in register.get("sampled_duplicate_families", []):
    sampled_low32 = str(row.get("sampled_low32") or "").lower()
    selector = str(row.get("selector") or "").lower()
    if not sampled_low32 or not selector:
        continue
    print(f"{sampled_low32}\t{selector}")
PY
      )
    fi

    if [[ -n "$POOL_REGRESSION_FLAT_SUMMARY" && -n "$POOL_REGRESSION_DUAL_SUMMARY" && -n "$POOL_REGRESSION_ORDERED_SUMMARY" && -n "$POOL_REGRESSION_SURFACE_PACKAGE" ]]; then
      pool_regression_review_json="$on_bundle/traces/hires-sampled-pool-regression-review-${POOL_REGRESSION_SAMPLE_LOW32}.json"
      pool_regression_review_md="$on_bundle/traces/hires-sampled-pool-regression-review-${POOL_REGRESSION_SAMPLE_LOW32}.md"
      live_pool_review_json="$on_bundle/traces/hires-sampled-pool-review-${POOL_REGRESSION_SAMPLE_LOW32}.json"
      if [[ -f "$live_pool_review_json" ]]; then
        python3 "$REPO_ROOT/tools/hires_pool_regression_review.py" \
          --sampled-low32 "$POOL_REGRESSION_SAMPLE_LOW32" \
          --flat-summary "$POOL_REGRESSION_FLAT_SUMMARY" \
          --dual-summary "$POOL_REGRESSION_DUAL_SUMMARY" \
          --ordered-summary "$POOL_REGRESSION_ORDERED_SUMMARY" \
          --surface-package "$POOL_REGRESSION_SURFACE_PACKAGE" \
          --live-pool-review "$live_pool_review_json" \
          --output "$pool_regression_review_md" \
          --output-json "$pool_regression_review_json"
      fi
    fi
  fi
done

EXPECTED_ROM_PATH="${PAPER_MARIO_EXPECTED_ROM_PATH:-$REPO_ROOT/assets/Paper Mario (USA).zip}"
MIN_NATIVE_SAMPLED_COUNT="$MIN_NATIVE_SAMPLED_COUNT" python3 - "$CACHE_PATH" "$BUNDLE_ROOT" "$EXPECTED_ROM_PATH" <<'PY'
import hashlib
import json
import math
import os
import sys
from pathlib import Path
from PIL import Image, ImageChops

cache_path = Path(sys.argv[1])
bundle_root = Path(sys.argv[2])
expected_rom_path = Path(sys.argv[3])
expected_rom_sha256 = hashlib.sha256(expected_rom_path.read_bytes()).hexdigest()
expected_cache_sha256 = hashlib.sha256(cache_path.read_bytes()).hexdigest()
summary = {
    'cache_path': str(cache_path),
    'cache_sha256': expected_cache_sha256,
    'passed': True,
    'all_passed': True,
    'steps': [],
}

def one_capture(bundle_dir: Path):
    captures = sorted((bundle_dir / 'captures').glob('*'))
    if len(captures) != 1:
        raise SystemExit(f'expected exactly one capture in {bundle_dir}/captures, found {len(captures)}')
    return captures[0]

def disabled_hires_evidence_ok(evidence):
    summary = evidence.get('summary') or {}
    return (
        evidence.get('available') is False
        and evidence.get('cache_loaded') is False
        and not evidence.get('cache_path')
        and not evidence.get('cache_sha256')
        and summary.get('provider') in (None, 'off')
        and int(summary.get('entry_count') or 0) == 0
        and int(summary.get('native_sampled_entry_count') or 0) == 0
        and int(summary.get('compat_entry_count') or 0) == 0
    )

def same_resolved_path(actual, expected):
    if actual in (None, ''):
        return False
    try:
        return Path(actual).resolve() == Path(expected).resolve()
    except OSError:
        return str(actual) == str(expected)

def read_env_file(path: Path):
    if not path.is_file():
        return None
    values = {}
    for line in path.read_text().splitlines():
        if not line or line.startswith('#') or '=' not in line:
            continue
        key, value = line.split('=', 1)
        values[key] = value
    return values

def sha256_file(path: Path):
    h = hashlib.sha256()
    with path.open('rb') as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b''):
            h.update(chunk)
    return h.hexdigest()

def command_log_signature(bundle_dir: Path):
    command_log = bundle_dir / 'retroarch.executed.commands.log'
    if not command_log.is_file():
        return None
    return hashlib.sha256(command_log.read_bytes()).hexdigest()

def expected_command_log_signature(bundle_dir: Path):
    command_log = bundle_dir / 'retroarch.expected.commands.log'
    if not command_log.is_file():
        return None
    return hashlib.sha256(command_log.read_bytes()).hexdigest()

def path_within(path: Path, root: Path) -> bool:
    try:
        path.resolve().relative_to(root.resolve())
        return True
    except ValueError:
        return False

def require_adapter_config_provenance(bundle_dir: Path, session):
    for key in ('BASE_CONFIG', 'BASE_CONFIG_SHA256', 'APPEND_CONFIG', 'APPEND_CONFIG_SHA256', 'CORE_OPTIONS_FILE', 'CORE_OPTIONS_FILE_SHA256'):
        if not session.get(key):
            raise SystemExit(f'expected adapter config provenance {key} in {bundle_dir}')
    base_config = Path(session['BASE_CONFIG'])
    append_config = Path(session['APPEND_CONFIG'])
    core_options = Path(session['CORE_OPTIONS_FILE'])
    if not base_config.is_file():
        raise SystemExit(f'expected adapter BASE_CONFIG to exist in {bundle_dir}: {base_config!s}')
    if sha256_file(base_config) != session['BASE_CONFIG_SHA256']:
        raise SystemExit(f'adapter BASE_CONFIG_SHA256 does not match current artifact in {bundle_dir}')
    for path, sha_key, label in (
        (append_config, 'APPEND_CONFIG_SHA256', 'append config'),
        (core_options, 'CORE_OPTIONS_FILE_SHA256', 'core options'),
    ):
        if not path.is_file():
            raise SystemExit(f'expected adapter {label} snapshot to exist in {bundle_dir}: {path!s}')
        if not path_within(path, bundle_dir):
            raise SystemExit(f'expected adapter {label} snapshot to be bundle-local in {bundle_dir}: {path!s}')
        if sha256_file(path) != session[sha_key]:
            raise SystemExit(f'adapter {sha_key} does not match current snapshot in {bundle_dir}')

def require_adapter_session_provenance(bundle_dir: Path, expected_mode: str, expected_cache_path: Path | None, expected_cache_sha: str | None, expected_step: int):
    session = read_env_file(bundle_dir / 'retroarch.session.env')
    run = read_env_file(bundle_dir / 'retroarch.run.env')
    if session is None:
        raise SystemExit(f'expected adapter session provenance in {bundle_dir}')
    if run is None:
        raise SystemExit(f'expected adapter run status provenance in {bundle_dir}')
    if run.get('RUNTIME_EXECUTED') != '1':
        raise SystemExit(f'expected RUNTIME_EXECUTED=1 in {bundle_dir}, found {run.get("RUNTIME_EXECUTED")!r}')
    if run.get('FORCED_TERMINATION') != '0':
        raise SystemExit(f'expected FORCED_TERMINATION=0 in {bundle_dir}, found {run.get("FORCED_TERMINATION")!r}')
    if run.get('RETROARCH_EXIT_STATUS') != '0':
        raise SystemExit(f'expected RETROARCH_EXIT_STATUS=0 in {bundle_dir}, found {run.get("RETROARCH_EXIT_STATUS")!r}')
    if session.get('MODE') != expected_mode:
        raise SystemExit(f'expected adapter MODE={expected_mode!r} in {bundle_dir}, found {session.get("MODE")!r}')

    bundle_meta_path = bundle_dir / 'bundle.json'
    if not bundle_meta_path.is_file():
        raise SystemExit(f'expected bundle provenance manifest in {bundle_dir}')
    bundle_meta = json.loads(bundle_meta_path.read_text())
    if bundle_meta.get('fixture_id') != 'paper-mario-title-timeout-probe':
        raise SystemExit(f'expected timeout probe fixture_id in {bundle_dir}, found {bundle_meta.get("fixture_id")!r}')
    if bundle_meta.get('mode') != expected_mode:
        raise SystemExit(f'expected bundle mode={expected_mode!r} in {bundle_dir}, found {bundle_meta.get("mode")!r}')
    status = bundle_meta.get('status') if isinstance(bundle_meta.get('status'), dict) else {}
    if status.get('runtime_executed') is not True:
        raise SystemExit(f'expected runtime_executed=true in bundle manifest for {bundle_dir}')
    probe_meta = bundle_meta.get('probe') if isinstance(bundle_meta.get('probe'), dict) else {}
    if int(probe_meta.get('step_frames') or -1) != int(expected_step):
        raise SystemExit(f'expected timeout probe step_frames={expected_step} in {bundle_dir}, found {probe_meta.get("step_frames")!r}')
    if int(probe_meta.get('step_chunk_frames') or -1) != int(expected_step):
        raise SystemExit(
            f'expected timeout probe step_chunk_frames={expected_step} in {bundle_dir}, '
            f'found {probe_meta.get("step_chunk_frames")!r}'
        )
    if probe_meta.get('authority_fixture_id') != 'paper-mario-title-screen':
        raise SystemExit(
            f'expected timeout probe authority_fixture_id=paper-mario-title-screen in {bundle_dir}, '
            f'found {probe_meta.get("authority_fixture_id")!r}'
        )
    bundle_inputs = bundle_meta.get('inputs') if isinstance(bundle_meta.get('inputs'), dict) else {}
    rom_path = bundle_inputs.get('rom_path')
    rom_sha = bundle_inputs.get('rom_sha256')
    if not rom_path or not rom_sha:
        raise SystemExit(f'expected bundle ROM path/SHA provenance in {bundle_dir}')
    if not same_resolved_path(rom_path, expected_rom_path):
        raise SystemExit(f'expected bundle ROM path {expected_rom_path} in {bundle_dir}, found {rom_path!r}')
    if rom_sha != expected_rom_sha256:
        raise SystemExit(f'expected bundle ROM SHA {expected_rom_sha256!r} in {bundle_dir}, found {rom_sha!r}')
    if not same_resolved_path(session.get('ROM_PATH'), Path(rom_path)):
        raise SystemExit(f'expected adapter ROM_PATH={rom_path!r} in {bundle_dir}, found {session.get("ROM_PATH")!r}')
    if session.get('ROM_SHA256') != rom_sha:
        raise SystemExit(f'expected adapter ROM_SHA256={rom_sha!r} in {bundle_dir}, found {session.get("ROM_SHA256")!r}')
    if not Path(rom_path).is_file():
        raise SystemExit(f'expected bundle ROM path to exist in {bundle_dir}: {rom_path!r}')
    if sha256_file(Path(rom_path)) != rom_sha:
        raise SystemExit(f'adapter ROM_SHA256 does not match current ROM artifact in {bundle_dir}')

    core_path = session.get('CORE_PATH')
    core_sha = session.get('CORE_SHA256')
    if not core_path or not core_sha:
        raise SystemExit(f'expected adapter core path/SHA provenance in {bundle_dir}')
    if not Path(core_path).is_file():
        raise SystemExit(f'expected adapter CORE_PATH to exist in {bundle_dir}: {core_path!r}')
    if sha256_file(Path(core_path)) != core_sha:
        raise SystemExit(f'adapter CORE_SHA256 does not match current core artifact in {bundle_dir}')
    require_adapter_config_provenance(bundle_dir, session)

    if expected_cache_path is None:
        if session.get('HIRES_CACHE_PATH') or session.get('HIRES_CACHE_SHA256'):
            raise SystemExit(f'expected off-bundle adapter cache provenance to be empty in {bundle_dir}, found {session!r}')
        if bundle_meta.get('hires_pack_path') or bundle_meta.get('hires_pack_sha256') not in (None, '', 'missing'):
            raise SystemExit(f'expected off-bundle top-level cache provenance to be empty in {bundle_dir}, found {bundle_meta!r}')
        if bundle_inputs.get('hires_pack_path') or bundle_inputs.get('hires_pack_sha256') not in (None, '', 'missing'):
            raise SystemExit(f'expected off-bundle cache provenance to be empty in {bundle_dir}, found {bundle_inputs!r}')
    else:
        if not same_resolved_path(session.get('HIRES_CACHE_PATH'), expected_cache_path):
            raise SystemExit(
                f'expected adapter HIRES_CACHE_PATH={expected_cache_path!s} in {bundle_dir}, '
                f'found {session.get("HIRES_CACHE_PATH")!r}'
            )
        if session.get('HIRES_CACHE_SHA256') != expected_cache_sha:
            raise SystemExit(
                f'expected adapter HIRES_CACHE_SHA256={expected_cache_sha!r} in {bundle_dir}, '
                f'found {session.get("HIRES_CACHE_SHA256")!r}'
            )

    expected_command_signature = command_log_signature(bundle_dir)
    if not expected_command_signature:
        raise SystemExit(f'expected adapter command log in {bundle_dir}')
    if session.get('COMMAND_SIGNATURE') != expected_command_signature:
        raise SystemExit(
            f'expected adapter COMMAND_SIGNATURE={expected_command_signature!r} in {bundle_dir}, '
            f'found {session.get("COMMAND_SIGNATURE")!r}'
        )
    planned_command_signature = expected_command_log_signature(bundle_dir)
    if not planned_command_signature:
        raise SystemExit(f'expected adapter planned command log in {bundle_dir}')
    if planned_command_signature != expected_command_signature:
        raise SystemExit(
            f'expected executed adapter command signature {expected_command_signature!r} to match '
            f'planned command signature {planned_command_signature!r} in {bundle_dir}'
        )

def require_on_bundle_cache_provenance(on_dir: Path, on_hires):
    bundle_meta_path = on_dir / 'bundle.json'
    if not bundle_meta_path.is_file():
        raise SystemExit(f'expected on-bundle provenance manifest in {on_dir}')
    bundle_meta = json.loads(bundle_meta_path.read_text())
    bundle_inputs = bundle_meta.get('inputs') if isinstance(bundle_meta.get('inputs'), dict) else {}
    cache_path_values = [
        ('hires_pack_path', bundle_meta.get('hires_pack_path')),
        ('inputs.hires_pack_path', bundle_inputs.get('hires_pack_path')),
    ]
    cache_sha_values = [
        ('hires_pack_sha256', bundle_meta.get('hires_pack_sha256')),
        ('inputs.hires_pack_sha256', bundle_inputs.get('hires_pack_sha256')),
    ]
    if not any(value for _, value in cache_path_values):
        raise SystemExit(f'expected on-bundle hires_pack_path provenance in {on_dir}')
    if not any(value for _, value in cache_sha_values):
        raise SystemExit(f'expected on-bundle hires_pack_sha256 provenance in {on_dir}')
    for label, value in cache_path_values:
        if value and not same_resolved_path(value, cache_path):
            raise SystemExit(f'expected on-bundle {label}={cache_path} in {on_dir}, found {value!r}')
    for label, value in cache_sha_values:
        if value and value != expected_cache_sha256:
            raise SystemExit(f'expected on-bundle {label}={expected_cache_sha256} in {on_dir}, found {value!r}')
    evidence_cache_path = on_hires.get('cache_path')
    if not evidence_cache_path:
        raise SystemExit(f'expected on-bundle hi-res evidence cache_path provenance in {on_dir}')
    if not same_resolved_path(evidence_cache_path, cache_path):
        raise SystemExit(f'expected on-bundle hi-res evidence cache_path={cache_path} in {on_dir}, found {evidence_cache_path!r}')
    evidence_cache_sha = on_hires.get('cache_sha256')
    if not evidence_cache_sha:
        raise SystemExit(f'expected on-bundle hi-res evidence cache_sha256 provenance in {on_dir}')
    if evidence_cache_sha != expected_cache_sha256:
        raise SystemExit(
            f'expected on-bundle hi-res evidence cache_sha256={expected_cache_sha256} in {on_dir}, '
            f'found {evidence_cache_sha!r}'
        )
    expected_step = int(on_dir.name.split('-', 1)[1])
    require_adapter_session_provenance(on_dir, 'on', cache_path, expected_cache_sha256, expected_step)

for off_dir in sorted((bundle_root / 'off').iterdir()):
    if not off_dir.is_dir() or not off_dir.name.startswith('timeout-'):
        continue
    step = int(off_dir.name.split('-', 1)[1])
    on_dir = bundle_root / 'on' / off_dir.name
    off_capture = one_capture(off_dir)
    on_capture = one_capture(on_dir)

    off_img = Image.open(off_capture).convert('RGBA')
    on_img = Image.open(on_capture).convert('RGBA')
    diff = ImageChops.difference(off_img, on_img)
    hist = diff.histogram()
    total_abs = sum((i % 256) * v for i, v in enumerate(hist))
    total_sq = sum(((i % 256) ** 2) * v for i, v in enumerate(hist))
    count = off_img.size[0] * off_img.size[1] * 4

    off_semantic = json.loads((off_dir / 'traces' / 'paper-mario-game-status.json').read_text())
    off_hires = json.loads((off_dir / 'traces' / 'hires-evidence.json').read_text())
    off_hires_summary = off_hires.get('summary') or {}
    if not disabled_hires_evidence_ok(off_hires):
        raise SystemExit(f'expected off-bundle hi-res evidence to stay disabled in {off_dir}, found {off_hires!r}')
    require_adapter_session_provenance(off_dir, 'off', None, None, step)
    off_status = off_semantic.get('paper_mario_us', {})
    if (
        off_status.get('game_status', {}).get('map_name_candidate') != 'kmr_03'
        or int(off_status.get('game_status', {}).get('entry_id') or -1) != 5
        or off_status.get('cur_game_mode', {}).get('init_symbol') != 'state_init_world'
        or off_status.get('cur_game_mode', {}).get('step_symbol') != 'state_step_world'
    ):
        raise SystemExit(f'unexpected off-bundle semantic state in {off_dir}: {off_status!r}')
    on_semantic = json.loads((on_dir / 'traces' / 'paper-mario-game-status.json').read_text())
    on_status = on_semantic.get('paper_mario_us', {})
    if (
        on_status.get('game_status', {}).get('map_name_candidate') != 'kmr_03'
        or int(on_status.get('game_status', {}).get('entry_id') or -1) != 5
        or on_status.get('cur_game_mode', {}).get('init_symbol') != 'state_init_world'
        or on_status.get('cur_game_mode', {}).get('step_symbol') != 'state_step_world'
    ):
        raise SystemExit(f'unexpected on-bundle semantic state in {on_dir}: {on_status!r}')
    if off_semantic != on_semantic:
        raise SystemExit(f'expected selected-package semantic trace to match feature-off trace for step {step}')
    on_hires = json.loads((on_dir / 'traces' / 'hires-evidence.json').read_text())
    require_on_bundle_cache_provenance(on_dir, on_hires)
    hires_summary = on_hires.get('summary') or {}
    if on_hires.get('available') is not True:
        raise SystemExit(f'expected on-bundle hi-res evidence to be available in {on_dir}')
    if on_hires.get('cache_loaded') is not True:
        raise SystemExit(f'expected on-bundle hi-res cache to be loaded in {on_dir}')
    if hires_summary.get('provider') != 'on':
        raise SystemExit(f'expected on-bundle hi-res provider to be "on" in {on_dir}, found {hires_summary.get("provider")!r}')
    if hires_summary.get('source_mode') != 'phrb-only':
        raise SystemExit(f'expected selected-package source_mode=phrb-only in {on_dir}, found {hires_summary.get("source_mode")!r}')
    if int(hires_summary.get('compat_entry_count') or 0) != 0:
        raise SystemExit(f'expected selected-package compat_entry_count=0 in {on_dir}, found {hires_summary.get("compat_entry_count")!r}')
    if int(hires_summary.get('native_sampled_entry_count') or 0) < int(os.environ['MIN_NATIVE_SAMPLED_COUNT']):
        raise SystemExit(
            f'expected native sampled entries >= {os.environ["MIN_NATIVE_SAMPLED_COUNT"]} in {on_dir}, '
            f'found {hires_summary.get("native_sampled_entry_count")!r}'
        )
    if int((hires_summary.get('source_counts') or {}).get('phrb') or 0) < 1:
        raise SystemExit(f'expected PHRB-backed entries in {on_dir}, found {(hires_summary.get("source_counts") or {}).get("phrb")!r}')
    for key, value in (hires_summary.get('source_counts') or {}).items():
        if key != 'phrb' and int(value or 0) != 0:
            raise SystemExit(f'expected selected-package source_counts.{key}=0 in {on_dir}, found {value!r}')
    descriptor_path_counts = hires_summary.get('descriptor_path_counts') or {}
    if int(descriptor_path_counts.get('sampled') or 0) <= 0:
        raise SystemExit(f'expected sampled descriptor path evidence in {on_dir}, found {descriptor_path_counts!r}')
    for key in ('native_checksum', 'generic', 'compat'):
        if int(descriptor_path_counts.get(key) or 0) != 0:
            raise SystemExit(f'expected selected-package descriptor_paths.{key}=0 in {on_dir}, found {descriptor_path_counts.get(key)!r}')
    sampled_object_probe = on_hires.get('sampled_object_probe') or {}
    if sampled_object_probe.get('available') is not True:
        raise SystemExit(f'expected sampled-object probe to be available in {on_dir}')
    if int(sampled_object_probe.get('line_count') or 0) <= 0:
        raise SystemExit(f'expected sampled-object probe line_count > 0 in {on_dir}, found {sampled_object_probe.get("line_count")!r}')
    for key in ('exact_hit_count', 'exact_miss_count', 'exact_conflict_miss_count', 'exact_unresolved_miss_count'):
        if sampled_object_probe.get(key) is None:
            raise SystemExit(f'expected sampled-object probe field {key} in {on_dir}')
    sampled_duplicate_probe = on_hires.get('sampled_duplicate_probe') or {}
    if sampled_duplicate_probe.get('available') is not True:
        raise SystemExit(f'expected sampled duplicate probe to be available in {on_dir}')
    if int(sampled_duplicate_probe.get('line_count') or 0) <= 0:
        raise SystemExit(f'expected sampled duplicate probe line_count > 0 in {on_dir}, found {sampled_duplicate_probe.get("line_count")!r}')
    for key in ('line_count', 'unique_bucket_count'):
        if sampled_duplicate_probe.get(key) is None:
            raise SystemExit(f'expected sampled duplicate probe field {key} in {on_dir}')
    sampled_pool_stream_probe = on_hires.get('sampled_pool_stream_probe') or {}
    if sampled_pool_stream_probe.get('available') is not True:
        raise SystemExit(f'expected sampled pool stream probe to be available in {on_dir}')
    if int(sampled_pool_stream_probe.get('line_count') or 0) <= 0:
        raise SystemExit(f'expected sampled pool stream probe line_count > 0 in {on_dir}, found {sampled_pool_stream_probe.get("line_count")!r}')
    for key in ('line_count', 'family_count'):
        if sampled_pool_stream_probe.get(key) is None:
            raise SystemExit(f'expected sampled pool stream probe field {key} in {on_dir}')
    review_md = on_dir / 'traces' / 'hires-sampled-selector-review.md'
    review_json = on_dir / 'traces' / 'hires-sampled-selector-review.json'
    alternate_source_review_json = on_dir / 'traces' / 'hires-alternate-source-review.json'
    alternate_source_activation_review_json = on_dir / 'traces' / 'hires-alternate-source-activation-review.json'
    cross_scene_review_json = on_dir / 'traces' / 'hires-sampled-cross-scene-review.json'
    seam_register_json = on_dir / 'traces' / 'hires-runtime-seam-register.json'
    pool_regression_review_json = next(iter(sorted((on_dir / 'traces').glob('hires-sampled-pool-regression-review-*.json'))), None)
    if review_json.is_file() and not seam_register_json.is_file():
        raise SystemExit(f'expected runtime seam-register evidence in {on_dir}')
    alternate_source_review_data = {}
    if alternate_source_review_json.is_file():
        alternate_source_review_data = json.loads(alternate_source_review_json.read_text())
    cross_scene_review_data = {}
    if cross_scene_review_json.is_file():
        cross_scene_review_data = json.loads(cross_scene_review_json.read_text())
    alternate_source_activation_review_data = {}
    if alternate_source_activation_review_json.is_file():
        alternate_source_activation_review_data = json.loads(alternate_source_activation_review_json.read_text())
    seam_register_data = {}
    if seam_register_json.is_file():
        seam_register_data = json.loads(seam_register_json.read_text())
    pool_regression_review_data = {}
    if pool_regression_review_json and pool_regression_review_json.is_file():
        pool_regression_review_data = json.loads(pool_regression_review_json.read_text())
    pool_reviews = []
    duplicate_reviews = []
    for pool_json in sorted((on_dir / 'traces').glob('hires-sampled-pool-review-*.json')):
        pool_review = json.loads(pool_json.read_text())
        pool_md = pool_json.with_suffix('.md')
        pool_reviews.append({
            'sampled_low32': pool_review.get('sampled_low32'),
            'review_status': pool_review.get('review_status') or 'complete',
            'pool_recommendation': pool_review.get('pool_recommendation'),
            'runtime_shape_recommendation': pool_review.get('runtime_shape_recommendation'),
            'runtime_sample_replacement_id': pool_review.get('runtime_sample_replacement_id'),
            'runtime_sample_policy': pool_review.get('runtime_sample_policy'),
            'json_path': str(pool_json),
            'markdown_path': str(pool_md) if pool_md.is_file() else None,
        })
    for duplicate_json in sorted((on_dir / 'traces').glob('hires-sampled-duplicate-review-*.json')):
        duplicate_review = json.loads(duplicate_json.read_text())
        duplicate_md = duplicate_json.with_suffix('.md')
        duplicate_reviews.append({
            'sampled_low32': duplicate_review.get('sampled_low32'),
            'selector': duplicate_review.get('selector'),
            'recommendation': duplicate_review.get('recommendation'),
            'active_replacement_id': (duplicate_review.get('duplicate_bucket') or {}).get('replacement_id'),
            'selector_candidate_count': duplicate_review.get('selector_candidate_count'),
            'unique_selector_pixel_hash_count': len(duplicate_review.get('unique_selector_pixel_hashes') or []),
            'broader_alias_replacement_ids': duplicate_review.get('broader_alias_replacement_ids') or [],
            'json_path': str(duplicate_json),
            'markdown_path': str(duplicate_md) if duplicate_md.is_file() else None,
        })
    summary['steps'].append({
        'step_frames': step,
        'passed': True,
        'off_bundle': str(off_dir),
        'on_bundle': str(on_dir),
        'ae': total_abs,
        'rmse': math.sqrt(total_sq / count),
        'semantic': {
            'map_name_candidate': on_semantic.get('paper_mario_us', {}).get('game_status', {}).get('map_name_candidate'),
            'entry_id': on_semantic.get('paper_mario_us', {}).get('game_status', {}).get('entry_id'),
            'init_symbol': on_semantic.get('paper_mario_us', {}).get('cur_game_mode', {}).get('init_symbol'),
            'step_symbol': on_semantic.get('paper_mario_us', {}).get('cur_game_mode', {}).get('step_symbol'),
        },
        'off_semantic': {
            'map_name_candidate': off_semantic.get('paper_mario_us', {}).get('game_status', {}).get('map_name_candidate'),
            'entry_id': off_semantic.get('paper_mario_us', {}).get('game_status', {}).get('entry_id'),
            'init_symbol': off_semantic.get('paper_mario_us', {}).get('cur_game_mode', {}).get('init_symbol'),
            'step_symbol': off_semantic.get('paper_mario_us', {}).get('cur_game_mode', {}).get('step_symbol'),
        },
        'off_hires_summary': off_hires_summary,
        'hires_summary': hires_summary,
        'descriptor_path_counts': descriptor_path_counts,
        'sampled_object_probe': {
            'exact_hit_count': sampled_object_probe.get('exact_hit_count'),
            'exact_miss_count': sampled_object_probe.get('exact_miss_count'),
            'exact_conflict_miss_count': sampled_object_probe.get('exact_conflict_miss_count'),
            'exact_unresolved_miss_count': sampled_object_probe.get('exact_unresolved_miss_count'),
            'top_exact_hit_buckets': sampled_object_probe.get('top_exact_hit_buckets', [])[:5],
            'top_exact_conflict_miss_buckets': sampled_object_probe.get('top_exact_conflict_miss_buckets', [])[:5],
            'top_exact_unresolved_miss_buckets': sampled_object_probe.get('top_exact_unresolved_miss_buckets', [])[:5],
        },
        'sampled_duplicate_probe': {
            'line_count': sampled_duplicate_probe.get('line_count'),
            'unique_bucket_count': sampled_duplicate_probe.get('unique_bucket_count'),
            'top_buckets': sampled_duplicate_probe.get('top_buckets', [])[:5],
        },
        'sampled_pool_stream_probe': {
            'line_count': sampled_pool_stream_probe.get('line_count'),
            'family_count': sampled_pool_stream_probe.get('family_count'),
            'top_families': sampled_pool_stream_probe.get('top_families', [])[:5],
        },
        'sampled_selector_review': {
            'markdown_path': str(review_md) if review_md.is_file() else None,
            'json_path': str(review_json) if review_json.is_file() else None,
        },
        'runtime_seam_register': {
            'markdown_path': str(on_dir / 'traces' / 'hires-runtime-seam-register.md') if (on_dir / 'traces' / 'hires-runtime-seam-register.md').is_file() else None,
            'json_path': str(seam_register_json) if seam_register_json.is_file() else None,
            'summary': seam_register_data.get('summary') or {},
            'pool_conflict_families': (seam_register_data.get('pool_conflict_families') or [])[:5],
            'sampled_duplicate_families': (seam_register_data.get('sampled_duplicate_families') or [])[:5],
        },
        'alternate_source_review': {
            'markdown_path': str(on_dir / 'traces' / 'hires-alternate-source-review.md') if (on_dir / 'traces' / 'hires-alternate-source-review.md').is_file() else None,
            'json_path': str(alternate_source_review_json) if alternate_source_review_json.is_file() else None,
            'group_count': alternate_source_review_data.get('group_count'),
            'available_group_count': alternate_source_review_data.get('available_group_count'),
            'total_candidate_count': alternate_source_review_data.get('total_candidate_count'),
            'groups': [
                {
                    'sampled_low32': (group.get('signature') or {}).get('sampled_low32'),
                    'status': group.get('alternate_source_status'),
                    'seed_dimensions': (group.get('seeded_transport_pool') or {}).get('seed_dimensions'),
                    'candidate_count': (group.get('seeded_transport_pool') or {}).get('candidate_count'),
                }
                for group in (alternate_source_review_data.get('groups') or [])[:5]
            ],
        },
        'alternate_source_activation_review': {
            'markdown_path': str(on_dir / 'traces' / 'hires-alternate-source-activation-review.md') if (on_dir / 'traces' / 'hires-alternate-source-activation-review.md').is_file() else None,
            'json_path': str(alternate_source_activation_review_json) if alternate_source_activation_review_json.is_file() else None,
            'summary': alternate_source_activation_review_data.get('summary') or {},
            'families': [
                {
                    'sampled_low32': family.get('sampled_low32'),
                    'activation_status': family.get('activation_status'),
                    'activation_recommendation': family.get('activation_recommendation'),
                    'candidate_count': family.get('candidate_count'),
                    'cross_scene_promotion_status': family.get('cross_scene_promotion_status'),
                }
                for family in (alternate_source_activation_review_data.get('families') or [])[:5]
            ],
        },
        'sampled_cross_scene_review': {
            'markdown_path': str(on_dir / 'traces' / 'hires-sampled-cross-scene-review.md') if (on_dir / 'traces' / 'hires-sampled-cross-scene-review.md').is_file() else None,
            'json_path': str(cross_scene_review_json) if cross_scene_review_json.is_file() else None,
            'target_labels': cross_scene_review_data.get('target_labels') or [],
            'guard_labels': cross_scene_review_data.get('guard_labels') or [],
            'families': [
                {
                    'sampled_low32': family.get('sampled_low32'),
                    'promotion_status': family.get('promotion_status'),
                    'shared_signature_count': family.get('shared_signature_count'),
                    'target_exclusive_signature_count': family.get('target_exclusive_signature_count'),
                    'shared_guard_labels': family.get('shared_guard_labels') or [],
                    'guard_labels_without_observation': family.get('guard_labels_without_observation') or [],
                }
                for family in (cross_scene_review_data.get('families') or [])[:5]
            ],
        },
        'sampled_pool_reviews': pool_reviews,
        'sampled_duplicate_reviews': duplicate_reviews,
        'sampled_pool_regression_review': {
            'markdown_path': str(pool_regression_review_json.with_suffix('.md')) if pool_regression_review_json and pool_regression_review_json.with_suffix('.md').is_file() else None,
            'json_path': str(pool_regression_review_json) if pool_regression_review_json and pool_regression_review_json.is_file() else None,
            'sampled_low32': pool_regression_review_data.get('sampled_low32'),
            'recommendation': (pool_regression_review_data.get('recommendation') or {}).get('recommendation'),
            'pool_follow_up': (pool_regression_review_data.get('recommendation') or {}).get('pool_follow_up'),
            'reasons': ((pool_regression_review_data.get('recommendation') or {}).get('reasons') or [])[:5],
            'case_metrics': [
                {
                    'label': case.get('label'),
                    'ae': case.get('ae'),
                    'rmse': case.get('rmse'),
                    'family_total_hits': case.get('family_total_hits'),
                    'family_reason_counts': case.get('family_reason_counts') or [],
                }
                for case in (pool_regression_review_data.get('cases') or [])[:3]
            ],
        },
    })

summary_path = bundle_root / 'validation-summary.json'
summary_path.write_text(json.dumps(summary, indent=2) + '\n')
md = [
    '# Title Timeout Selected-Package Validation',
    '',
    f'- Cache: `{cache_path}`',
    f'- Cache SHA-256: `{summary["cache_sha256"]}`',
    '',
]
for step in summary['steps']:
    md.extend([
        f'## {step["step_frames"]} Frames',
        f'- On bundle: [{Path(step["on_bundle"]).name}]({step["on_bundle"]})',
        f'- Off bundle: [{Path(step["off_bundle"]).name}]({step["off_bundle"]})',
        f'- AE: `{step["ae"]}`',
        f'- RMSE: `{step["rmse"]}`',
        f'- Semantic: `{step["semantic"]["init_symbol"]}` / `{step["semantic"]["step_symbol"]}`, map `{step["semantic"]["map_name_candidate"]}`, entry `{step["semantic"]["entry_id"]}`',
    ])
    hires_summary = step.get('hires_summary') or {}
    summary_line = f'- Hi-res summary: provider `{hires_summary.get("provider")}`'
    if hires_summary.get('source_mode') is not None:
        summary_line += f', source mode `{hires_summary.get("source_mode")}`'
    if hires_summary.get('entry_count') is not None:
        summary_line += (
            f', entries `{hires_summary.get("entry_count")}`'
            f', native sampled `{hires_summary.get("native_sampled_entry_count")}`'
            f', compat `{hires_summary.get("compat_entry_count")}`'
            f', sampled index `{hires_summary.get("sampled_index_count")}`'
            f', sampled dupe keys `{hires_summary.get("sampled_duplicate_key_count")}`'
            f', sampled families `{hires_summary.get("sampled_family_count")}`'
            f', source PHRB `{(hires_summary.get("source_counts") or {}).get("phrb")}`'
        )
    descriptor_path_counts = step.get('descriptor_path_counts') or {}
    descriptor_detail = hires_summary.get('descriptor_path_detail_counts') or {}
    resolution_reasons = hires_summary.get('resolution_reason_counts') or {}
    if descriptor_path_counts:
        summary_line += (
            f', descriptor paths sampled `{descriptor_path_counts.get("sampled", 0)}`'
            f' / native checksum `{descriptor_path_counts.get("native_checksum", 0)}`'
            f' / generic `{descriptor_path_counts.get("generic", 0)}`'
            f' / compat `{descriptor_path_counts.get("compat", 0)}`'
        )
    if descriptor_detail:
        summary_line += (
            f', sampled detail family singleton `{descriptor_detail.get("sampled_family_singleton", 0)}`'
            f' / ordered-surface singleton `{descriptor_detail.get("sampled_ordered_surface_singleton", 0)}`'
            f' / exact selector `{descriptor_detail.get("sampled_exact_selector", 0)}`'
            f', native checksum detail exact `{descriptor_detail.get("native_checksum_exact", 0)}`'
            f' / identity assisted `{descriptor_detail.get("native_checksum_identity_assisted", 0)}`'
            f' / generic fallback `{descriptor_detail.get("native_checksum_generic_fallback", 0)}`'
            f', generic detail identity assisted `{descriptor_detail.get("generic_identity_assisted", 0)}`'
            f' / plain `{descriptor_detail.get("generic_plain", 0)}`'
        )
    if resolution_reasons:
        formatted_reasons = ", ".join(
            f'`{reason}` x `{count}`'
            for reason, count in list(resolution_reasons.items())[:6]
        )
        summary_line += f', resolution reasons {formatted_reasons}'
    md.extend([
        summary_line,
        f'- Sampled exact hits: `{step["sampled_object_probe"]["exact_hit_count"]}`',
        f'- Sampled exact misses: `{step["sampled_object_probe"]["exact_miss_count"]}`',
        f'- Sampled conflict misses: `{step["sampled_object_probe"]["exact_conflict_miss_count"]}`',
        f'- Sampled unresolved misses: `{step["sampled_object_probe"]["exact_unresolved_miss_count"]}`',
    ])
    duplicate_probe = step.get('sampled_duplicate_probe') or {}
    md.append(
        f'- Sampled duplicate keys: `{duplicate_probe.get("unique_bucket_count")}` buckets, `{duplicate_probe.get("line_count")}` log lines'
    )
    for duplicate_bucket in duplicate_probe.get('top_buckets', [])[:3]:
        duplicate_fields = duplicate_bucket.get('fields') or {}
        md.append(
            '- Sampled duplicate detail: '
            f'low32 `{duplicate_fields.get("sampled_low32")}`, '
            f'palette `{duplicate_fields.get("palette_crc")}`, '
            f'fs `{duplicate_fields.get("fs")}`, '
            f'selector `{duplicate_fields.get("selector")}`, '
            f'total `{duplicate_fields.get("total_entries")}`, '
            f'active policy `{duplicate_fields.get("policy")}`, '
            f'replacement `{duplicate_fields.get("replacement_id")}`'
        )
    pool_stream_probe = step.get('sampled_pool_stream_probe') or {}
    md.append(
        f'- Sampled pool stream families: `{pool_stream_probe.get("family_count", 0)}` families, `{pool_stream_probe.get("line_count", 0)}` log lines'
    )
    for pool_family in pool_stream_probe.get('top_families', [])[:3]:
        pool_fields = pool_family.get('fields') or {}
        md.append(
            '- Sampled pool stream detail: '
            f'low32 `{pool_fields.get("sampled_low32")}`, '
            f'palette `{pool_fields.get("palette_crc")}`, '
            f'fs `{pool_fields.get("fs")}`, '
            f'observed `{pool_fields.get("observed_count")}` across `{pool_fields.get("unique_observed_selectors")}` selectors, '
            f'transitions `{pool_fields.get("transition_count")}`, '
            f'max run `{pool_fields.get("max_run")}`, '
            f'latest selector `{pool_fields.get("observed_selector")}` from `{pool_fields.get("observed_selector_source")}`'
        )
    review_md = step.get('sampled_selector_review', {}).get('markdown_path')
    review_json = step.get('sampled_selector_review', {}).get('json_path')
    if review_md:
        md.append(f'- Sampled selector review: [{Path(review_md).name}]({review_md})')
    if review_json:
        md.append(f'- Sampled selector review JSON: [{Path(review_json).name}]({review_json})')
    seam_md = step.get('runtime_seam_register', {}).get('markdown_path')
    seam_json = step.get('runtime_seam_register', {}).get('json_path')
    if seam_md:
        md.append(f'- Runtime seam register: [{Path(seam_md).name}]({seam_md})')
    if seam_json:
        md.append(f'- Runtime seam register JSON: [{Path(seam_json).name}]({seam_json})')
    alt_review = step.get('alternate_source_review') or {}
    alt_review_md = alt_review.get('markdown_path')
    alt_review_json = alt_review.get('json_path')
    if alt_review_md:
        md.append(
            f"- Alternate-source review: [{Path(alt_review_md).name}]({alt_review_md}) "
            f"-> `{alt_review.get('available_group_count')}` / `{alt_review.get('group_count')}` groups with candidates, "
            f"`{alt_review.get('total_candidate_count')}` total candidates"
        )
    if alt_review_json:
        md.append(f'- Alternate-source review JSON: [{Path(alt_review_json).name}]({alt_review_json})')
    for alt_group in alt_review.get('groups', []):
        md.append(
            f"- Alternate-source family `{alt_group.get('sampled_low32')}`: "
            f"`{alt_group.get('status')}`, dims `{alt_group.get('seed_dimensions')}`, "
            f"candidates `{alt_group.get('candidate_count')}`"
        )
    activation_review = step.get('alternate_source_activation_review') or {}
    activation_review_md = activation_review.get('markdown_path')
    activation_review_json = activation_review.get('json_path')
    activation_summary = activation_review.get('summary') or {}
    if activation_review_md:
        md.append(
            f"- Alternate-source activation review: [{Path(activation_review_md).name}]({activation_review_md}) "
            f"-> review-bounded `{activation_summary.get('review_bounded_probe_count', 0)}`, "
            f"shared-scene blocked `{activation_summary.get('shared_scene_blocked_count', 0)}`, "
            f"partial-overlap blocked `{activation_summary.get('partial_overlap_blocked_count', 0)}`"
        )
    if activation_review_json:
        md.append(f'- Alternate-source activation review JSON: [{Path(activation_review_json).name}]({activation_review_json})')
    for family in activation_review.get('families', []):
        md.append(
            f"- Alternate-source activation family `{family.get('sampled_low32')}`: "
            f"`{family.get('activation_status')}`, "
            f"`{family.get('activation_recommendation')}`, "
            f"candidates `{family.get('candidate_count')}`, "
            f"cross-scene `{family.get('cross_scene_promotion_status')}`"
        )
    cross_scene_review = step.get('sampled_cross_scene_review') or {}
    cross_scene_review_md = cross_scene_review.get('markdown_path')
    cross_scene_review_json = cross_scene_review.get('json_path')
    if cross_scene_review_md:
        md.append(f'- Cross-scene review: [{Path(cross_scene_review_md).name}]({cross_scene_review_md})')
    if cross_scene_review_json:
        md.append(f'- Cross-scene review JSON: [{Path(cross_scene_review_json).name}]({cross_scene_review_json})')
    for family in cross_scene_review.get('families', []):
        extra = []
        if family.get('shared_guard_labels'):
            extra.append(f"shared guards `{','.join(family.get('shared_guard_labels') or [])}`")
        if family.get('guard_labels_without_observation'):
            extra.append(f"absent guards `{','.join(family.get('guard_labels_without_observation') or [])}`")
        md.append(
            f"- Cross-scene family `{family.get('sampled_low32')}`: "
            f"`{family.get('promotion_status')}`, shared `{family.get('shared_signature_count')}`, "
            f"target-exclusive `{family.get('target_exclusive_signature_count')}`"
            f"{', ' + ', '.join(extra) if extra else ''}"
        )
    for pool_review in step.get('sampled_pool_reviews', []):
        if pool_review.get('markdown_path'):
            replacement_suffix = ''
            if pool_review.get('runtime_sample_replacement_id'):
                replacement_suffix = f", replacement `{pool_review.get('runtime_sample_replacement_id')}`"
            status_suffix = f", status `{pool_review.get('review_status') or 'complete'}`"
            review_label = pool_review.get('runtime_shape_recommendation') or pool_review.get('review_status') or 'unknown'
            md.append(
                f"- Sampled pool review `{pool_review.get('sampled_low32')}`: "
                f"[{Path(pool_review['markdown_path']).name}]({pool_review['markdown_path']}) "
                f"-> `{review_label}`{replacement_suffix}{status_suffix}"
            )
        if pool_review.get('json_path'):
            md.append(
                f"- Sampled pool review JSON `{pool_review.get('sampled_low32')}`: "
                f"[{Path(pool_review['json_path']).name}]({pool_review['json_path']})"
            )
    for duplicate_review in step.get('sampled_duplicate_reviews', []):
        if duplicate_review.get('markdown_path'):
            replacement_suffix = ''
            if duplicate_review.get('active_replacement_id'):
                replacement_suffix = f", active replacement `{duplicate_review.get('active_replacement_id')}`"
            md.append(
                f"- Sampled duplicate review `{duplicate_review.get('sampled_low32')}`: "
                f"[{Path(duplicate_review['markdown_path']).name}]({duplicate_review['markdown_path']}) "
                f"-> `{duplicate_review.get('recommendation')}`{replacement_suffix}"
            )
        if duplicate_review.get('json_path'):
            md.append(
                f"- Sampled duplicate review JSON `{duplicate_review.get('sampled_low32')}`: "
                f"[{Path(duplicate_review['json_path']).name}]({duplicate_review['json_path']})"
            )
    pool_regression_review = step.get('sampled_pool_regression_review') or {}
    if pool_regression_review.get('markdown_path'):
        md.append(
            f"- Sampled pool regression review: "
            f"[{Path(pool_regression_review['markdown_path']).name}]({pool_regression_review['markdown_path']}) "
            f"-> `{pool_regression_review.get('recommendation')}`"
        )
    if pool_regression_review.get('json_path'):
        md.append(
            f"- Sampled pool regression review JSON: "
            f"[{Path(pool_regression_review['json_path']).name}]({pool_regression_review['json_path']})"
        )
    for case in pool_regression_review.get('case_metrics', []):
        reasons = ", ".join(
            f"{row.get('reason')} x`{row.get('count')}`" for row in case.get('family_reason_counts') or []
        ) or "none"
        md.append(
            f"- Sampled pool regression case `{case.get('label')}`: "
            f"AE `{case.get('ae')}`, RMSE `{case.get('rmse')}`, "
            f"`{pool_regression_review.get('sampled_low32')}` hits `{case.get('family_total_hits')}`, reasons {reasons}"
        )
    md.append('')
(bundle_root / 'validation-summary.md').write_text('\n'.join(md) + '\n')
print(summary_path)
print(bundle_root / 'validation-summary.md')
PY

echo "[validation] complete: $BUNDLE_ROOT"
