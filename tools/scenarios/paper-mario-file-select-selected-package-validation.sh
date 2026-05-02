#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "$SCRIPT_DIR/../.." && pwd)"
source "$SCRIPT_DIR/lib/common.sh"

CACHE_PATH="${PARALLEL_RDP_HIRES_CACHE_PATH:-}"
BUNDLE_ROOT=""
RUN_PROBES=1
INPUT_MASK=""
INPUT_SEQUENCE=""
INPUT_HOLD_FRAMES="1"
INPUT_REPEAT_COUNT="1"
INTER_PULSE_SETTLE_FRAMES="5"
POST_INPUT_SETTLE_FRAMES="20"
STEP_CHUNK_FRAMES="1"
PROBE_LABEL="selected-package-validation"

usage() {
  cat <<'USAGE'
Usage:
  tools/scenarios/paper-mario-file-select-selected-package-validation.sh [options]

Options:
  --cache-path PATH        Selected PHRB package to validate (defaults to env PARALLEL_RDP_HIRES_CACHE_PATH)
  --bundle-root PATH       Root directory for emitted legacy/selected probe bundles
  --input-mask HEX         Controller mask to pulse (example: 0x01)
  --input-sequence SPEC    Comma-separated pulse sequence overriding repeat mode
  --input-hold-frames N    Frames to hold each pulse (default: 1)
  --input-repeat-count N   Number of repeated pulses when using --input-mask (default: 1)
  --inter-pulse-settle N   Frames to settle between repeated pulses (default: 5)
  --post-input-settle N    Frames to settle after final pulse (default: 20)
  --step-chunk-frames N    Maximum frames per STEP_FRAME command (default: 1)
  --probe-label LABEL      Short label for bundle metadata
  --reuse                  Reuse existing bundles instead of rerunning probes
  -h, --help               Show this help
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
    --input-mask)
      shift
      INPUT_MASK="${1:-}"
      ;;
    --input-sequence)
      shift
      INPUT_SEQUENCE="${1:-}"
      ;;
    --input-hold-frames)
      shift
      INPUT_HOLD_FRAMES="${1:-}"
      ;;
    --input-repeat-count)
      shift
      INPUT_REPEAT_COUNT="${1:-}"
      ;;
    --inter-pulse-settle)
      shift
      INTER_PULSE_SETTLE_FRAMES="${1:-}"
      ;;
    --post-input-settle)
      shift
      POST_INPUT_SETTLE_FRAMES="${1:-}"
      ;;
    --step-chunk-frames)
      shift
      STEP_CHUNK_FRAMES="${1:-}"
      ;;
    --probe-label)
      shift
      PROBE_LABEL="${1:-}"
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
if [[ -z "$INPUT_MASK" && -z "$INPUT_SEQUENCE" ]]; then
  echo "--input-mask or --input-sequence is required." >&2
  exit 2
fi

if [[ -z "$BUNDLE_ROOT" ]]; then
  BUNDLE_ROOT="$REPO_ROOT/artifacts/paper-mario-file-select-validation/$(date +"%Y%m%d-%H%M%S")-${PROBE_LABEL}"
fi
mkdir -p "$BUNDLE_ROOT/legacy" "$BUNDLE_ROOT/selected"

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

probe_args=(
  --probe-label "$PROBE_LABEL"
  --post-input-settle "$POST_INPUT_SETTLE_FRAMES"
  --step-chunk-frames "$STEP_CHUNK_FRAMES"
  --run
)
if [[ -n "$INPUT_SEQUENCE" ]]; then
  probe_args+=(--input-sequence "$INPUT_SEQUENCE")
else
  probe_args+=(
    --input-mask "$INPUT_MASK"
    --input-hold-frames "$INPUT_HOLD_FRAMES"
    --input-repeat-count "$INPUT_REPEAT_COUNT"
    --inter-pulse-settle "$INTER_PULSE_SETTLE_FRAMES"
  )
fi

legacy_bundle="$BUNDLE_ROOT/legacy"
selected_bundle="$BUNDLE_ROOT/selected"

if (( RUN_PROBES )); then
  env "${HIRES_ENV_UNSET[@]}" \
  tools/scenarios/paper-mario-file-select-input-probe.sh \
    --mode off \
    "${probe_args[@]}" \
    --bundle-dir "$legacy_bundle"

  env "${HIRES_ENV_UNSET[@]}" \
  PARALLEL_RDP_HIRES_CACHE_PATH="$CACHE_PATH" \
  PARALLEL_RDP_HIRES_SAMPLED_OBJECT_LOOKUP=1 \
  PARALLEL_RDP_HIRES_SAMPLED_OBJECT_PROBE=1 \
  tools/scenarios/paper-mario-file-select-input-probe.sh \
    --mode on \
    "${probe_args[@]}" \
    --bundle-dir "$selected_bundle"
fi

if [[ ! -d "$legacy_bundle" || ! -d "$selected_bundle" ]]; then
  echo "Missing bundles under $BUNDLE_ROOT" >&2
  exit 1
fi

EXPECTED_ROM_PATH="${PAPER_MARIO_EXPECTED_ROM_PATH:-$REPO_ROOT/assets/Paper Mario (USA).zip}"
python3 - "$CACHE_PATH" "$BUNDLE_ROOT" "$INPUT_MASK" "$INPUT_SEQUENCE" "$INPUT_HOLD_FRAMES" "$INPUT_REPEAT_COUNT" "$INTER_PULSE_SETTLE_FRAMES" "$POST_INPUT_SETTLE_FRAMES" "$STEP_CHUNK_FRAMES" "$PROBE_LABEL" "$EXPECTED_ROM_PATH" <<'PY'
import hashlib
import json
import math
import sys
from pathlib import Path
from PIL import Image, ImageChops

cache_path = Path(sys.argv[1])
bundle_root = Path(sys.argv[2])
expected_input_mask = sys.argv[3]
expected_input_sequence = sys.argv[4]
expected_input_hold_frames = int(sys.argv[5])
expected_input_repeat_count = int(sys.argv[6])
expected_inter_pulse_settle_frames = int(sys.argv[7])
expected_post_input_settle_frames = int(sys.argv[8])
expected_step_chunk_frames = int(sys.argv[9])
expected_probe_label = sys.argv[10]
expected_rom_path = Path(sys.argv[11])
expected_rom_sha256 = hashlib.sha256(expected_rom_path.read_bytes()).hexdigest()
legacy_dir = bundle_root / 'legacy'
selected_dir = bundle_root / 'selected'

def sha256_file(path: Path):
    h = hashlib.sha256()
    with path.open('rb') as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b''):
            h.update(chunk)
    return h.hexdigest()

def one_capture(bundle_dir: Path):
    captures = sorted((bundle_dir / 'captures').glob('*.png'))
    if len(captures) != 1:
        raise SystemExit(f'expected exactly one capture in {bundle_dir}/captures, found {len(captures)}')
    return captures[0]

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

def require_adapter_session_provenance(bundle_dir: Path, expected_mode: str, expected_cache_sha: str | None):
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
        raise SystemExit(f'expected adapter MODE={expected_mode} in {bundle_dir}, found {session.get("MODE")!r}')

    bundle_meta_path = bundle_dir / 'bundle.json'
    if not bundle_meta_path.is_file():
        raise SystemExit(f'expected bundle provenance manifest in {bundle_dir}')
    bundle_meta = json.loads(bundle_meta_path.read_text())
    if bundle_meta.get('fixture_id') != 'paper-mario-file-select-input-probe':
        raise SystemExit(
            f'expected file-select input-probe fixture_id in {bundle_dir}, '
            f'found {bundle_meta.get("fixture_id")!r}'
        )
    if bundle_meta.get('mode') != expected_mode:
        raise SystemExit(f'expected bundle mode={expected_mode!r} in {bundle_dir}, found {bundle_meta.get("mode")!r}')
    probe_meta = bundle_meta.get('probe') if isinstance(bundle_meta.get('probe'), dict) else {}
    expected_probe = {
        'label': expected_probe_label,
        'input_mask': expected_input_mask,
        'input_sequence': expected_input_sequence,
        'input_hold_frames': expected_input_hold_frames,
        'input_repeat_count': expected_input_repeat_count,
        'inter_pulse_settle_frames': expected_inter_pulse_settle_frames,
        'post_input_settle_frames': expected_post_input_settle_frames,
        'step_chunk_frames': expected_step_chunk_frames,
    }
    for key, expected_value in expected_probe.items():
        actual_value = probe_meta.get(key)
        if isinstance(expected_value, int):
            try:
                actual_value = int(actual_value)
            except (TypeError, ValueError):
                raise SystemExit(f'expected bundle probe.{key}={expected_value!r} in {bundle_dir}, found {probe_meta.get(key)!r}')
        if actual_value != expected_value:
            raise SystemExit(f'expected bundle probe.{key}={expected_value!r} in {bundle_dir}, found {probe_meta.get(key)!r}')
    status = bundle_meta.get('status') if isinstance(bundle_meta.get('status'), dict) else {}
    if status.get('runtime_executed') is not True:
        raise SystemExit(f'expected runtime_executed=true in bundle manifest for {bundle_dir}')
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
        raise SystemExit(f'bundle ROM SHA does not match current ROM artifact in {bundle_dir}')

    if expected_mode == 'off':
        if session.get('HIRES_CACHE_PATH') or session.get('HIRES_CACHE_SHA256'):
            raise SystemExit(f'expected legacy off adapter cache provenance to be empty in {bundle_dir}, found {session!r}')
        if bundle_meta.get('hires_pack_path') or bundle_meta.get('hires_pack_sha256') not in (None, '', 'missing'):
            raise SystemExit(f'expected legacy off top-level cache provenance to be empty in {bundle_dir}, found {bundle_meta!r}')
        if bundle_inputs.get('hires_pack_path') or bundle_inputs.get('hires_pack_sha256') not in (None, '', 'missing'):
            raise SystemExit(f'expected legacy off bundle cache provenance to be empty in {bundle_dir}, found {bundle_inputs!r}')
    else:
        cache_path_values = [
            ('hires_pack_path', bundle_meta.get('hires_pack_path')),
            ('inputs.hires_pack_path', bundle_inputs.get('hires_pack_path')),
        ]
        cache_sha_values = [
            ('hires_pack_sha256', bundle_meta.get('hires_pack_sha256')),
            ('inputs.hires_pack_sha256', bundle_inputs.get('hires_pack_sha256')),
        ]
        if not any(value for _, value in cache_path_values) or not any(value for _, value in cache_sha_values):
            raise SystemExit(f'expected selected bundle cache path/SHA provenance in {bundle_dir}')
        for label, value in cache_path_values:
            if value and not same_resolved_path(value, cache_path):
                raise SystemExit(f'expected selected bundle {label} {cache_path} in {bundle_dir}, found {value!r}')
        for label, value in cache_sha_values:
            if value and value != expected_cache_sha:
                raise SystemExit(f'expected selected bundle {label} {expected_cache_sha!r} in {bundle_dir}, found {value!r}')
        if not same_resolved_path(session.get('HIRES_CACHE_PATH'), cache_path):
            raise SystemExit(f'expected selected adapter cache path {cache_path} in {bundle_dir}, found {session.get("HIRES_CACHE_PATH")!r}')
        if session.get('HIRES_CACHE_SHA256') != expected_cache_sha:
            raise SystemExit(
                f'expected selected adapter cache SHA {expected_cache_sha!r} in {bundle_dir}, '
                f'found {session.get("HIRES_CACHE_SHA256")!r}'
            )
    core_path = session.get('CORE_PATH')
    core_sha = session.get('CORE_SHA256')
    if not core_path or not core_sha:
        raise SystemExit(f'expected adapter core path/SHA provenance in {bundle_dir}')
    if not Path(core_path).is_file() or sha256_file(Path(core_path)) != core_sha:
        raise SystemExit(f'adapter CORE_SHA256 does not match current core artifact in {bundle_dir}')
    require_adapter_config_provenance(bundle_dir, session)
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

def require_selected_provider_evidence(selected_hires, expected_cache_sha: str):
    if selected_hires.get('available') is not True:
        raise SystemExit('expected selected hi-res evidence to be available')
    if selected_hires.get('cache_loaded') is not True:
        raise SystemExit('expected selected hi-res cache to be loaded')
    if not same_resolved_path(selected_hires.get('cache_path'), cache_path):
        raise SystemExit(f'expected selected hi-res cache path {cache_path}, found {selected_hires.get("cache_path")!r}')
    if selected_hires.get('cache_sha256') != expected_cache_sha:
        raise SystemExit(f'expected selected hi-res cache SHA {expected_cache_sha!r}, found {selected_hires.get("cache_sha256")!r}')
    summary = selected_hires.get('summary') or {}
    if summary.get('provider') != 'on':
        raise SystemExit(f'expected selected provider=on, found {summary.get("provider")!r}')
    if summary.get('source_mode') != 'phrb-only':
        raise SystemExit(f'expected selected source_mode=phrb-only, found {summary.get("source_mode")!r}')
    if int(summary.get('compat_entry_count') or 0) != 0:
        raise SystemExit(f'expected selected compat_entry_count=0, found {summary.get("compat_entry_count")!r}')
    if int(summary.get('native_sampled_entry_count') or 0) <= 0:
        raise SystemExit(f'expected selected native sampled entries, found {summary.get("native_sampled_entry_count")!r}')
    if int((summary.get('source_counts') or {}).get('phrb') or 0) <= 0:
        raise SystemExit(f'expected selected PHRB-backed entries, found {summary.get("source_counts")!r}')
    for key, value in (summary.get('source_counts') or {}).items():
        if key != 'phrb' and int(value or 0) != 0:
            raise SystemExit(f'expected selected source_counts.{key}=0, found {value!r}')
    descriptor_paths = summary.get('descriptor_path_counts') or {}
    if int(descriptor_paths.get('sampled') or 0) <= 0:
        raise SystemExit(f'expected selected sampled descriptor path evidence, found {descriptor_paths!r}')
    for key in ('native_checksum', 'generic', 'compat'):
        if int(descriptor_paths.get(key) or 0) != 0:
            raise SystemExit(f'expected selected descriptor_paths.{key}=0, found {descriptor_paths.get(key)!r}')
    sampled_probe = selected_hires.get('sampled_object_probe') or {}
    if sampled_probe.get('available') is not True:
        raise SystemExit('expected selected sampled-object probe to be available')
    if int(sampled_probe.get('line_count') or 0) <= 0:
        raise SystemExit(f'expected selected sampled-object probe line_count > 0, found {sampled_probe.get("line_count")!r}')
    for key in ('exact_hit_count', 'exact_miss_count', 'exact_conflict_miss_count', 'exact_unresolved_miss_count'):
        if sampled_probe.get(key) is None:
            raise SystemExit(f'expected selected sampled-object probe field {key}')

def require_legacy_off_evidence(legacy_hires):
    summary = legacy_hires.get('summary') or {}
    if (
        legacy_hires.get('available') is not False
        or legacy_hires.get('cache_loaded') is not False
        or legacy_hires.get('cache_path')
        or legacy_hires.get('cache_sha256')
        or summary.get('provider') not in (None, 'off')
        or int(summary.get('entry_count') or 0) != 0
        or int(summary.get('native_sampled_entry_count') or 0) != 0
        or int(summary.get('compat_entry_count') or 0) != 0
    ):
        raise SystemExit(f'expected legacy bundle hi-res evidence to stay disabled, found {legacy_hires!r}')

legacy_capture = one_capture(legacy_dir)
selected_capture = one_capture(selected_dir)

legacy_img = Image.open(legacy_capture).convert('RGBA')
selected_img = Image.open(selected_capture).convert('RGBA')
diff = ImageChops.difference(legacy_img, selected_img)
hist = diff.histogram()
total_abs = sum((i % 256) * v for i, v in enumerate(hist))
total_sq = sum(((i % 256) ** 2) * v for i, v in enumerate(hist))
count = legacy_img.size[0] * legacy_img.size[1] * 4
cache_sha256 = sha256_file(cache_path)
legacy_hires = json.loads((legacy_dir / 'traces' / 'hires-evidence.json').read_text())
selected_hires = json.loads((selected_dir / 'traces' / 'hires-evidence.json').read_text())
require_legacy_off_evidence(legacy_hires)
require_adapter_session_provenance(legacy_dir, 'off', None)
require_selected_provider_evidence(selected_hires, cache_sha256)
require_adapter_session_provenance(selected_dir, 'on', cache_sha256)

legacy_semantic = json.loads((legacy_dir / 'traces' / 'paper-mario-game-status.json').read_text())
selected_semantic = json.loads((selected_dir / 'traces' / 'paper-mario-game-status.json').read_text())
legacy_pm = legacy_semantic.get('paper_mario_us') or {}
selected_pm = selected_semantic.get('paper_mario_us') or {}
for label, pm in (('legacy', legacy_pm), ('selected', selected_pm)):
    if (
        pm.get('cur_game_mode', {}).get('init_symbol') != 'state_init_file_select'
        or pm.get('cur_game_mode', {}).get('step_symbol') != 'state_step_file_select'
    ):
        raise SystemExit(f'expected {label} run to remain in file-select state, found {pm!r}')
if legacy_semantic != selected_semantic:
    raise SystemExit('expected selected-package file-select semantic trace to match feature-off legacy trace')

summary = {
    'cache_path': str(cache_path),
    'cache_sha256': cache_sha256,
    'all_passed': True,
    'legacy_bundle': str(legacy_dir),
    'selected_bundle': str(selected_dir),
    'ae': total_abs,
    'rmse': math.sqrt(total_sq / count),
    'legacy_semantic': legacy_semantic,
    'selected_semantic': selected_semantic,
    'legacy_hires': legacy_hires,
    'selected_hires': selected_hires,
  }

summary_path = bundle_root / 'validation-summary.json'
summary_path.write_text(json.dumps(summary, indent=2) + '\n')

legacy_pm = summary['legacy_semantic'].get('paper_mario_us', {})
selected_pm = summary['selected_semantic'].get('paper_mario_us', {})
selected_probe = summary['selected_hires'].get('sampled_object_probe', {})

md = [
    '# File-Select Selected-Package Validation',
    '',
    f'- Cache: `{cache_path}`',
    f'- Cache SHA-256: `{summary["cache_sha256"]}`',
    f'- Legacy bundle: [{legacy_dir.name}]({legacy_dir})',
    f'- Selected bundle: [{selected_dir.name}]({selected_dir})',
    f'- AE: `{summary["ae"]}`',
    f'- RMSE: `{summary["rmse"]}`',
    f'- Legacy semantic: `{legacy_pm.get("cur_game_mode", {}).get("init_symbol")}` / `{legacy_pm.get("cur_game_mode", {}).get("step_symbol")}`',
    f'- Selected semantic: `{selected_pm.get("cur_game_mode", {}).get("init_symbol")}` / `{selected_pm.get("cur_game_mode", {}).get("step_symbol")}`',
]
if 'sampled_object_probe' in summary['selected_hires']:
    md.extend([
        f'- Selected exact hits: `{selected_probe.get("exact_hit_count")}`',
        f'- Selected conflict misses: `{selected_probe.get("exact_conflict_miss_count")}`',
        f'- Selected unresolved misses: `{selected_probe.get("exact_unresolved_miss_count")}`',
        '',
    ])
else:
    md.extend([
        '- Selected exact hits: `n/a`',
        '- Selected conflict misses: `n/a`',
        '- Selected unresolved misses: `n/a`',
        '',
    ])

(bundle_root / 'validation-summary.md').write_text('\n'.join(md) + '\n')
print(summary_path)
print(bundle_root / 'validation-summary.md')
PY

echo "[validation] complete: $BUNDLE_ROOT"
