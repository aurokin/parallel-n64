#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "$SCRIPT_DIR/../.." && pwd)"
source "$SCRIPT_DIR/lib/common.sh"

GAME_ID=""
ROM_PATH=""
CACHE_PATH=""
STATE_PATH=""
BUNDLE_DIR=""
POST_LOAD_SETTLE_FRAMES=3
STARTUP_WAIT=8
EXPECTED_STATE_SHA256=""
MIN_ENTRIES=1
MIN_COMPAT_DRAW_HITS=0
MIN_CI_ATTEMPTS=0
MIN_CI_HITS=0
CI_COMPAT=0
EXPECTED_SOURCE_MODE="phrb-only"
EXPECTED_ENTRY_CLASS=""
RETROARCH_BIN="${RETROARCH_BIN:-/home/auro/code/RetroArch/retroarch}"
RETROARCH_BASE_CONFIG="${RETROARCH_BASE_CONFIG:-/home/auro/code/RetroArch/retroarch.cfg}"
CORE_PATH="${CORE_PATH:-$REPO_ROOT/parallel_n64_libretro.so}"
REUSE=0

usage() {
  cat <<'USAGE'
Usage:
  tools/scenarios/cross-game-hires-savestate-fixture-validation.sh [options]

Required:
  --game-id ID                  Stable fixture/game label
  --rom PATH                    ROM/content path
  --cache-path PATH             Runtime PHRB package
  --state-path PATH             Authoritative savestate path

Options:
  --bundle-dir PATH             Bundle directory
  --settle-frames N             Frames to step after loading state (default: 3)
  --startup-wait N              Initial RetroArch startup wait seconds (default: 8)
  --expected-state-sha256 H     Required locked savestate hash
  --min-entries N               Minimum loaded provider entries (default: 1)
  --min-compat-draw-hits N      Minimum GlideN64-compat draw-time hits (default: 0)
  --min-ci-attempts N           Minimum CI compat draw attempts (default: 0)
  --min-ci-hits N               Minimum CI compat draw hits (default: 0)
  --ci-compat                   Enable CI palette CRC compatibility during runtime
  --expected-source-mode MODE   Expected hi-res source mode (default: phrb-only)
  --expected-entry-class CLASS  Optional expected entry class
  --reuse                       Reuse an existing bundle instead of launching RetroArch
  -h, --help                    Show this help
USAGE
}

while (($#)); do
  case "$1" in
    --game-id)
      shift
      GAME_ID="${1:-}"
      ;;
    --rom)
      shift
      ROM_PATH="${1:-}"
      ;;
    --cache-path)
      shift
      CACHE_PATH="${1:-}"
      ;;
    --state-path)
      shift
      STATE_PATH="${1:-}"
      ;;
    --bundle-dir)
      shift
      BUNDLE_DIR="${1:-}"
      ;;
    --settle-frames)
      shift
      POST_LOAD_SETTLE_FRAMES="${1:-}"
      ;;
    --startup-wait)
      shift
      STARTUP_WAIT="${1:-}"
      ;;
    --expected-state-sha256)
      shift
      EXPECTED_STATE_SHA256="${1:-}"
      ;;
    --allow-unlocked-state)
      echo "--allow-unlocked-state is no longer supported; provide --expected-state-sha256." >&2
      exit 2
      ;;
    --min-entries)
      shift
      MIN_ENTRIES="${1:-}"
      ;;
    --min-compat-draw-hits)
      shift
      MIN_COMPAT_DRAW_HITS="${1:-}"
      ;;
    --min-ci-attempts)
      shift
      MIN_CI_ATTEMPTS="${1:-}"
      ;;
    --min-ci-hits)
      shift
      MIN_CI_HITS="${1:-}"
      ;;
    --ci-compat)
      CI_COMPAT=1
      ;;
    --expected-source-mode)
      shift
      EXPECTED_SOURCE_MODE="${1:-}"
      ;;
    --expected-entry-class)
      shift
      EXPECTED_ENTRY_CLASS="${1:-}"
      ;;
    --reuse)
      REUSE=1
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

if [[ -z "$GAME_ID" || -z "$ROM_PATH" || -z "$CACHE_PATH" || -z "$STATE_PATH" ]]; then
  echo "--game-id, --rom, --cache-path, and --state-path are required." >&2
  exit 2
fi
for value_name in POST_LOAD_SETTLE_FRAMES MIN_ENTRIES MIN_COMPAT_DRAW_HITS MIN_CI_ATTEMPTS MIN_CI_HITS; do
  value="${!value_name}"
  if ! [[ "$value" =~ ^[0-9]+$ ]]; then
    echo "${value_name} must be a non-negative integer." >&2
    exit 2
  fi
done

if [[ ! -f "$ROM_PATH" ]]; then
  echo "ROM not found: $ROM_PATH" >&2
  exit 2
fi
if [[ ! -f "$CACHE_PATH" ]]; then
  echo "PHRB package not found: $CACHE_PATH" >&2
  exit 2
fi
if ! scenario_require_phrb_runtime_cache "$CACHE_PATH"; then
  exit 2
fi
if [[ ! -f "$STATE_PATH" ]]; then
  echo "Savestate not found: $STATE_PATH" >&2
  exit 2
fi
if [[ -z "$EXPECTED_STATE_SHA256" ]]; then
  echo "--expected-state-sha256 is required." >&2
  exit 2
fi
if [[ ! -f "$CORE_PATH" ]]; then
  echo "Core not found: $CORE_PATH" >&2
  exit 2
fi
if (( ! REUSE )) && [[ ! -x "$RETROARCH_BIN" ]]; then
  echo "RetroArch binary not executable: $RETROARCH_BIN" >&2
  exit 2
fi
if (( ! REUSE )) && [[ ! -f "$RETROARCH_BASE_CONFIG" ]]; then
  echo "RetroArch base config not found: $RETROARCH_BASE_CONFIG" >&2
  exit 2
fi

if [[ -z "$BUNDLE_DIR" ]]; then
  safe_game_id="${GAME_ID//[^A-Za-z0-9._-]/-}"
  BUNDLE_DIR="$REPO_ROOT/artifacts/cross-game-probes/fixtures/$(date +"%Y%m%d-%H%M%S")-${safe_game_id}"
fi

rom_file="$(basename -- "$ROM_PATH")"
rom_stem="$rom_file"
case "${rom_stem,,}" in
  *.zip|*.z64|*.n64|*.v64)
    rom_stem="${rom_stem%.*}"
    ;;
esac

if (( ! REUSE )); then
  rm -rf "$BUNDLE_DIR"/captures "$BUNDLE_DIR"/logs "$BUNDLE_DIR"/traces "$BUNDLE_DIR"/savefiles "$BUNDLE_DIR/states/ParaLLEl N64"
fi
mkdir -p "$BUNDLE_DIR"/captures "$BUNDLE_DIR"/logs "$BUNDLE_DIR"/traces "$BUNDLE_DIR/states/ParaLLEl N64"
CACHE_SHA256="$(scenario_sha256_file "$CACHE_PATH")"
if (( ! REUSE )); then
  python3 - "$BUNDLE_DIR" "$GAME_ID" "$ROM_PATH" "$CACHE_PATH" "$CACHE_SHA256" "$STATE_PATH" "$CORE_PATH" "$POST_LOAD_SETTLE_FRAMES" "$EXPECTED_SOURCE_MODE" "$EXPECTED_ENTRY_CLASS" <<'PY'
import json
import sys
from pathlib import Path

bundle, game_id, rom_path, cache_path, cache_sha256, state_path, core_path, settle_frames, expected_source_mode, expected_entry_class = sys.argv[1:]

manifest = {
    "schema_version": 1,
    "scenario": "cross-game-hires-savestate-fixture-validation",
    "scenario_state": "bundle_initialized",
    "runtime_executed": False,
    "game_id": game_id,
    "authority_mode": "compat-only-regression",
    "mode": "on",
    "rom_path": rom_path,
    "cache_path": cache_path,
    "hires_pack_sha256": cache_sha256,
    "state_path": state_path,
    "core_path": core_path,
    "post_load_settle_frames": int(settle_frames),
    "expected_source_mode": expected_source_mode,
    "expected_entry_class": expected_entry_class or None,
}
Path(bundle, "bundle.json").write_text(json.dumps(manifest, indent=2) + "\n")
PY
elif [[ ! -f "$BUNDLE_DIR/bundle.json" ]]; then
  echo "Reused bundle manifest missing: $BUNDLE_DIR/bundle.json" >&2
  exit 2
fi
STAGED_STATE_PATH="$BUNDLE_DIR/states/ParaLLEl N64/${rom_stem}.state"
if (( REUSE )); then
  if [[ ! -f "$STAGED_STATE_PATH" ]]; then
    echo "Staged savestate missing in reused bundle: $STAGED_STATE_PATH" >&2
    exit 2
  fi
else
  cp "$STATE_PATH" "$STAGED_STATE_PATH"
fi

ADAPTER_RC=0
if (( ! REUSE )); then
  set +e
  env \
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
    PARALLEL_N64_GFX_PLUGIN_OVERRIDE=parallel \
    PARALLEL_RDP_HIRES_CACHE_PATH="$CACHE_PATH" \
    PARALLEL_RDP_HIRES_CACHE_SHA256="$CACHE_SHA256" \
    PARALLEL_RDP_HIRES_CI_COMPAT="$CI_COMPAT" \
    PARALLEL_RDP_HIRES_SAMPLED_OBJECT_LOOKUP=0 \
    PARALLEL_RDP_HIRES_SAMPLED_OBJECT_PROBE=0 \
    "$REPO_ROOT/tools/adapters/retroarch_stdin_session.sh" \
    --bundle-dir "$BUNDLE_DIR" \
    --mode on \
    --retroarch-bin "$RETROARCH_BIN" \
    --base-config "$RETROARCH_BASE_CONFIG" \
    --core "$CORE_PATH" \
    --rom "$ROM_PATH" \
    --startup-wait "$STARTUP_WAIT" \
    --command "WAIT_COMMAND_READY 120" \
    --command "LOAD_STATE_SLOT_PAUSED 0" \
    --command "STEP_FRAME ${POST_LOAD_SETTLE_FRAMES}" \
    --command "WAIT_STATUS_FRAME PAUSED ${POST_LOAD_SETTLE_FRAMES} 15" \
    --command "SCREENSHOT" \
    --command "WAIT_NEW_CAPTURE 15" \
    --command "QUIT"
  ADAPTER_RC=$?
  set -e
else
  if [[ -f "$BUNDLE_DIR/retroarch.run.env" ]]; then
    ADAPTER_RC="$(awk -F= '$1 == "RETROARCH_EXIT_STATUS" { print $2 }' "$BUNDLE_DIR/retroarch.run.env" | tail -n1)"
    ADAPTER_RC="${ADAPTER_RC:-1}"
  else
    ADAPTER_RC=1
  fi
  RUN_EXECUTED="$(awk -F= '$1 == "RUNTIME_EXECUTED" { print $2 }' "$BUNDLE_DIR/retroarch.run.env" 2>/dev/null | tail -n1 || true)"
  FORCED_TERMINATION="$(awk -F= '$1 == "FORCED_TERMINATION" { print $2 }' "$BUNDLE_DIR/retroarch.run.env" 2>/dev/null | tail -n1 || true)"
  if [[ "$RUN_EXECUTED" != "1" || "$FORCED_TERMINATION" != "0" ]]; then
    ADAPTER_RC=1
  fi
fi

scenario_extract_hires_log_evidence "$BUNDLE_DIR" "$BUNDLE_DIR/traces/hires-evidence.json"

python3 - "$GAME_ID" "$ROM_PATH" "$CACHE_PATH" "$STATE_PATH" "$STAGED_STATE_PATH" "$CORE_PATH" "$BUNDLE_DIR" \
  "$POST_LOAD_SETTLE_FRAMES" "$MIN_ENTRIES" "$MIN_COMPAT_DRAW_HITS" \
  "$MIN_CI_ATTEMPTS" "$MIN_CI_HITS" "$EXPECTED_SOURCE_MODE" "$EXPECTED_ENTRY_CLASS" "$EXPECTED_STATE_SHA256" "$ADAPTER_RC" <<'PY'
import hashlib
import json
import re
import sys
from pathlib import Path

(
    game_id,
    rom_path,
    cache_path,
    state_path,
    staged_state_path,
    core_path,
    bundle_dir,
    settle_frames,
    min_entries,
    min_compat_draw_hits,
    min_ci_attempts,
    min_ci_hits,
    expected_source_mode,
    expected_entry_class,
    expected_state_sha256,
    adapter_rc,
) = sys.argv[1:]

bundle = Path(bundle_dir)
hires_path = bundle / "traces" / "hires-evidence.json"
hires = json.loads(hires_path.read_text()) if hires_path.exists() else {}
summary = hires.get("summary") or {}
captures = sorted((bundle / "captures").glob("*"))

def sha256_file(path: Path):
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()

def as_int(value):
    try:
        return int(value or 0)
    except (TypeError, ValueError):
        return 0

def same_resolved_path(left, right):
    if not left or not right:
        return False
    try:
        return Path(left).resolve() == Path(right).resolve()
    except OSError:
        return False

def read_session_env(path: Path):
    values = {}
    if not path.is_file():
        return values
    for line in path.read_text(errors="replace").splitlines():
        if "=" not in line:
            continue
        key, value = line.split("=", 1)
        values[key] = value
    return values

def log_has_exact_state_load_ack(log_text, path):
    raw_path = str(path)
    try:
        resolved_path = str(Path(path).resolve())
    except OSError:
        resolved_path = raw_path
    expected = {raw_path, resolved_path}
    for match in re.finditer(r"\[State\] Loading state \"([^\"]+)\"", log_text):
        loaded_path = match.group(1)
        if loaded_path in expected:
            return match.end()
    return None

capture_path = captures[0] if len(captures) == 1 else None
actual_capture_sha256 = sha256_file(capture_path) if capture_path else None
actual_state_sha256 = sha256_file(Path(state_path))
staged_state_sha256 = sha256_file(Path(staged_state_path))
log_text = (bundle / "logs" / "retroarch.log").read_text(errors="replace") if (bundle / "logs" / "retroarch.log").is_file() else ""
load_ack_end = log_has_exact_state_load_ack(log_text, staged_state_path)
load_ack_uses_staged_state = load_ack_end is not None
post_load_status_frame = None
post_load_log_text = log_text[load_ack_end:] if load_ack_end is not None else ""
for match in re.finditer(r"GET_STATUS\s+PAUSED\b[^\n]*\bframe=(\d+)", post_load_log_text):
    post_load_status_frame = int(match.group(1))
reported_cache_path = hires.get("cache_path")
reported_cache_sha256 = hires.get("cache_sha256")
session_env = read_session_env(bundle / "retroarch.session.env")
manifest_path = bundle / "bundle.json"
manifest = json.loads(manifest_path.read_text()) if manifest_path.is_file() else {}
rom_sha256 = sha256_file(Path(rom_path))
core_sha256 = sha256_file(Path(core_path))
cache_sha256 = sha256_file(Path(cache_path))
expected_commands = [
    "WAIT_COMMAND_READY 120",
    "LOAD_STATE_SLOT_PAUSED 0",
    f"STEP_FRAME {settle_frames}",
    f"WAIT_STATUS_FRAME PAUSED {settle_frames} 15",
    "SCREENSHOT",
    "WAIT_NEW_CAPTURE 15",
    "QUIT",
]
expected_command_signature = hashlib.sha256(("\n".join(expected_commands) + "\n").encode()).hexdigest()

def read_command_log(path: Path):
    if not path.is_file():
        return None
    return path.read_text(errors="replace").splitlines()

expected_command_log = read_command_log(bundle / "retroarch.expected.commands.log")
executed_command_log = read_command_log(bundle / "retroarch.executed.commands.log")
proof_log = read_command_log(bundle / "retroarch.command-proofs.log")
proof_commands = []
proofs_have_sources = []
if proof_log is not None:
    for line in proof_log:
        command, sep, proof = line.partition("\tproof=")
        proof_commands.append(command)
        proofs_have_sources.append(bool(sep and proof))

checks = {
    "single_capture": len(captures) == 1,
    "state_sha256_match": None if not expected_state_sha256 else actual_state_sha256 == expected_state_sha256,
    "staged_state_sha256_match": staged_state_sha256 == actual_state_sha256,
    "load_ack_uses_staged_state": load_ack_uses_staged_state,
    "post_load_paused_frame_reached": post_load_status_frame is not None and post_load_status_frame >= int(settle_frames),
    "hires_evidence_available": bool(hires.get("available")),
    "cache_loaded": bool(hires.get("cache_loaded")),
    "cache_path_match": same_resolved_path(reported_cache_path, cache_path),
    "cache_sha256_match": reported_cache_sha256 == cache_sha256,
    "session_rom_path_match": same_resolved_path(session_env.get("ROM_PATH"), rom_path),
    "session_core_path_match": same_resolved_path(session_env.get("CORE_PATH"), core_path),
    "session_rom_sha256_match": session_env.get("ROM_SHA256") == rom_sha256,
    "session_core_sha256_match": session_env.get("CORE_SHA256") == core_sha256,
    "session_cache_path_match": same_resolved_path(session_env.get("HIRES_CACHE_PATH"), cache_path),
    "session_cache_sha256_match": session_env.get("HIRES_CACHE_SHA256") == cache_sha256,
    "session_mode_on": session_env.get("MODE") == "on",
    "session_command_signature_match": session_env.get("COMMAND_SIGNATURE") == expected_command_signature,
    "expected_command_log_match": expected_command_log == expected_commands,
    "executed_command_log_match": executed_command_log == expected_commands,
    "command_proof_log_match": proof_commands == expected_commands and len(proofs_have_sources) == len(expected_commands) and all(proofs_have_sources),
    "manifest_scenario_match": manifest.get("scenario") == "cross-game-hires-savestate-fixture-validation",
    "manifest_mode_on": manifest.get("mode") == "on",
    "manifest_game_id_match": manifest.get("game_id") == game_id,
    "manifest_rom_path_match": same_resolved_path(manifest.get("rom_path"), rom_path),
    "manifest_cache_path_match": same_resolved_path(manifest.get("cache_path"), cache_path),
    "manifest_cache_sha256_match": manifest.get("hires_pack_sha256") == cache_sha256,
    "manifest_core_path_match": same_resolved_path(manifest.get("core_path"), core_path),
    "manifest_state_path_match": same_resolved_path(manifest.get("state_path"), state_path),
    "provider_on": summary.get("provider") == "on",
    "source_mode_match": summary.get("source_mode") == expected_source_mode,
    "min_entries": as_int(summary.get("entry_count")) >= int(min_entries),
    "min_compat_draw_hits": as_int(summary.get("compat_draw_hits")) >= int(min_compat_draw_hits),
    "min_ci_attempts": as_int(summary.get("compat_draw_ci_attempts")) >= int(min_ci_attempts),
    "min_ci_hits": as_int(summary.get("compat_draw_ci_hits")) >= int(min_ci_hits),
    "adapter_succeeded": int(adapter_rc) == 0,
}
if expected_entry_class:
    checks["entry_class_match"] = summary.get("entry_class") == expected_entry_class
if expected_entry_class == "compat-only":
    descriptor_paths = summary.get("descriptor_path_counts") or {}
    checks["compat_descriptor_paths_only"] = (
        as_int(descriptor_paths.get("compat")) > 0
        and as_int(descriptor_paths.get("sampled")) == 0
        and as_int(descriptor_paths.get("native_checksum")) == 0
        and as_int(descriptor_paths.get("generic")) == 0
    )

failures = []
if not checks["single_capture"]:
    failures.append(f"Expected exactly one capture, found {len(captures)}.")
if not checks["adapter_succeeded"]:
    failures.append(f"RetroArch adapter exited with status {adapter_rc}.")
if expected_state_sha256 and not checks["state_sha256_match"]:
    failures.append(f"State hash mismatch: expected {expected_state_sha256}, got {actual_state_sha256}.")
if not checks["staged_state_sha256_match"]:
    failures.append(f"Staged state hash mismatch: source {actual_state_sha256}, staged {staged_state_sha256}.")
if not checks["load_ack_uses_staged_state"]:
    failures.append(f"RetroArch log did not acknowledge loading the staged state {staged_state_path}.")
if not checks["post_load_paused_frame_reached"]:
    failures.append(
        f"RetroArch log did not confirm paused post-load frame >= {settle_frames}; got {post_load_status_frame!r}."
    )
if not checks["hires_evidence_available"]:
    failures.append("Hi-res evidence was not available in the RetroArch log.")
if not checks["cache_loaded"]:
    failures.append("Hi-res cache was not reported as loaded.")
if not checks["cache_path_match"]:
    failures.append(f"Expected loaded cache path {cache_path!r}, got {reported_cache_path!r}.")
if not checks["cache_sha256_match"]:
    failures.append(f"Expected loaded cache SHA-256 {cache_sha256!r}, got {reported_cache_sha256!r}.")
if not checks["session_rom_path_match"]:
    failures.append(f"Expected reused session ROM path {rom_path!r}, got {session_env.get('ROM_PATH')!r}.")
if not checks["session_core_path_match"]:
    failures.append(f"Expected reused session core path {core_path!r}, got {session_env.get('CORE_PATH')!r}.")
if not checks["session_rom_sha256_match"]:
    failures.append(f"Expected reused session ROM SHA-256 {rom_sha256!r}, got {session_env.get('ROM_SHA256')!r}.")
if not checks["session_core_sha256_match"]:
    failures.append(f"Expected reused session core SHA-256 {core_sha256!r}, got {session_env.get('CORE_SHA256')!r}.")
if not checks["session_cache_path_match"]:
    failures.append(f"Expected reused session cache path {cache_path!r}, got {session_env.get('HIRES_CACHE_PATH')!r}.")
if not checks["session_cache_sha256_match"]:
    failures.append(f"Expected reused session cache SHA-256 {cache_sha256!r}, got {session_env.get('HIRES_CACHE_SHA256')!r}.")
if not checks["session_mode_on"]:
    failures.append(f"Expected reused session mode 'on', got {session_env.get('MODE')!r}.")
if not checks["session_command_signature_match"]:
    failures.append(
        f"Expected reused savestate command signature {expected_command_signature!r}, "
        f"got {session_env.get('COMMAND_SIGNATURE')!r}."
    )
if not checks["expected_command_log_match"]:
    failures.append(f"Expected adapter command log {expected_commands!r}, got {expected_command_log!r}.")
if not checks["executed_command_log_match"]:
    failures.append(f"Expected executed command log {expected_commands!r}, got {executed_command_log!r}.")
if not checks["command_proof_log_match"]:
    failures.append(f"Expected command proof log for all commands, got {proof_log!r}.")
if not checks["manifest_scenario_match"]:
    failures.append(f"Expected bundle manifest scenario cross-game-hires-savestate-fixture-validation, got {manifest.get('scenario')!r}.")
if not checks["manifest_mode_on"]:
    failures.append(f"Expected bundle manifest mode 'on', got {manifest.get('mode')!r}.")
if not checks["manifest_game_id_match"]:
    failures.append(f"Expected bundle manifest game_id {game_id!r}, got {manifest.get('game_id')!r}.")
if not checks["manifest_rom_path_match"]:
    failures.append(f"Expected bundle manifest ROM path {rom_path!r}, got {manifest.get('rom_path')!r}.")
if not checks["manifest_cache_path_match"]:
    failures.append(f"Expected bundle manifest cache path {cache_path!r}, got {manifest.get('cache_path')!r}.")
if not checks["manifest_cache_sha256_match"]:
    failures.append(f"Expected bundle manifest cache SHA-256 {cache_sha256!r}, got {manifest.get('hires_pack_sha256')!r}.")
if not checks["manifest_core_path_match"]:
    failures.append(f"Expected bundle manifest core path {core_path!r}, got {manifest.get('core_path')!r}.")
if not checks["manifest_state_path_match"]:
    failures.append(f"Expected bundle manifest state path {state_path!r}, got {manifest.get('state_path')!r}.")
if not checks["provider_on"]:
    failures.append(f"Expected provider=on, got {summary.get('provider')!r}.")
if not checks["source_mode_match"]:
    failures.append(f"Expected source_mode={expected_source_mode!r}, got {summary.get('source_mode')!r}.")
if not checks["min_entries"]:
    failures.append(f"Expected entry_count >= {min_entries}, got {summary.get('entry_count')!r}.")
if not checks["min_compat_draw_hits"]:
    failures.append(f"Expected compat_draw_hits >= {min_compat_draw_hits}, got {summary.get('compat_draw_hits')!r}.")
if not checks["min_ci_attempts"]:
    failures.append(f"Expected compat_draw_ci_attempts >= {min_ci_attempts}, got {summary.get('compat_draw_ci_attempts')!r}.")
if not checks["min_ci_hits"]:
    failures.append(f"Expected compat_draw_ci_hits >= {min_ci_hits}, got {summary.get('compat_draw_ci_hits')!r}.")
if expected_entry_class and not checks.get("entry_class_match"):
    failures.append(f"Expected entry_class={expected_entry_class!r}, got {summary.get('entry_class')!r}.")
if expected_entry_class == "compat-only" and not checks.get("compat_descriptor_paths_only"):
    failures.append(
        "Expected compat-only descriptor paths with no sampled/native-checksum/generic traffic, "
        f"got {summary.get('descriptor_path_counts')!r}."
    )

validation = {
    "game_id": game_id,
    "bundle_dir": str(bundle),
    "settle_frames": int(settle_frames),
    "rom_path": rom_path,
    "rom_sha256": rom_sha256,
    "cache_path": cache_path,
    "reported_cache_path": reported_cache_path,
    "reported_cache_sha256": reported_cache_sha256,
    "cache_sha256": cache_sha256,
    "state_path": state_path,
    "state_sha256": actual_state_sha256,
    "staged_state_path": staged_state_path,
    "staged_state_sha256": staged_state_sha256,
    "expected_state_sha256": expected_state_sha256 or None,
    "core_path": core_path,
    "core_sha256": core_sha256,
    "session_identity": {
        "rom_path": session_env.get("ROM_PATH"),
        "core_path": session_env.get("CORE_PATH"),
        "rom_sha256": session_env.get("ROM_SHA256"),
        "core_sha256": session_env.get("CORE_SHA256"),
        "cache_path": session_env.get("HIRES_CACHE_PATH"),
        "cache_sha256": session_env.get("HIRES_CACHE_SHA256"),
        "mode": session_env.get("MODE"),
        "command_signature": session_env.get("COMMAND_SIGNATURE"),
        "expected_command_signature": expected_command_signature,
    },
    "bundle_manifest": manifest,
    "capture_path": str(capture_path) if capture_path else None,
    "capture_sha256": actual_capture_sha256,
    "post_load_status_frame": post_load_status_frame,
    "expected_source_mode": expected_source_mode,
    "expected_entry_class": expected_entry_class or None,
    "thresholds": {
        "min_entries": int(min_entries),
        "min_compat_draw_hits": int(min_compat_draw_hits),
        "min_ci_attempts": int(min_ci_attempts),
        "min_ci_hits": int(min_ci_hits),
    },
    "adapter_exit_code": int(adapter_rc),
    "checks": checks,
    "hires_summary": summary,
    "passed": not failures,
    "failures": failures,
}
(bundle / "validation-summary.json").write_text(json.dumps(validation, indent=2) + "\n")

descriptor_paths = summary.get("descriptor_path_counts") or {}
md = [
    f"# {game_id} Hi-Res Savestate Fixture Validation",
    "",
    f"- Bundle: `{bundle}`",
    f"- State SHA-256: `{validation['state_sha256']}`",
    f"- Expected state SHA-256: `{validation['expected_state_sha256']}`",
    f"- Capture SHA-256 (artifact identity only): `{validation['capture_sha256']}`",
    f"- Passed: `{str(validation['passed']).lower()}`",
    f"- Provider: `{summary.get('provider')}`",
    f"- Source mode: `{summary.get('source_mode')}`",
    f"- Entry class: `{summary.get('entry_class')}`",
    f"- Entries: `{summary.get('entry_count')}` native sampled `{summary.get('native_sampled_entry_count')}` compat `{summary.get('compat_entry_count')}`",
    f"- Compat draw hits: `{summary.get('compat_draw_hits', 0)}` CI `{summary.get('compat_draw_ci_hits', 0)}/{summary.get('compat_draw_ci_attempts', 0)}`",
    f"- Descriptor paths: sampled `{descriptor_paths.get('sampled', 0)}`, native checksum `{descriptor_paths.get('native_checksum', 0)}`, generic `{descriptor_paths.get('generic', 0)}`, compat `{descriptor_paths.get('compat', 0)}`",
]
if failures:
    md.append(f"- Failures: `{' | '.join(failures)}`")
(bundle / "validation-summary.md").write_text("\n".join(md) + "\n")

if failures:
    for failure in failures:
        print(f"FAIL: {failure}", file=sys.stderr)
    print(bundle / "validation-summary.json", file=sys.stderr)
    sys.exit(1)

print(bundle / "validation-summary.json")
PY

echo "[validation] complete: $BUNDLE_DIR"
