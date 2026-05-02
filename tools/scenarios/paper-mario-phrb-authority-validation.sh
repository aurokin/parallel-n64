#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "$SCRIPT_DIR/../.." && pwd)"
source "$SCRIPT_DIR/lib/common.sh"

CACHE_PATH="${PARALLEL_RDP_HIRES_CACHE_PATH:-}"
BUNDLE_ROOT=""
RUN_PROBES=1
SUMMARY_TITLE="Paper Mario PHRB Authority Validation"
EXPECTED_SOURCE_MODE="phrb-only"
MIN_NATIVE_SAMPLED_COUNT=0
ALLOW_COMPAT_DESCRIPTOR_TRAFFIC=0
EXPECTED_ROM_PATH="${PAPER_MARIO_EXPECTED_ROM_PATH:-$REPO_ROOT/assets/Paper Mario (USA).zip}"

usage() {
  cat <<'USAGE'
Usage:
  tools/scenarios/paper-mario-phrb-authority-validation.sh [options]

Options:
  --cache-path PATH              PHRB package to validate (defaults to env PARALLEL_RDP_HIRES_CACHE_PATH)
  --bundle-root PATH             Root directory for emitted authority bundles
  --reuse                        Reuse existing bundles instead of rerunning probes
  --summary-title TEXT           Markdown title for the validation summary
  --expected-source-mode MODE    Expected hi-res summary source_mode (default: phrb-only)
  --min-native-sampled-count N   Minimum native sampled entry count required per fixture (default: 0)
  --allow-compat-descriptor-traffic
                                 Allow compat descriptor traffic in provider-owned evidence.
                                 Use only for enriched full-cache validation, not selected packages.
  --expected-rom-path PATH       Expected Paper Mario ROM artifact for reused bundle provenance
                                 (default: assets/Paper Mario (USA).zip)
  -h, --help                     Show this help
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
    --reuse)
      RUN_PROBES=0
      ;;
    --summary-title)
      shift
      SUMMARY_TITLE="${1:-}"
      ;;
    --expected-source-mode)
      shift
      EXPECTED_SOURCE_MODE="${1:-}"
      ;;
    --min-native-sampled-count)
      shift
      MIN_NATIVE_SAMPLED_COUNT="${1:-}"
      ;;
    --allow-compat-descriptor-traffic)
      ALLOW_COMPAT_DESCRIPTOR_TRAFFIC=1
      ;;
    --expected-rom-path)
      shift
      EXPECTED_ROM_PATH="${1:-}"
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
  echo "PHRB package not found: $CACHE_PATH" >&2
  exit 2
fi
if [[ ! -f "$EXPECTED_ROM_PATH" ]]; then
  echo "Expected ROM not found: $EXPECTED_ROM_PATH" >&2
  exit 2
fi
if ! scenario_require_phrb_runtime_cache "$CACHE_PATH"; then
  exit 2
fi

if ! [[ "$MIN_NATIVE_SAMPLED_COUNT" =~ ^[0-9]+$ ]]; then
  echo "--min-native-sampled-count must be a non-negative integer." >&2
  exit 2
fi

if [[ -z "$BUNDLE_ROOT" ]]; then
  BUNDLE_ROOT="$REPO_ROOT/artifacts/paper-mario-probes/validation/$(date +"%Y%m%d-%H%M%S")-paper-mario-phrb-authorities"
fi

declare -a FIXTURES=(
  "title-screen|tools/scenarios/paper-mario-title-screen.sh"
  "file-select|tools/scenarios/paper-mario-file-select.sh"
  "kmr-03-entry-5|tools/scenarios/paper-mario-kmr-03-entry-5.sh"
)

mkdir -p "$BUNDLE_ROOT"

HIRES_ENV_UNSET=(
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

for fixture in "${FIXTURES[@]}"; do
  IFS='|' read -r label scenario_path <<<"$fixture"
  bundle_dir="$BUNDLE_ROOT/$label"

  if (( RUN_PROBES )); then
    mkdir -p "$bundle_dir"
    runtime_env_source="$REPO_ROOT/${scenario_path%.sh}.runtime.env"
    if [[ ! -f "$runtime_env_source" ]]; then
      echo "Missing runtime env for $scenario_path at $runtime_env_source" >&2
      exit 1
    fi
    runtime_env_override="$bundle_dir/runtime.override.env"
    cat > "$runtime_env_override" <<EOF
source "$runtime_env_source"
EXPECTED_HIRES_SUMMARY_SOURCE_MODE_ON="$EXPECTED_SOURCE_MODE"
EXPECTED_HIRES_MIN_SUMMARY_ENTRY_COUNT_ON="1"
EXPECTED_HIRES_MIN_SUMMARY_NATIVE_SAMPLED_ENTRY_COUNT_ON="$MIN_NATIVE_SAMPLED_COUNT"
EXPECTED_HIRES_MIN_SUMMARY_SOURCE_PHRB_COUNT_ON="1"
EOF

    env "${HIRES_ENV_UNSET[@]}" \
    RUNTIME_ENV_OVERRIDE="$runtime_env_override" \
    PARALLEL_RDP_HIRES_CACHE_PATH="$CACHE_PATH" \
    DISABLE_SCREENSHOT_VERIFY=1 \
    "$REPO_ROOT/$scenario_path" \
      --mode on \
      --authority-mode authoritative \
      --bundle-dir "$bundle_dir" \
      --run
  fi

  if [[ ! -f "$bundle_dir/traces/fixture-verification.json" ]]; then
    echo "Missing fixture verification output: $bundle_dir/traces/fixture-verification.json" >&2
    exit 1
  fi
done

python3 - "$CACHE_PATH" "$BUNDLE_ROOT" "$SUMMARY_TITLE" "$EXPECTED_SOURCE_MODE" "$MIN_NATIVE_SAMPLED_COUNT" "$ALLOW_COMPAT_DESCRIPTOR_TRAFFIC" "$EXPECTED_ROM_PATH" <<'PY'
import hashlib
import json
import sys
from pathlib import Path

cache_path = Path(sys.argv[1])
bundle_root = Path(sys.argv[2])
summary_title = sys.argv[3]
expected_source_mode = sys.argv[4]
min_native_sampled_count = int(sys.argv[5])
allow_compat_descriptor_traffic = sys.argv[6] == "1"
expected_rom_path = Path(sys.argv[7])

def sha256_file(path: Path):
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()

expected_cache_sha256 = sha256_file(cache_path)
expected_rom_sha256 = sha256_file(expected_rom_path)

fixtures = [
    ("title-screen", "paper-mario-title-screen"),
    ("file-select", "paper-mario-file-select"),
    ("kmr-03-entry-5", "paper-mario-kmr-03-entry-5"),
]

summary = {
    "cache_path": str(cache_path),
    "cache_sha256": expected_cache_sha256,
    "summary_title": summary_title,
    "expected_source_mode": expected_source_mode,
    "min_native_sampled_count": min_native_sampled_count,
    "all_passed": True,
    "fixtures": [],
}

def to_int(value, default=-1):
    try:
        return int(value)
    except (TypeError, ValueError):
        return default

def same_resolved_path(actual, expected):
    if actual in (None, ""):
        return False
    try:
        return Path(actual).resolve() == Path(expected).resolve()
    except OSError:
        return str(actual) == str(expected)

def read_env_file(path):
    values = {}
    if not path.is_file():
        return None
    for line in path.read_text().splitlines():
        if not line or line.startswith("#") or "=" not in line:
            continue
        key, value = line.split("=", 1)
        values[key] = value
    return values

def command_log_signature(bundle_dir):
    command_log = bundle_dir / "retroarch.executed.commands.log"
    if not command_log.is_file():
        return None
    return hashlib.sha256(command_log.read_bytes()).hexdigest()

def expected_command_log_signature(bundle_dir):
    command_log = bundle_dir / "retroarch.expected.commands.log"
    if not command_log.is_file():
        return None
    return hashlib.sha256(command_log.read_bytes()).hexdigest()

def path_within(path: Path, root: Path) -> bool:
    try:
        path.resolve().relative_to(root.resolve())
        return True
    except ValueError:
        return False

def require_adapter_config_provenance(bundle_dir, session):
    failures = []
    for key in ("BASE_CONFIG", "BASE_CONFIG_SHA256", "APPEND_CONFIG", "APPEND_CONFIG_SHA256", "CORE_OPTIONS_FILE", "CORE_OPTIONS_FILE_SHA256"):
        if not session.get(key):
            failures.append(f"Missing adapter config provenance {key}.")
    if failures:
        return failures
    base_config = Path(session["BASE_CONFIG"])
    append_config = Path(session["APPEND_CONFIG"])
    core_options = Path(session["CORE_OPTIONS_FILE"])
    if not base_config.is_file():
        failures.append(f"Adapter BASE_CONFIG does not exist: {base_config!s}.")
    elif sha256_file(base_config) != session["BASE_CONFIG_SHA256"]:
        failures.append("Adapter BASE_CONFIG_SHA256 does not match the current config artifact.")
    for path, sha_key, label in (
        (append_config, "APPEND_CONFIG_SHA256", "append config"),
        (core_options, "CORE_OPTIONS_FILE_SHA256", "core options"),
    ):
        if not path.is_file():
            failures.append(f"Adapter {label} snapshot does not exist: {path!s}.")
        elif not path_within(path, bundle_dir):
            failures.append(f"Adapter {label} snapshot is not bundle-local: {path!s}.")
        elif sha256_file(path) != session[sha_key]:
            failures.append(f"Adapter {sha_key} does not match the current snapshot.")
    return failures

def require_adapter_session_provenance(bundle_dir, bundle_meta, expected_mode):
    failures = []
    session = read_env_file(bundle_dir / "retroarch.session.env")
    run = read_env_file(bundle_dir / "retroarch.run.env")
    if session is None:
        failures.append(f"Missing adapter session provenance: {bundle_dir / 'retroarch.session.env'}.")
        return failures
    if run is None:
        failures.append(f"Missing adapter run status provenance: {bundle_dir / 'retroarch.run.env'}.")
        return failures
    if run.get("RUNTIME_EXECUTED") != "1":
        failures.append(f"Expected adapter RUNTIME_EXECUTED=1, got {run.get('RUNTIME_EXECUTED')!r}.")
    if run.get("FORCED_TERMINATION") != "0":
        failures.append(f"Expected adapter FORCED_TERMINATION=0, got {run.get('FORCED_TERMINATION')!r}.")
    if run.get("RETROARCH_EXIT_STATUS") != "0":
        failures.append(f"Expected adapter RETROARCH_EXIT_STATUS=0, got {run.get('RETROARCH_EXIT_STATUS')!r}.")
    if session.get("MODE") != expected_mode:
        failures.append(f"Expected adapter MODE={expected_mode!r}, got {session.get('MODE')!r}.")

    bundle_inputs = bundle_meta.get("inputs") if isinstance(bundle_meta.get("inputs"), dict) else {}
    bundle_rom_path = bundle_inputs.get("rom_path")
    bundle_rom_sha = bundle_inputs.get("rom_sha256")
    if not bundle_rom_path:
        failures.append("Missing bundle ROM path provenance.")
    else:
        if not same_resolved_path(bundle_rom_path, expected_rom_path):
            failures.append(f"Expected bundle ROM path {str(expected_rom_path)!r}, got {bundle_rom_path!r}.")
        if not same_resolved_path(session.get("ROM_PATH"), Path(bundle_rom_path)):
            failures.append(f"Expected adapter ROM_PATH={bundle_rom_path!r}, got {session.get('ROM_PATH')!r}.")
    if not bundle_rom_sha:
        failures.append("Missing bundle ROM SHA provenance.")
    else:
        if bundle_rom_sha != expected_rom_sha256:
            failures.append(f"Expected bundle ROM SHA {expected_rom_sha256!r}, got {bundle_rom_sha!r}.")
        if session.get("ROM_SHA256") != bundle_rom_sha:
            failures.append(f"Expected adapter ROM_SHA256={bundle_rom_sha!r}, got {session.get('ROM_SHA256')!r}.")
    if bundle_rom_path and not Path(bundle_rom_path).is_file():
        failures.append(f"Bundle ROM artifact is missing: {bundle_rom_path!r}.")
    elif bundle_rom_path and sha256_file(Path(bundle_rom_path)) != session.get("ROM_SHA256"):
        failures.append("Adapter ROM_SHA256 does not match the current ROM artifact.")

    core_path = session.get("CORE_PATH")
    core_sha = session.get("CORE_SHA256")
    if not core_path or not core_sha:
        failures.append("Missing adapter core path/SHA provenance.")
    elif not Path(core_path).is_file():
        failures.append(f"Adapter CORE_PATH does not exist: {core_path!r}.")
    elif sha256_file(Path(core_path)) != core_sha:
        failures.append("Adapter CORE_SHA256 does not match the current core artifact.")
    failures.extend(require_adapter_config_provenance(bundle_dir, session))

    if not same_resolved_path(session.get("HIRES_CACHE_PATH"), cache_path):
        failures.append(f"Expected adapter HIRES_CACHE_PATH={str(cache_path)!r}, got {session.get('HIRES_CACHE_PATH')!r}.")
    if session.get("HIRES_CACHE_SHA256") != expected_cache_sha256:
        failures.append(
            f"Expected adapter HIRES_CACHE_SHA256={expected_cache_sha256!r}, "
            f"got {session.get('HIRES_CACHE_SHA256')!r}."
        )
    expected_command_signature = command_log_signature(bundle_dir)
    if not expected_command_signature:
        failures.append(f"Missing adapter command log: {bundle_dir / 'logs' / 'retroarch.commands.log'}.")
    elif session.get("COMMAND_SIGNATURE") != expected_command_signature:
        failures.append(
            f"Expected adapter COMMAND_SIGNATURE={expected_command_signature!r}, "
            f"got {session.get('COMMAND_SIGNATURE')!r}."
        )
    planned_command_signature = expected_command_log_signature(bundle_dir)
    if not planned_command_signature:
        failures.append(f"Missing adapter expected command log: {bundle_dir / 'retroarch.expected.commands.log'}.")
    elif planned_command_signature != expected_command_signature:
        failures.append(
            f"Expected executed adapter commands to match planned command signature {planned_command_signature!r}, "
            f"got {expected_command_signature!r}."
        )
    return failures

def require_reuse_provenance(bundle_dir, verification, expected_fixture_id, hires_evidence):
    failures = []
    actual_fixture_id = verification.get("fixture_id")
    if actual_fixture_id != expected_fixture_id:
        failures.append(f"Expected fixture_id={expected_fixture_id!r}, got {actual_fixture_id!r}.")
    bundle_meta_path = bundle_dir / "bundle.json"
    if not bundle_meta_path.is_file():
        failures.append(f"Missing bundle provenance manifest: {bundle_meta_path}.")
        return failures
    try:
        bundle_meta = json.loads(bundle_meta_path.read_text())
    except json.JSONDecodeError as exc:
        failures.append(f"Bundle provenance manifest is not valid JSON: {bundle_meta_path}: {exc}.")
        return failures
    if bundle_meta.get("fixture_id") != expected_fixture_id:
        failures.append(f"Expected bundle fixture_id={expected_fixture_id!r}, got {bundle_meta.get('fixture_id')!r}.")
    if bundle_meta.get("mode") != "on":
        failures.append(f"Expected bundle mode='on', got {bundle_meta.get('mode')!r}.")
    status = bundle_meta.get("status") if isinstance(bundle_meta.get("status"), dict) else {}
    if status.get("runtime_executed") is not True:
        failures.append(f"Expected bundle status.runtime_executed=true, got {status.get('runtime_executed')!r}.")
    fixture_authority = bundle_meta.get("fixture_authority") if isinstance(bundle_meta.get("fixture_authority"), dict) else {}
    if fixture_authority.get("authority_mode_used") != "authoritative":
        failures.append(
            f"Expected bundle fixture_authority.authority_mode_used='authoritative', "
            f"got {fixture_authority.get('authority_mode_used')!r}."
        )
    bundle_inputs = bundle_meta.get("inputs") if isinstance(bundle_meta.get("inputs"), dict) else {}
    cache_path_values = [
        ("hires_pack_path", bundle_meta.get("hires_pack_path")),
        ("inputs.hires_pack_path", bundle_inputs.get("hires_pack_path")),
    ]
    cache_sha_values = [
        ("hires_pack_sha256", bundle_meta.get("hires_pack_sha256")),
        ("inputs.hires_pack_sha256", bundle_inputs.get("hires_pack_sha256")),
    ]
    if not any(value for _, value in cache_path_values):
        failures.append("Missing bundle hires_pack_path provenance.")
    if not any(value for _, value in cache_sha_values):
        failures.append("Missing bundle hires_pack_sha256 provenance.")
    for label, value in cache_path_values:
        if value and not same_resolved_path(value, cache_path):
            failures.append(f"Expected bundle {label}={str(cache_path)!r}, got {value!r}.")
    for label, value in cache_sha_values:
        if value and value != expected_cache_sha256:
            failures.append(f"Expected bundle {label}={expected_cache_sha256}, got {value!r}.")
    failures.extend(require_adapter_session_provenance(bundle_dir, bundle_meta, "on"))
    evidence_cache_path = hires_evidence.get("cache_path") if isinstance(hires_evidence, dict) else None
    if not evidence_cache_path:
        failures.append("Missing hi-res evidence cache_path provenance.")
    elif not same_resolved_path(evidence_cache_path, cache_path):
        failures.append(f"Expected hi-res evidence cache_path={str(cache_path)!r}, got {evidence_cache_path!r}.")
    evidence_cache_sha = hires_evidence.get("cache_sha256") if isinstance(hires_evidence, dict) else None
    if not evidence_cache_sha:
        failures.append("Missing hi-res evidence cache_sha256 provenance.")
    elif evidence_cache_sha != expected_cache_sha256:
        failures.append(f"Expected hi-res evidence cache_sha256={expected_cache_sha256}, got {evidence_cache_sha!r}.")
    return failures

def require_provider_owned_evidence(hires_evidence, hires_evidence_path):
    failures = []
    if not hires_evidence:
        return [f"Missing provider-owned hi-res evidence: {hires_evidence_path}."]
    evidence_summary = hires_evidence.get("summary") or {}
    if hires_evidence.get("available") is not True:
        failures.append("Hi-res evidence is not marked available.")
    if hires_evidence.get("cache_loaded") is not True:
        failures.append("Hi-res evidence does not report a loaded cache.")
    if evidence_summary.get("provider") != "on":
        failures.append(f"Expected hi-res evidence provider='on', got {evidence_summary.get('provider')!r}.")
    if evidence_summary.get("source_mode") != expected_source_mode:
        failures.append(
            f"Expected hi-res evidence source_mode={expected_source_mode!r}, "
            f"got {evidence_summary.get('source_mode')!r}."
        )
    if not allow_compat_descriptor_traffic and to_int(evidence_summary.get("compat_entry_count"), 0) != 0:
        failures.append(
            f"Expected hi-res evidence compat_entry_count=0, got {evidence_summary.get('compat_entry_count')!r}."
        )
    if to_int(evidence_summary.get("native_sampled_entry_count")) < min_native_sampled_count:
        failures.append(
            f"Expected hi-res evidence native sampled count >= {min_native_sampled_count}, "
            f"got {evidence_summary.get('native_sampled_entry_count')!r}."
        )
    if to_int((evidence_summary.get("source_counts") or {}).get("phrb")) < 1:
        failures.append(
            f"Expected hi-res evidence source_counts.phrb >= 1, "
            f"got {(evidence_summary.get('source_counts') or {}).get('phrb')!r}."
        )
    for source_key, source_count in (evidence_summary.get("source_counts") or {}).items():
        if source_key != "phrb" and to_int(source_count) != 0:
            failures.append(f"Expected hi-res evidence source_counts.{source_key}=0, got {source_count!r}.")
    descriptor_paths = evidence_summary.get("descriptor_path_counts") or {}
    if to_int(descriptor_paths.get("sampled"), 0) <= 0:
        failures.append(f"Expected sampled descriptor path evidence, got {descriptor_paths!r}.")
    forbidden_descriptor_keys = ["native_checksum", "generic"]
    if not allow_compat_descriptor_traffic:
        forbidden_descriptor_keys.append("compat")
    for key in forbidden_descriptor_keys:
        if to_int(descriptor_paths.get(key), 0) != 0:
            failures.append(f"Expected zero {key} descriptor traffic, got {descriptor_paths.get(key)!r}.")
    sampled_probe = hires_evidence.get("sampled_object_probe") or {}
    if sampled_probe.get("available") is not True:
        failures.append(f"Expected sampled-object probe to be available, got {sampled_probe.get('available')!r}.")
    if to_int(sampled_probe.get("line_count"), 0) <= 0:
        failures.append(f"Expected sampled-object probe line_count > 0, got {sampled_probe.get('line_count')!r}.")
    if sampled_probe.get("exact_miss_count") is None:
        conflict = sampled_probe.get("exact_conflict_miss_count")
        unresolved = sampled_probe.get("exact_unresolved_miss_count")
        if conflict is not None and unresolved is not None:
            sampled_probe["exact_miss_count"] = to_int(conflict, 0) + to_int(unresolved, 0)
    for key in ("exact_hit_count", "exact_miss_count", "exact_conflict_miss_count", "exact_unresolved_miss_count"):
        if sampled_probe.get(key) is None:
            failures.append(f"Missing sampled-object probe field {key}.")
    return failures

for label, fixture_id in fixtures:
    bundle_dir = bundle_root / label
    verification_path = bundle_dir / "traces" / "fixture-verification.json"
    hires_evidence_path = bundle_dir / "traces" / "hires-evidence.json"
    verification = json.loads(verification_path.read_text())
    hires_evidence = json.loads(hires_evidence_path.read_text()) if hires_evidence_path.exists() else {}
    actual = verification.get("actual") or {}
    failures = list(verification.get("failures") or [])
    failures.extend(require_reuse_provenance(bundle_dir, verification, fixture_id, hires_evidence))
    failures.extend(require_provider_owned_evidence(hires_evidence, hires_evidence_path))
    source_mode = actual.get("hires_summary_source_mode")
    native_sampled_entry_count = actual.get("hires_summary_native_sampled_entry_count")
    source_phrb_count = actual.get("hires_summary_source_phrb_count")
    native_sampled_entry_count_int = to_int(native_sampled_entry_count)
    source_phrb_count_int = to_int(source_phrb_count)
    if source_mode != expected_source_mode:
        failures.append(f"Expected source_mode={expected_source_mode!r}, got {source_mode!r}.")
    if native_sampled_entry_count_int < min_native_sampled_count:
        failures.append(
            f"Expected at least {min_native_sampled_count} native sampled entries, "
            f"got {native_sampled_entry_count!r}."
        )
    if source_phrb_count_int < 1:
        failures.append(f"Expected at least one source PHRB entry, got {source_phrb_count!r}.")
    fixture_passed = bool(verification.get("passed")) and not failures
    evidence_sampled_probe = hires_evidence.get("sampled_object_probe") or {}
    exact_conflict_miss_count = actual.get("hires_exact_conflict_miss_count")
    if exact_conflict_miss_count is None:
        exact_conflict_miss_count = evidence_sampled_probe.get("exact_conflict_miss_count")
    exact_unresolved_miss_count = actual.get("hires_exact_unresolved_miss_count")
    if exact_unresolved_miss_count is None:
        exact_unresolved_miss_count = evidence_sampled_probe.get("exact_unresolved_miss_count")
    exact_hit_count = actual.get("hires_exact_hit_count")
    if exact_hit_count is None:
        exact_hit_count = evidence_sampled_probe.get("exact_hit_count")
    exact_miss_count = evidence_sampled_probe.get("exact_miss_count")
    if exact_miss_count is None and exact_conflict_miss_count is not None and exact_unresolved_miss_count is not None:
        exact_miss_count = to_int(exact_conflict_miss_count, 0) + to_int(exact_unresolved_miss_count, 0)
    fixture_summary = {
        "label": label,
        "fixture_id": fixture_id,
        "bundle_dir": str(bundle_dir),
        "passed": fixture_passed,
        "screenshot_sha256": (verification.get("checks") or {}).get("screenshot_sha256"),
        "capture_path": actual.get("capture_path"),
        "init_symbol": actual.get("init_symbol"),
        "step_symbol": actual.get("step_symbol"),
        "hires_summary": {
            "provider": actual.get("hires_summary_provider"),
            "source_mode": source_mode,
            "entry_count": actual.get("hires_summary_entry_count"),
            "native_sampled_entry_count": native_sampled_entry_count,
            "compat_entry_count": actual.get("hires_summary_compat_entry_count"),
            "entry_class": actual.get("hires_summary_entry_class") or ((hires_evidence.get("summary") or {}).get("entry_class")),
            "source_phrb_count": source_phrb_count,
            "descriptor_path_counts": ((hires_evidence.get("summary") or {}).get("descriptor_path_counts") or {}),
            "descriptor_path_class": actual.get("hires_summary_descriptor_path_class") or ((hires_evidence.get("summary") or {}).get("descriptor_path_class")),
            "descriptor_path_detail_counts": ((hires_evidence.get("summary") or {}).get("descriptor_path_detail_counts") or {}),
            "resolution_reason_counts": ((hires_evidence.get("summary") or {}).get("resolution_reason_counts") or {}),
        },
        "sampled_object_probe": {
            "exact_hit_count": exact_hit_count,
            "exact_miss_count": exact_miss_count,
            "exact_conflict_miss_count": exact_conflict_miss_count,
            "exact_unresolved_miss_count": exact_unresolved_miss_count,
        },
        "failures": failures,
    }
    summary["fixtures"].append(fixture_summary)
    if not fixture_summary["passed"]:
        summary["all_passed"] = False

summary_path = bundle_root / "validation-summary.json"
summary_path.write_text(json.dumps(summary, indent=2) + "\n")

md = [
    f"# {summary_title}",
    "",
    f"- Cache: `{cache_path}`",
    f"- Cache SHA-256: `{summary['cache_sha256']}`",
    f"- Expected source mode: `{expected_source_mode}`",
    f"- Minimum native sampled count: `{min_native_sampled_count}`",
    f"- All passed: `{str(summary['all_passed']).lower()}`",
    "",
]

for fixture in summary["fixtures"]:
    hires = fixture["hires_summary"]
    probe = fixture["sampled_object_probe"]
    descriptor_paths = hires.get("descriptor_path_counts") or {}
    descriptor_detail = hires.get("descriptor_path_detail_counts") or {}
    resolution_reasons = hires.get("resolution_reason_counts") or {}
    md.extend([
        f"## {fixture['label']}",
        f"- Bundle: [{Path(fixture['bundle_dir']).name}]({fixture['bundle_dir']})",
        f"- Passed: `{str(fixture['passed']).lower()}`",
        f"- Screenshot hash (artifact identity only): `{fixture['screenshot_sha256']}`",
        f"- Semantic: `{fixture['init_symbol']}` / `{fixture['step_symbol']}`",
        f"- Hi-res summary: provider `{hires.get('provider')}`, source mode `{hires.get('source_mode')}`, entries `{hires.get('entry_count')}`, native sampled `{hires.get('native_sampled_entry_count')}`, compat entries `{hires.get('compat_entry_count')}`, entry class `{hires.get('entry_class')}`, source PHRB `{hires.get('source_phrb_count')}`",
        f"- Descriptor paths: sampled `{descriptor_paths.get('sampled', 0)}`, native checksum `{descriptor_paths.get('native_checksum', 0)}`, generic `{descriptor_paths.get('generic', 0)}`, compat `{descriptor_paths.get('compat', 0)}`, class `{hires.get('descriptor_path_class')}`",
        f"- Sampled exact hits: `{probe.get('exact_hit_count')}`",
        f"- Sampled conflict misses: `{probe.get('exact_conflict_miss_count')}`",
        f"- Sampled unresolved misses: `{probe.get('exact_unresolved_miss_count')}`",
    ])
    if descriptor_detail:
        md.append(
            f"- Sampled detail: family singleton `{descriptor_detail.get('sampled_family_singleton', 0)}`, "
            f"ordered-surface singleton `{descriptor_detail.get('sampled_ordered_surface_singleton', 0)}`, "
            f"exact selector `{descriptor_detail.get('sampled_exact_selector', 0)}`"
        )
        md.append(
            f"- Descriptor detail: native checksum exact `{descriptor_detail.get('native_checksum_exact', 0)}`, "
            f"identity assisted `{descriptor_detail.get('native_checksum_identity_assisted', 0)}`, "
            f"generic fallback `{descriptor_detail.get('native_checksum_generic_fallback', 0)}`, "
            f"generic identity assisted `{descriptor_detail.get('generic_identity_assisted', 0)}`, "
            f"generic plain `{descriptor_detail.get('generic_plain', 0)}`"
        )
    if resolution_reasons:
        formatted_reasons = ", ".join(
            f"`{reason}` x `{count}`"
            for reason, count in list(resolution_reasons.items())[:6]
        )
        md.append(f"- Resolution reasons: {formatted_reasons}")
    if fixture["failures"]:
        md.append(f"- Failures: `{' | '.join(fixture['failures'])}`")
    md.append("")

(bundle_root / "validation-summary.md").write_text("\n".join(md) + "\n")
print(summary_path)
print(bundle_root / "validation-summary.md")
if not summary["all_passed"]:
    raise SystemExit(1)
PY

echo "[validation] complete: $BUNDLE_ROOT"
