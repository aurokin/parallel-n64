#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "$SCRIPT_DIR/../../.." && pwd)"

CACHE_PATH_DEFAULT="$REPO_ROOT/artifacts/hires-pack-review/20260330-selected-plus-timeout-960-v1-add-1b85/package.phrb"

CACHE_PATH="${EMU_RUNTIME_PM64_SELECTED_PHRB:-$CACHE_PATH_DEFAULT}"
BUNDLE_ROOT="${EMU_RUNTIME_PM64_SELECTED_TIMEOUT_BUNDLE_ROOT:-}"
ENFORCE_VISUAL_ENVELOPE="${EMU_RUNTIME_PM64_SELECTED_TIMEOUT_ENFORCE_VISUAL_ENVELOPE:-0}"
MIN_AE="${EMU_RUNTIME_PM64_SELECTED_TIMEOUT_MIN_AE:-0}"
MAX_AE="${EMU_RUNTIME_PM64_SELECTED_TIMEOUT_MAX_AE:-0}"
MIN_RMSE="${EMU_RUNTIME_PM64_SELECTED_TIMEOUT_MIN_RMSE:-0.0}"
MAX_RMSE="${EMU_RUNTIME_PM64_SELECTED_TIMEOUT_MAX_RMSE:-0.0}"
EXPECTED_NATIVE_SAMPLED_ENTRY_COUNT="${EMU_RUNTIME_PM64_SELECTED_TIMEOUT_EXPECTED_NATIVE_SAMPLED_ENTRY_COUNT:-195}"
EXPECTED_SAMPLED_DESCRIPTOR_PATH_COUNT="${EMU_RUNTIME_PM64_SELECTED_TIMEOUT_EXPECTED_SAMPLED_DESCRIPTOR_PATH_COUNT:-66}"

if [[ "${EMU_ENABLE_RUNTIME_CONFORMANCE:-0}" != "1" ]]; then
  echo "SKIP: set EMU_ENABLE_RUNTIME_CONFORMANCE=1 to run Paper Mario selected-package timeout lookup-without-probe conformance."
  exit 77
fi

SCENARIO="$REPO_ROOT/tools/scenarios/paper-mario-title-timeout-probe.sh"
if [[ ! -x "$SCENARIO" ]]; then
  echo "FAIL: timeout probe wrapper is missing or not executable." >&2
  exit 1
fi

if [[ ! -f "$CACHE_PATH" ]]; then
  echo "SKIP: selected Paper Mario PHRB package not found at $CACHE_PATH (set EMU_RUNTIME_PM64_SELECTED_PHRB to override)."
  exit 77
fi

TITLE_ENV="$REPO_ROOT/tools/scenarios/paper-mario-title-screen.runtime.env"
if [[ ! -f "$TITLE_ENV" ]]; then
  echo "SKIP: runtime env missing for paper-mario-title-screen at $TITLE_ENV."
  exit 77
fi

readarray -t prereq_paths < <(
  ENV_PATH="$TITLE_ENV" python3 - <<'PY'
import os
from pathlib import Path

env_path = Path(os.environ["ENV_PATH"])
values = {}
for raw in env_path.read_text().splitlines():
    line = raw.strip()
    if not line or line.startswith("#") or "=" not in line:
        continue
    key, value = line.split("=", 1)
    values[key] = value.strip().strip('"')
for key in ("RETROARCH_BIN", "RETROARCH_BASE_CONFIG", "CORE_PATH", "ROM_PATH", "AUTHORITATIVE_STATE_PATH"):
    print(values.get(key, ""))
PY
)

bin_path="${prereq_paths[0]:-}"
base_cfg="${prereq_paths[1]:-}"
core_path="${prereq_paths[2]:-}"
rom_path="${prereq_paths[3]:-}"
authoritative_state_path="${prereq_paths[4]:-}"

if [[ -z "$bin_path" || ! -x "$bin_path" ]]; then
  echo "SKIP: RetroArch binary missing for title-timeout conformance at $bin_path."
  exit 77
fi
if [[ -z "$base_cfg" || ! -f "$base_cfg" ]]; then
  echo "SKIP: RetroArch config missing for title-timeout conformance at $base_cfg."
  exit 77
fi
if [[ -z "$core_path" || ! -f "$core_path" ]]; then
  echo "SKIP: libretro core missing for title-timeout conformance at $core_path."
  exit 77
fi
if [[ -z "$rom_path" || ! -f "$rom_path" ]]; then
  echo "SKIP: Paper Mario ROM missing for title-timeout conformance at $rom_path."
  exit 77
fi
if [[ -z "$authoritative_state_path" || ! -f "$authoritative_state_path" ]]; then
  echo "SKIP: authoritative state missing for title-timeout conformance at $authoritative_state_path."
  exit 77
fi

cleanup_bundle_root=0
if [[ -z "$BUNDLE_ROOT" ]]; then
  BUNDLE_ROOT="$(mktemp -d)"
  cleanup_bundle_root=1
fi

cleanup() {
  local rc=$?
  if (( cleanup_bundle_root )) && [[ $rc -eq 0 ]]; then
    rm -rf "$BUNDLE_ROOT"
  else
    echo "[conformance] bundle root: $BUNDLE_ROOT"
  fi
  exit "$rc"
}
trap cleanup EXIT

OFF_BUNDLE="$BUNDLE_ROOT/off/timeout-960"
ON_BUNDLE="$BUNDLE_ROOT/on/timeout-960"

env \
  -u RUNTIME_ENV_OVERRIDE \
  -u PARALLEL_N64_GFX_PLUGIN_OVERRIDE \
  -u PARALLEL_RDP_HIRES_BLOCK_SHAPE_PROBE \
  -u PARALLEL_RDP_HIRES_CACHE_PATH \
  -u PARALLEL_RDP_HIRES_CI_COMPAT \
  -u PARALLEL_RDP_HIRES_CI_LOW32_FALLBACK \
  -u PARALLEL_RDP_HIRES_CI_PALETTE_PROBE \
  -u PARALLEL_RDP_HIRES_CI_SELECT \
  -u PARALLEL_RDP_HIRES_DEBUG \
  -u PARALLEL_RDP_HIRES_FILTER_ALLOW_BLOCK \
  -u PARALLEL_RDP_HIRES_FILTER_ALLOW_TILE \
  -u PARALLEL_RDP_HIRES_FILTER_SIGNATURES \
  -u PARALLEL_RDP_HIRES_GLIDEN64_COMPAT_CRC \
  -u PARALLEL_RDP_HIRES_GPU_BUDGET_MB \
  -u PARALLEL_RDP_HIRES_PHRB_DEBUG \
  -u PARALLEL_RDP_HIRES_SAMPLED_OBJECT_LOOKUP \
  -u PARALLEL_RDP_HIRES_SAMPLED_OBJECT_PROBE \
  -u HIRES_FILTER_ALLOW_TILE \
  -u HIRES_FILTER_ALLOW_BLOCK \
  -u HIRES_FILTER_SIGNATURES \
  PARALLEL_N64_GFX_PLUGIN_OVERRIDE=parallel \
  timeout --signal=INT --kill-after=15 600s \
  "$SCENARIO" \
  --mode off \
  --step-frames 960 \
  --step-chunk-frames 960 \
  --probe-label "timeout-960-selected-package-no-probe-off" \
  --bundle-dir "$OFF_BUNDLE" \
  --run

env \
  -u RUNTIME_ENV_OVERRIDE \
  -u PARALLEL_N64_GFX_PLUGIN_OVERRIDE \
  -u PARALLEL_RDP_HIRES_BLOCK_SHAPE_PROBE \
  -u PARALLEL_RDP_HIRES_CI_COMPAT \
  -u PARALLEL_RDP_HIRES_CI_LOW32_FALLBACK \
  -u PARALLEL_RDP_HIRES_CI_PALETTE_PROBE \
  -u PARALLEL_RDP_HIRES_CI_SELECT \
  -u PARALLEL_RDP_HIRES_DEBUG \
  -u PARALLEL_RDP_HIRES_FILTER_ALLOW_BLOCK \
  -u PARALLEL_RDP_HIRES_FILTER_ALLOW_TILE \
  -u PARALLEL_RDP_HIRES_FILTER_SIGNATURES \
  -u PARALLEL_RDP_HIRES_GLIDEN64_COMPAT_CRC \
  -u PARALLEL_RDP_HIRES_GPU_BUDGET_MB \
  -u PARALLEL_RDP_HIRES_PHRB_DEBUG \
  -u PARALLEL_RDP_HIRES_SAMPLED_OBJECT_LOOKUP \
  -u PARALLEL_RDP_HIRES_SAMPLED_OBJECT_PROBE \
  -u HIRES_FILTER_ALLOW_TILE \
  -u HIRES_FILTER_ALLOW_BLOCK \
  -u HIRES_FILTER_SIGNATURES \
  PARALLEL_N64_GFX_PLUGIN_OVERRIDE=parallel \
  PARALLEL_RDP_HIRES_CACHE_PATH="$CACHE_PATH" \
  PARALLEL_RDP_HIRES_SAMPLED_OBJECT_LOOKUP=1 \
  PARALLEL_RDP_HIRES_SAMPLED_OBJECT_PROBE=0 \
  timeout --signal=INT --kill-after=15 600s \
  "$SCENARIO" \
  --mode on \
  --step-frames 960 \
  --step-chunk-frames 960 \
  --probe-label "timeout-960-selected-package-no-probe" \
  --bundle-dir "$ON_BUNDLE" \
  --run

python3 - "$OFF_BUNDLE" "$ON_BUNDLE" "$CACHE_PATH" "$rom_path" "$ENFORCE_VISUAL_ENVELOPE" "$MIN_AE" "$MAX_AE" "$MIN_RMSE" "$MAX_RMSE" "$EXPECTED_NATIVE_SAMPLED_ENTRY_COUNT" "$EXPECTED_SAMPLED_DESCRIPTOR_PATH_COUNT" <<'PY'
import hashlib
import json
import math
import sys
from pathlib import Path
from PIL import Image, ImageChops

off_bundle = Path(sys.argv[1])
on_bundle = Path(sys.argv[2])
cache_path = Path(sys.argv[3])
expected_rom_path = Path(sys.argv[4])
expected_rom_sha256 = None
enforce_visual_envelope = bool(int(sys.argv[5]))
min_ae = int(sys.argv[6])
max_ae = int(sys.argv[7])
min_rmse = float(sys.argv[8])
max_rmse = float(sys.argv[9])
expected_native_sampled_entry_count = int(sys.argv[10])
expected_sampled_descriptor_path_count = int(sys.argv[11])

def sha256_file(path: Path):
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()

def same_resolved_path(actual, expected):
    if not actual:
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
        if not line or line.startswith("#") or "=" not in line:
            continue
        key, value = line.split("=", 1)
        values[key] = value
    return values

expected_rom_sha256 = sha256_file(expected_rom_path)

def command_log_signature(bundle_dir: Path):
    command_log = bundle_dir / "retroarch.executed.commands.log"
    if not command_log.is_file():
        return None
    return hashlib.sha256(command_log.read_bytes()).hexdigest()

def expected_command_log_signature(bundle_dir: Path):
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

def require_adapter_config_provenance(bundle_dir: Path, session):
    for key in ("BASE_CONFIG", "BASE_CONFIG_SHA256", "APPEND_CONFIG", "APPEND_CONFIG_SHA256", "CORE_OPTIONS_FILE", "CORE_OPTIONS_FILE_SHA256"):
        if not session.get(key):
            raise SystemExit(f"FAIL: expected adapter config provenance {key} in {bundle_dir}.")
    base_config = Path(session["BASE_CONFIG"])
    append_config = Path(session["APPEND_CONFIG"])
    core_options = Path(session["CORE_OPTIONS_FILE"])
    if not base_config.is_file():
        raise SystemExit(f"FAIL: expected adapter BASE_CONFIG to exist in {bundle_dir}: {base_config!s}.")
    if sha256_file(base_config) != session["BASE_CONFIG_SHA256"]:
        raise SystemExit(f"FAIL: adapter BASE_CONFIG_SHA256 does not match current artifact in {bundle_dir}.")
    for path, sha_key, label in (
        (append_config, "APPEND_CONFIG_SHA256", "append config"),
        (core_options, "CORE_OPTIONS_FILE_SHA256", "core options"),
    ):
        if not path.is_file():
            raise SystemExit(f"FAIL: expected adapter {label} snapshot to exist in {bundle_dir}: {path!s}.")
        if not path_within(path, bundle_dir):
            raise SystemExit(f"FAIL: expected adapter {label} snapshot to be bundle-local in {bundle_dir}: {path!s}.")
        if sha256_file(path) != session[sha_key]:
            raise SystemExit(f"FAIL: adapter {sha_key} does not match current snapshot in {bundle_dir}.")

def require_adapter_session_provenance(bundle_dir: Path, expected_mode: str, expected_cache_path: Path | None, expected_cache_sha: str | None, expected_step: int):
    session = read_env_file(bundle_dir / "retroarch.session.env")
    run = read_env_file(bundle_dir / "retroarch.run.env")
    if session is None:
        raise SystemExit(f"FAIL: expected adapter session provenance in {bundle_dir}.")
    if run is None:
        raise SystemExit(f"FAIL: expected adapter run status provenance in {bundle_dir}.")
    if run.get("RUNTIME_EXECUTED") != "1":
        raise SystemExit(f"FAIL: expected RUNTIME_EXECUTED=1 in {bundle_dir}, got {run.get('RUNTIME_EXECUTED')!r}.")
    if run.get("FORCED_TERMINATION") != "0":
        raise SystemExit(f"FAIL: expected FORCED_TERMINATION=0 in {bundle_dir}, got {run.get('FORCED_TERMINATION')!r}.")
    if run.get("RETROARCH_EXIT_STATUS") != "0":
        raise SystemExit(f"FAIL: expected RETROARCH_EXIT_STATUS=0 in {bundle_dir}, got {run.get('RETROARCH_EXIT_STATUS')!r}.")
    if session.get("MODE") != expected_mode:
        raise SystemExit(f"FAIL: expected adapter MODE={expected_mode!r} in {bundle_dir}, got {session.get('MODE')!r}.")
    bundle_meta = json.loads((bundle_dir / "bundle.json").read_text())
    if bundle_meta.get("fixture_id") != "paper-mario-title-timeout-probe":
        raise SystemExit(f"FAIL: expected timeout probe fixture_id in {bundle_dir}, got {bundle_meta.get('fixture_id')!r}.")
    if bundle_meta.get("mode") != expected_mode:
        raise SystemExit(f"FAIL: expected bundle mode={expected_mode!r} in {bundle_dir}, got {bundle_meta.get('mode')!r}.")
    status = bundle_meta.get("status") if isinstance(bundle_meta.get("status"), dict) else {}
    if status.get("runtime_executed") is not True:
        raise SystemExit(f"FAIL: expected runtime_executed=true in bundle manifest for {bundle_dir}.")
    probe_meta = bundle_meta.get("probe") if isinstance(bundle_meta.get("probe"), dict) else {}
    if int(probe_meta.get("step_frames") or -1) != int(expected_step):
        raise SystemExit(
            f"FAIL: expected timeout probe step_frames={expected_step} in {bundle_dir}, "
            f"got {probe_meta.get('step_frames')!r}."
        )
    if int(probe_meta.get("step_chunk_frames") or -1) != int(expected_step):
        raise SystemExit(
            f"FAIL: expected timeout probe step_chunk_frames={expected_step} in {bundle_dir}, "
            f"got {probe_meta.get('step_chunk_frames')!r}."
        )
    if probe_meta.get("authority_fixture_id") != "paper-mario-title-screen":
        raise SystemExit(
            f"FAIL: expected timeout probe authority_fixture_id=paper-mario-title-screen in {bundle_dir}, "
            f"got {probe_meta.get('authority_fixture_id')!r}."
        )
    bundle_inputs = bundle_meta.get("inputs") if isinstance(bundle_meta.get("inputs"), dict) else {}
    rom_path = bundle_inputs.get("rom_path")
    rom_sha = bundle_inputs.get("rom_sha256")
    if not rom_path or not rom_sha:
        raise SystemExit(f"FAIL: expected bundle ROM path/SHA provenance in {bundle_dir}.")
    if not same_resolved_path(rom_path, expected_rom_path):
        raise SystemExit(f"FAIL: expected bundle ROM path {expected_rom_path} in {bundle_dir}, got {rom_path!r}.")
    if rom_sha != expected_rom_sha256:
        raise SystemExit(f"FAIL: expected bundle ROM SHA {expected_rom_sha256!r} in {bundle_dir}, got {rom_sha!r}.")
    if not same_resolved_path(session.get("ROM_PATH"), Path(rom_path)):
        raise SystemExit(
            f"FAIL: expected adapter ROM_PATH={rom_path!r} in {bundle_dir}, got {session.get('ROM_PATH')!r}."
        )
    if session.get("ROM_SHA256") != rom_sha:
        raise SystemExit(
            f"FAIL: expected adapter ROM_SHA256={rom_sha!r} in {bundle_dir}, got {session.get('ROM_SHA256')!r}."
        )
    if not Path(rom_path).is_file():
        raise SystemExit(f"FAIL: expected bundle ROM path to exist in {bundle_dir}: {rom_path!r}.")
    if sha256_file(Path(rom_path)) != rom_sha:
        raise SystemExit(f"FAIL: bundle ROM SHA does not match current ROM artifact in {bundle_dir}.")
    if expected_cache_path is None:
        if session.get("HIRES_CACHE_PATH") or session.get("HIRES_CACHE_SHA256"):
            raise SystemExit(f"FAIL: expected off adapter cache provenance to be empty in {bundle_dir}, got {session!r}.")
        if bundle_meta.get("hires_pack_path") or bundle_meta.get("hires_pack_sha256") not in (None, "", "missing"):
            raise SystemExit(f"FAIL: expected off-bundle top-level cache provenance to be empty in {bundle_dir}, got {bundle_meta!r}.")
        if bundle_inputs.get("hires_pack_path") or bundle_inputs.get("hires_pack_sha256") not in (None, "", "missing"):
            raise SystemExit(f"FAIL: expected off-bundle cache provenance to be empty in {bundle_dir}, got {bundle_inputs!r}.")
    else:
        if not same_resolved_path(bundle_inputs.get("hires_pack_path"), expected_cache_path):
            raise SystemExit(
                f"FAIL: expected bundle hires_pack_path={expected_cache_path} in {bundle_dir}, "
                f"got {bundle_inputs.get('hires_pack_path')!r}."
            )
        if bundle_inputs.get("hires_pack_sha256") != expected_cache_sha:
            raise SystemExit(
                f"FAIL: expected bundle hires_pack_sha256={expected_cache_sha!r} in {bundle_dir}, "
                f"got {bundle_inputs.get('hires_pack_sha256')!r}."
            )
        if not same_resolved_path(session.get("HIRES_CACHE_PATH"), expected_cache_path):
            raise SystemExit(
                f"FAIL: expected adapter HIRES_CACHE_PATH={expected_cache_path} in {bundle_dir}, "
                f"got {session.get('HIRES_CACHE_PATH')!r}."
            )
        if session.get("HIRES_CACHE_SHA256") != expected_cache_sha:
            raise SystemExit(
                f"FAIL: expected adapter HIRES_CACHE_SHA256={expected_cache_sha!r} in {bundle_dir}, "
                f"got {session.get('HIRES_CACHE_SHA256')!r}."
            )
    core_path = session.get("CORE_PATH")
    core_sha = session.get("CORE_SHA256")
    if not core_path or not core_sha:
        raise SystemExit(f"FAIL: expected adapter core path/SHA provenance in {bundle_dir}.")
    if not Path(core_path).is_file() or sha256_file(Path(core_path)) != core_sha:
        raise SystemExit(f"FAIL: adapter CORE_SHA256 does not match current core artifact in {bundle_dir}.")
    require_adapter_config_provenance(bundle_dir, session)
    expected_command_signature = command_log_signature(bundle_dir)
    if not expected_command_signature:
        raise SystemExit(f"FAIL: expected adapter command log in {bundle_dir}.")
    if session.get("COMMAND_SIGNATURE") != expected_command_signature:
        raise SystemExit(
            f"FAIL: expected adapter COMMAND_SIGNATURE={expected_command_signature!r} in {bundle_dir}, "
            f"got {session.get('COMMAND_SIGNATURE')!r}."
        )
    planned_command_signature = expected_command_log_signature(bundle_dir)
    if not planned_command_signature:
        raise SystemExit(f"FAIL: expected adapter planned command log in {bundle_dir}.")
    if planned_command_signature != expected_command_signature:
        raise SystemExit(
            f"FAIL: expected executed adapter command signature {expected_command_signature!r} to match "
            f"planned command signature {planned_command_signature!r} in {bundle_dir}."
        )

expected_cache_sha256 = sha256_file(cache_path)
off_captures = sorted((off_bundle / "captures").glob("*"))
captures = sorted((on_bundle / "captures").glob("*"))
if len(off_captures) != 1:
    raise SystemExit(f"FAIL: expected exactly one off capture in {off_bundle}/captures, found {len(off_captures)}.")
if len(captures) != 1:
    raise SystemExit(f"FAIL: expected exactly one capture in {on_bundle}/captures, found {len(captures)}.")

off_semantic = json.loads((off_bundle / "traces" / "paper-mario-game-status.json").read_text())
off_status = off_semantic.get("paper_mario_us") or {}
if (
    off_status.get("game_status", {}).get("map_name_candidate") != "kmr_03"
    or int(off_status.get("game_status", {}).get("entry_id") or -1) != 5
    or off_status.get("cur_game_mode", {}).get("init_symbol") != "state_init_world"
    or off_status.get("cur_game_mode", {}).get("step_symbol") != "state_step_world"
):
    raise SystemExit(f"FAIL: unexpected off semantic state {off_status!r}.")
off_hires = json.loads((off_bundle / "traces" / "hires-evidence.json").read_text())
off_summary = off_hires.get("summary") or {}
if (
    off_hires.get("available") is not False
    or off_hires.get("cache_loaded") is not False
    or off_hires.get("cache_path")
    or off_hires.get("cache_sha256")
    or off_summary.get("provider") not in (None, "off")
    or int(off_summary.get("entry_count") or 0) != 0
    or int(off_summary.get("native_sampled_entry_count") or 0) != 0
    or int(off_summary.get("compat_entry_count") or 0) != 0
):
    raise SystemExit(f"FAIL: expected off hi-res evidence to stay disabled, got {off_hires!r}.")
require_adapter_session_provenance(off_bundle, "off", None, None, 960)

off_img = Image.open(off_captures[0]).convert("RGBA")
on_img = Image.open(captures[0]).convert("RGBA")
diff = ImageChops.difference(off_img, on_img)
hist = diff.histogram()
ae = sum((i % 256) * v for i, v in enumerate(hist))
total_sq = sum(((i % 256) ** 2) * v for i, v in enumerate(hist))
rmse = math.sqrt(total_sq / (off_img.size[0] * off_img.size[1] * 4))
if enforce_visual_envelope:
    if ae < min_ae:
        raise SystemExit(f"FAIL: timeout no-probe visual AE fell below opt-in non-checksum envelope: expected >= {min_ae}, got {ae}.")
    if ae > max_ae:
        raise SystemExit(f"FAIL: timeout no-probe visual AE exceeded opt-in non-checksum envelope: expected <= {max_ae}, got {ae}.")
    if rmse < min_rmse:
        raise SystemExit(f"FAIL: timeout no-probe visual RMSE fell below opt-in non-checksum envelope: expected >= {min_rmse}, got {rmse}.")
    if rmse > max_rmse:
        raise SystemExit(f"FAIL: timeout no-probe visual RMSE exceeded opt-in non-checksum envelope: expected <= {max_rmse}, got {rmse}.")

hires = json.loads((on_bundle / "traces" / "hires-evidence.json").read_text())
bundle_meta = json.loads((on_bundle / "bundle.json").read_text())
on_semantic = json.loads((on_bundle / "traces" / "paper-mario-game-status.json").read_text())
on_status = on_semantic.get("paper_mario_us") or {}
if (
    on_status.get("game_status", {}).get("map_name_candidate") != "kmr_03"
    or int(on_status.get("game_status", {}).get("entry_id") or -1) != 5
    or on_status.get("cur_game_mode", {}).get("init_symbol") != "state_init_world"
    or on_status.get("cur_game_mode", {}).get("step_symbol") != "state_step_world"
):
    raise SystemExit(f"FAIL: unexpected on semantic state {on_status!r}.")
if off_semantic != on_semantic:
    raise SystemExit("FAIL: expected timeout no-probe semantic trace to match feature-off trace.")
summary = hires.get("summary") or {}
descriptor_paths = summary.get("descriptor_path_counts") or {}
detail_counts = summary.get("descriptor_path_detail_counts") or {}
sampled_probe = hires.get("sampled_object_probe") or {}
bundle_inputs = bundle_meta.get("inputs") if isinstance(bundle_meta.get("inputs"), dict) else {}

if hires.get("available") is not True:
    raise SystemExit("FAIL: expected no-probe on-bundle hi-res evidence to be available.")
if hires.get("cache_loaded") is not True:
    raise SystemExit("FAIL: expected no-probe on-bundle hi-res cache to be loaded.")
if not same_resolved_path(bundle_inputs.get("hires_pack_path"), cache_path):
    raise SystemExit(
        "FAIL: bundle manifest hires_pack_path does not match requested cache: "
        f"expected {cache_path}, got {bundle_inputs.get('hires_pack_path')!r}."
    )
if bundle_inputs.get("hires_pack_sha256") != expected_cache_sha256:
    raise SystemExit(
        "FAIL: bundle manifest hires_pack_sha256 does not match requested cache: "
        f"expected {expected_cache_sha256}, got {bundle_inputs.get('hires_pack_sha256')!r}."
    )
if not same_resolved_path(hires.get("cache_path"), cache_path):
    raise SystemExit(
        "FAIL: hi-res evidence cache_path does not match requested cache: "
        f"expected {cache_path}, got {hires.get('cache_path')!r}."
    )
if hires.get("cache_sha256") != expected_cache_sha256:
    raise SystemExit(
        "FAIL: hi-res evidence cache_sha256 does not match requested cache: "
        f"expected {expected_cache_sha256}, got {hires.get('cache_sha256')!r}."
    )
require_adapter_session_provenance(on_bundle, "on", cache_path, expected_cache_sha256, 960)

if summary.get("provider") != "on":
    raise SystemExit(f"FAIL: expected provider=on, got {summary.get('provider')!r}.")
if summary.get("source_mode") != "phrb-only":
    raise SystemExit(f"FAIL: expected source_mode=phrb-only, got {summary.get('source_mode')!r}.")
if int(summary.get("compat_entry_count") or 0) != 0:
    raise SystemExit(f"FAIL: expected compat_entry_count=0, got {summary.get('compat_entry_count')!r}.")
if int(summary.get("native_sampled_entry_count") or 0) < expected_native_sampled_entry_count:
    raise SystemExit(
        "FAIL: selected-package no-probe lane native sampled entry count fell below the required minimum: "
        f"expected >= {expected_native_sampled_entry_count}, got {summary.get('native_sampled_entry_count')!r}."
    )
if int((summary.get("source_counts") or {}).get("phrb") or 0) < 1:
    raise SystemExit("FAIL: expected at least one PHRB-backed entry.")
for key, value in (summary.get("source_counts") or {}).items():
    if key != "phrb" and int(value or 0) != 0:
        raise SystemExit(f"FAIL: expected source_counts.{key}=0, got {value!r}.")
if int(descriptor_paths.get("sampled") or 0) != expected_sampled_descriptor_path_count:
    raise SystemExit(
        "FAIL: selected-package no-probe lane changed sampled descriptor-path resolutions: "
        f"expected {expected_sampled_descriptor_path_count}, got {descriptor_paths!r}."
    )
for key in ("native_checksum", "generic", "compat"):
    if int(descriptor_paths.get(key) or 0) != 0:
        raise SystemExit(f"FAIL: expected descriptor_paths.{key}=0, got {descriptor_paths.get(key)!r}.")
if int(sampled_probe.get("exact_hit_count") or 0) < 1:
    raise SystemExit(f"FAIL: expected sampled exact hits without probe, got {sampled_probe.get('exact_hit_count')!r}.")
if int(detail_counts.get("sampled_ordered_surface_singleton") or 0) < 1:
    raise SystemExit(
        "FAIL: expected sampled ordered-surface singleton descriptor traffic to stay live without probe, "
        f"got {detail_counts.get('sampled_ordered_surface_singleton')!r}."
    )
PY

echo "emu_conformance_paper_mario_selected_package_timeout_lookup_without_probe: PASS ($CACHE_PATH)"
