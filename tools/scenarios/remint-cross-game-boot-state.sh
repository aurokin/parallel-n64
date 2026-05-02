#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "$SCRIPT_DIR/../.." && pwd)"
source "$SCRIPT_DIR/lib/common.sh"

GAME_ID=""
ROM_PATH=""
OUTPUT_PATH=""
BUNDLE_ROOT=""
BOOT_WAIT_SECONDS=30
VERIFY_SETTLE_FRAMES=3
STARTUP_WAIT=8
EXPECTED_VERIFY_CAPTURE_SHA256=""
ALLOW_UNVERIFIED=0
RETROARCH_BIN="${RETROARCH_BIN:-/home/auro/code/RetroArch/retroarch}"
RETROARCH_BASE_CONFIG="${RETROARCH_BASE_CONFIG:-/home/auro/code/RetroArch/retroarch.cfg}"
CORE_PATH="${CORE_PATH:-$REPO_ROOT/parallel_n64_libretro.so}"
RUNTIME_ADAPTER="${RUNTIME_ADAPTER:-$REPO_ROOT/tools/adapters/retroarch_stdin_session.sh}"

usage() {
  cat <<'USAGE'
Usage:
  tools/scenarios/remint-cross-game-boot-state.sh [options]

Required:
  --game-id ID                  Stable game/fixture label
  --rom PATH                    ROM/content path
  --output-path PATH            Destination savestate path

Options:
  --bundle-root PATH            Directory for remint/verify evidence bundles
  --boot-wait-seconds N         Seconds to wait before saving (default: 30)
  --verify-settle-frames N      Frames to step after loading during verification (default: 3)
  --startup-wait N              Initial RetroArch startup wait seconds (default: 8)
  --expected-verify-capture-sha256 H
                                Required locked baseline-off verification capture hash
  --allow-unverified            Explicitly write an unverified remint summary without a capture hash
  -h, --help                    Show this help

Notes:
  - This mints a baseline authority state with hi-res disabled.
  - Verification uses load -> settle frames -> capture, also with hi-res disabled.
  - Runtime hi-res validation should be done by cross-game-hires-savestate-fixture-validation.sh.
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
    --output-path)
      shift
      OUTPUT_PATH="${1:-}"
      ;;
    --bundle-root)
      shift
      BUNDLE_ROOT="${1:-}"
      ;;
    --boot-wait-seconds)
      shift
      BOOT_WAIT_SECONDS="${1:-}"
      ;;
    --verify-settle-frames)
      shift
      VERIFY_SETTLE_FRAMES="${1:-}"
      ;;
    --startup-wait)
      shift
      STARTUP_WAIT="${1:-}"
      ;;
    --expected-verify-capture-sha256)
      shift
      EXPECTED_VERIFY_CAPTURE_SHA256="${1:-}"
      ;;
    --allow-unverified)
      ALLOW_UNVERIFIED=1
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

if [[ -z "$GAME_ID" || -z "$ROM_PATH" || -z "$OUTPUT_PATH" ]]; then
  echo "--game-id, --rom, and --output-path are required." >&2
  exit 2
fi
if ! [[ "$BOOT_WAIT_SECONDS" =~ ^[0-9]+([.][0-9]+)?$ ]]; then
  echo "--boot-wait-seconds must be numeric." >&2
  exit 2
fi
if ! [[ "$STARTUP_WAIT" =~ ^[0-9]+([.][0-9]+)?$ ]]; then
  echo "--startup-wait must be numeric." >&2
  exit 2
fi
if ! [[ "$VERIFY_SETTLE_FRAMES" =~ ^[0-9]+$ ]]; then
  echo "--verify-settle-frames must be a non-negative integer." >&2
  exit 2
fi

safe_game_id="${GAME_ID//[^A-Za-z0-9._-]/-}"
if [[ -z "$BUNDLE_ROOT" ]]; then
  BUNDLE_ROOT="$REPO_ROOT/artifacts/cross-game-probes/remint/$(date +"%Y%m%d-%H%M%S")-${safe_game_id}"
fi

rom_file="$(basename -- "$ROM_PATH")"
rom_stem="$rom_file"
case "${rom_stem,,}" in
  *.zip|*.z64|*.n64|*.v64)
    rom_stem="${rom_stem%.*}"
    ;;
esac

BOOTSTRAP_BUNDLE="$BUNDLE_ROOT/bootstrap"
VERIFY_BUNDLE="$BUNDLE_ROOT/verify"
BOOTSTRAP_STATE="$BOOTSTRAP_BUNDLE/states/ParaLLEl N64/${rom_stem}.state"
CANDIDATE_OUTPUT_PATH="$BUNDLE_ROOT/candidate/${rom_stem}.state"

rm -rf "$BOOTSTRAP_BUNDLE" "$VERIFY_BUNDLE" "$BUNDLE_ROOT/candidate"

write_remint_failure_summary() {
  local failure="$1"
  local exit_code="${2:-1}"
  mkdir -p "$BUNDLE_ROOT"
  python3 - "$GAME_ID" "$ROM_PATH" "$OUTPUT_PATH" "$CORE_PATH" "$BUNDLE_ROOT" "$failure" <<'PY'
import hashlib
import json
import sys
from pathlib import Path

game_id, rom_path, output_path, core_path, bundle_root, failure = sys.argv[1:]
bundle = Path(bundle_root)

def sha256_file(path):
    path = Path(path)
    if not path.is_file():
        return None
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()

summary = {
    "game_id": game_id,
    "bundle_root": str(bundle),
    "rom_path": rom_path,
    "rom_sha256": sha256_file(rom_path),
    "requested_output_path": output_path,
    "output_path": None,
    "output_written": False,
    "core_path": core_path,
    "core_sha256": sha256_file(core_path),
    "verification_status": "verification-failed",
    "authoritative": False,
    "promoted": False,
    "passed": False,
    "failure": failure,
}
(bundle / "remint-summary.json").write_text(json.dumps(summary, indent=2) + "\n")
(bundle / "remint-summary.md").write_text(
    "\n".join([
        f"# {game_id} Cross-Game Boot State Remint",
        "",
        f"- Bundle: `{bundle}`",
        f"- Requested output: `{output_path}`",
        "- Output written: `false`",
        f"- Verification status: `{summary['verification_status']}`",
        f"- Authoritative: `{str(summary['authoritative']).lower()}`",
        f"- Promoted: `{str(summary['promoted']).lower()}`",
        "- Passed: `false`",
        f"- Failure: `{failure}`",
    ]) + "\n"
)
PY
  exit "$exit_code"
}

if [[ ! -f "$ROM_PATH" ]]; then
  echo "ROM not found: $ROM_PATH" >&2
  write_remint_failure_summary "ROM not found: $ROM_PATH" 2
fi
if [[ ! -f "$CORE_PATH" ]]; then
  echo "Core not found: $CORE_PATH" >&2
  write_remint_failure_summary "Core not found: $CORE_PATH" 2
fi
if [[ ! -x "$RETROARCH_BIN" ]]; then
  echo "RetroArch binary not executable: $RETROARCH_BIN" >&2
  write_remint_failure_summary "RetroArch binary not executable: $RETROARCH_BIN" 2
fi
if [[ ! -f "$RETROARCH_BASE_CONFIG" ]]; then
  echo "RetroArch base config not found: $RETROARCH_BASE_CONFIG" >&2
  write_remint_failure_summary "RetroArch base config not found: $RETROARCH_BASE_CONFIG" 2
fi

if [[ -z "$EXPECTED_VERIFY_CAPTURE_SHA256" && "$ALLOW_UNVERIFIED" != "1" ]]; then
  echo "[remint] --expected-verify-capture-sha256 is required unless --allow-unverified is set." >&2
  write_remint_failure_summary "missing required expected verification capture hash" 2
fi

echo "[remint] bootstrap bundle: $BOOTSTRAP_BUNDLE"
echo "[remint] verify bundle: $VERIFY_BUNDLE"
echo "[remint] output path: $OUTPUT_PATH"
echo "[remint] ROM stem: $rom_stem"

set +e
env \
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
  PARALLEL_N64_GFX_PLUGIN_OVERRIDE=parallel \
  "$RUNTIME_ADAPTER" \
  --bundle-dir "$BOOTSTRAP_BUNDLE" \
  --mode off \
  --retroarch-bin "$RETROARCH_BIN" \
  --base-config "$RETROARCH_BASE_CONFIG" \
  --core "$CORE_PATH" \
  --rom "$ROM_PATH" \
  --startup-wait "$STARTUP_WAIT" \
  --command "WAIT_COMMAND_READY 120" \
  --command "WAIT ${BOOT_WAIT_SECONDS}" \
  --command "SAVE_STATE" \
  --command "WAIT_SAVE_STATE" \
  --command "QUIT"
BOOTSTRAP_RC=$?
set -e
if (( BOOTSTRAP_RC != 0 )); then
  echo "[remint] bootstrap adapter failed with status $BOOTSTRAP_RC." >&2
  write_remint_failure_summary "bootstrap adapter exited with status $BOOTSTRAP_RC" 1
fi
mkdir -p "$BOOTSTRAP_BUNDLE/traces"
scenario_extract_hires_log_evidence "$BOOTSTRAP_BUNDLE" "$BOOTSTRAP_BUNDLE/traces/hires-evidence.json"

if [[ ! -f "$BOOTSTRAP_STATE" ]]; then
  echo "[remint] bootstrap state missing: $BOOTSTRAP_STATE" >&2
  write_remint_failure_summary "bootstrap state missing: $BOOTSTRAP_STATE" 1
fi

mkdir -p "$(dirname -- "$CANDIDATE_OUTPUT_PATH")"
cp "$BOOTSTRAP_STATE" "$CANDIDATE_OUTPUT_PATH"

mkdir -p "$VERIFY_BUNDLE/states/ParaLLEl N64"
cp "$CANDIDATE_OUTPUT_PATH" "$VERIFY_BUNDLE/states/ParaLLEl N64/${rom_stem}.state"

rm -rf "$VERIFY_BUNDLE/captures"
set +e
env \
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
  PARALLEL_N64_GFX_PLUGIN_OVERRIDE=parallel \
  "$RUNTIME_ADAPTER" \
  --bundle-dir "$VERIFY_BUNDLE" \
  --mode off \
  --retroarch-bin "$RETROARCH_BIN" \
  --base-config "$RETROARCH_BASE_CONFIG" \
  --core "$CORE_PATH" \
  --rom "$ROM_PATH" \
  --startup-wait "$STARTUP_WAIT" \
  --command "WAIT_COMMAND_READY 120" \
  --command "LOAD_STATE_SLOT_PAUSED 0" \
  --command "STEP_FRAME ${VERIFY_SETTLE_FRAMES}" \
  --command "WAIT_STATUS_FRAME PAUSED ${VERIFY_SETTLE_FRAMES} 15" \
  --command "SCREENSHOT" \
  --command "WAIT_NEW_CAPTURE 15" \
  --command "QUIT"
VERIFY_RC=$?
set -e
if (( VERIFY_RC != 0 )); then
  echo "[remint] verify adapter failed with status $VERIFY_RC." >&2
  write_remint_failure_summary "verify adapter exited with status $VERIFY_RC" 1
fi
mkdir -p "$VERIFY_BUNDLE/traces"
scenario_extract_hires_log_evidence "$VERIFY_BUNDLE" "$VERIFY_BUNDLE/traces/hires-evidence.json"

mapfile -t VERIFY_CAPTURES < <(find "$VERIFY_BUNDLE/captures" -maxdepth 1 -type f | sort)
if [[ "${#VERIFY_CAPTURES[@]}" -ne 1 ]]; then
  echo "[remint] expected exactly one verification capture, found ${#VERIFY_CAPTURES[@]}." >&2
  write_remint_failure_summary "expected exactly one verification capture, found ${#VERIFY_CAPTURES[@]}" 1
fi
VERIFY_CAPTURE="${VERIFY_CAPTURES[0]}"

STATE_SHA256="$(scenario_sha256_file "$CANDIDATE_OUTPUT_PATH")"
VERIFY_SHA256="$(scenario_sha256_file "$VERIFY_CAPTURE")"

VERIFY_STAGED_STATE="$VERIFY_BUNDLE/states/ParaLLEl N64/${rom_stem}.state"

python3 - "$GAME_ID" "$ROM_PATH" "$OUTPUT_PATH" "$CANDIDATE_OUTPUT_PATH" "$VERIFY_STAGED_STATE" "$CORE_PATH" "$BUNDLE_ROOT" "$BOOTSTRAP_BUNDLE" "$VERIFY_BUNDLE" \
  "$BOOT_WAIT_SECONDS" "$VERIFY_SETTLE_FRAMES" "$VERIFY_CAPTURE" "$STATE_SHA256" \
  "$VERIFY_SHA256" "$EXPECTED_VERIFY_CAPTURE_SHA256" "$ALLOW_UNVERIFIED" <<'PY'
import hashlib
import json
import re
import sys
from pathlib import Path

(
    game_id,
    rom_path,
    output_path,
    candidate_output_path,
    verify_staged_state,
    core_path,
    bundle_root,
    bootstrap_bundle,
    verify_bundle,
    boot_wait_seconds,
    verify_settle_frames,
    verify_capture,
    state_sha256,
    verify_sha256,
    expected_verify_sha256,
    allow_unverified,
) = sys.argv[1:]

def sha256_file(path: Path):
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()

bundle = Path(bundle_root)
bootstrap_bundle_path = Path(bootstrap_bundle)
verify_bundle_path = Path(verify_bundle)
log_text = (verify_bundle_path / "logs" / "retroarch.log").read_text(errors="replace") if (verify_bundle_path / "logs" / "retroarch.log").is_file() else ""
verify_staged_state_path = Path(verify_staged_state)
candidate_state_path = Path(candidate_output_path)
verify_staged_state_raw = str(verify_staged_state_path)
verify_staged_state_resolved = str(verify_staged_state_path.resolve())
load_ack_end = None
for match in re.finditer(r"\[State\] Loading state \"([^\"]+)\"", log_text):
    if match.group(1) in {verify_staged_state_raw, verify_staged_state_resolved}:
        load_ack_end = match.end()
        break
load_ack_uses_staged_state = load_ack_end is not None
post_load_status_frame = None
post_load_log_text = log_text[load_ack_end:] if load_ack_end is not None else ""
for match in re.finditer(r"GET_STATUS\s+PAUSED\b[^\n]*\bframe=(\d+)", post_load_log_text):
    post_load_status_frame = int(match.group(1))
staged_state_sha256 = sha256_file(verify_staged_state_path)
staged_state_matches_candidate = staged_state_sha256 == state_sha256
capture_hash_matches = bool(expected_verify_sha256) and verify_sha256 == expected_verify_sha256
unverified_review_only = not expected_verify_sha256 and allow_unverified == "1"
state_hard_checks_pass = (
    staged_state_matches_candidate
    and load_ack_uses_staged_state
    and post_load_status_frame is not None
    and post_load_status_frame >= int(verify_settle_frames)
)

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

def load_hires_evidence(bundle_path: Path):
    evidence_path = bundle_path / "traces" / "hires-evidence.json"
    if not evidence_path.is_file():
        return {}
    return json.loads(evidence_path.read_text())

def same_resolved_path(left, right):
    if not left or not right:
        return False
    try:
        return Path(left).resolve() == Path(right).resolve()
    except OSError:
        return str(left) == str(right)

rom_sha256 = sha256_file(Path(rom_path))
core_sha256 = sha256_file(Path(core_path))
expected_bootstrap_commands = [
    "WAIT_COMMAND_READY 120",
    f"WAIT {boot_wait_seconds}",
    "SAVE_STATE",
    "WAIT_SAVE_STATE",
    "QUIT",
]
expected_verify_commands = [
    "WAIT_COMMAND_READY 120",
    "LOAD_STATE_SLOT_PAUSED 0",
    f"STEP_FRAME {verify_settle_frames}",
    f"WAIT_STATUS_FRAME PAUSED {verify_settle_frames} 15",
    "SCREENSHOT",
    "WAIT_NEW_CAPTURE 15",
    "QUIT",
]
expected_bootstrap_command_signature = hashlib.sha256(
    ("\n".join(expected_bootstrap_commands) + "\n").encode()
).hexdigest()
expected_verify_command_signature = hashlib.sha256(
    ("\n".join(expected_verify_commands) + "\n").encode()
).hexdigest()

def read_command_log(path: Path):
    if not path.is_file():
        return None
    return path.read_text(errors="replace").splitlines()

def command_proof_log_matches(bundle_path: Path, expected_commands):
    proof_log = read_command_log(bundle_path / "retroarch.command-proofs.log")
    if proof_log is None:
        return False
    proof_commands = []
    proof_sources = []
    for line in proof_log:
        command, sep, proof = line.partition("\tproof=")
        proof_commands.append(command)
        proof_sources.append(bool(sep and proof))
    return proof_commands == expected_commands and len(proof_sources) == len(expected_commands) and all(proof_sources)

def off_hires_checks(bundle_path: Path, expected_commands, expected_command_signature: str):
    session = read_session_env(bundle_path / "retroarch.session.env")
    evidence = load_hires_evidence(bundle_path)
    summary_data = evidence.get("summary") or {}
    provider = summary_data.get("provider")
    entry_count = int(summary_data.get("entry_count") or 0)
    native_count = int(summary_data.get("native_sampled_entry_count") or 0)
    compat_count = int(summary_data.get("compat_entry_count") or 0)
    return {
        "hires_evidence_present": bool(evidence),
        "session_mode_off": session.get("MODE") == "off",
        "session_rom_path_match": same_resolved_path(session.get("ROM_PATH"), rom_path),
        "session_core_path_match": same_resolved_path(session.get("CORE_PATH"), core_path),
        "session_rom_sha256_match": session.get("ROM_SHA256") == rom_sha256,
        "session_core_sha256_match": session.get("CORE_SHA256") == core_sha256,
        "session_cache_path_empty": not session.get("HIRES_CACHE_PATH"),
        "session_cache_sha256_empty": not session.get("HIRES_CACHE_SHA256"),
        "session_command_signature_match": session.get("COMMAND_SIGNATURE") == expected_command_signature,
        "expected_command_log_match": read_command_log(bundle_path / "retroarch.expected.commands.log") == expected_commands,
        "executed_command_log_match": read_command_log(bundle_path / "retroarch.executed.commands.log") == expected_commands,
        "command_proof_log_match": command_proof_log_matches(bundle_path, expected_commands),
        "hires_unavailable": evidence.get("available") is not True,
        "hires_cache_not_loaded": evidence.get("cache_loaded") is not True,
        "hires_cache_path_empty": not evidence.get("cache_path"),
        "hires_cache_sha256_empty": not evidence.get("cache_sha256"),
        "hires_provider_off_or_absent": provider in (None, "off"),
        "hires_zero_entries": entry_count == 0 and native_count == 0 and compat_count == 0,
    }

bootstrap_off_checks = off_hires_checks(bootstrap_bundle_path, expected_bootstrap_commands, expected_bootstrap_command_signature)
verify_off_checks = off_hires_checks(verify_bundle_path, expected_verify_commands, expected_verify_command_signature)
bootstrap_off_safe = all(bootstrap_off_checks.values())
verify_off_safe = all(verify_off_checks.values())
hard_checks_pass = state_hard_checks_pass and bootstrap_off_safe and verify_off_safe
authoritative = capture_hash_matches and hard_checks_pass
passed = authoritative
summary = {
    "game_id": game_id,
    "bundle_root": str(bundle),
    "rom_path": rom_path,
    "rom_sha256": rom_sha256,
    "requested_output_path": output_path,
    "output_path": None,
    "output_written": False,
    "candidate_output_path": candidate_output_path,
    "state_sha256": state_sha256,
    "verify_staged_state_path": str(verify_staged_state_path),
    "verify_staged_state_sha256": staged_state_sha256,
    "core_path": core_path,
    "core_sha256": core_sha256,
    "expected_bootstrap_command_signature": expected_bootstrap_command_signature,
    "expected_verify_command_signature": expected_verify_command_signature,
    "boot_wait_seconds": float(boot_wait_seconds),
    "verify_settle_frames": int(verify_settle_frames),
    "verify_capture_path": verify_capture,
    "verify_capture_sha256": verify_sha256,
    "expected_verify_capture_sha256": expected_verify_sha256 or None,
    "verification_status": (
        "verified"
        if capture_hash_matches and hard_checks_pass
        else (
            "explicitly-unverified" if unverified_review_only and hard_checks_pass else "verification-failed"
        )
    ),
    "authoritative": authoritative,
    "promoted": False,
    "review_only_hard_checks_passed": unverified_review_only and hard_checks_pass,
    "post_load_status_frame": post_load_status_frame,
    "checks": {
        "expected_verify_capture_sha256_present": bool(expected_verify_sha256),
        "verify_capture_sha256_match": capture_hash_matches,
        "staged_state_sha256_match": staged_state_matches_candidate,
        "load_ack_uses_staged_state": load_ack_uses_staged_state,
        "post_load_paused_frame_reached": post_load_status_frame is not None and post_load_status_frame >= int(verify_settle_frames),
        "bootstrap_feature_off_safe": bootstrap_off_safe,
        "verify_feature_off_safe": verify_off_safe,
        "bootstrap_feature_off_checks": bootstrap_off_checks,
        "verify_feature_off_checks": verify_off_checks,
    },
    "passed": passed,
}
if not summary["passed"]:
    failures = []
    if not expected_verify_sha256:
        failures.append("verification capture hash was not supplied; remint is non-authoritative")
    elif not capture_hash_matches:
        failures.append(f"verification capture hash mismatch: expected {expected_verify_sha256}, got {verify_sha256}")
    if not staged_state_matches_candidate:
        failures.append(f"staged state hash mismatch: candidate {state_sha256}, staged {staged_state_sha256}")
    if not load_ack_uses_staged_state:
        failures.append(f"RetroArch log did not acknowledge loading staged state {verify_staged_state_path}")
    if post_load_status_frame is None or post_load_status_frame < int(verify_settle_frames):
        failures.append(f"RetroArch log did not confirm paused post-load frame >= {verify_settle_frames}; got {post_load_status_frame!r}")
    if not bootstrap_off_safe:
        failures.append(f"bootstrap run did not prove feature-off safety: {bootstrap_off_checks}")
    if not verify_off_safe:
        failures.append(f"verify run did not prove feature-off safety: {verify_off_checks}")
    summary["failure"] = "; ".join(failures)

(bundle / "remint-summary.json").write_text(json.dumps(summary, indent=2) + "\n")
(bundle / "remint-summary.md").write_text(
    "\n".join(
        [
            f"# {game_id} Cross-Game Boot State Remint",
            "",
            f"- Bundle: `{bundle}`",
            f"- Requested output: `{output_path}`",
            f"- Output: `{summary['output_path']}`",
            f"- Output written: `{str(summary['output_written']).lower()}`",
            f"- State SHA-256: `{state_sha256}`",
            f"- Verify capture SHA-256: `{verify_sha256}`",
            f"- Expected verify capture SHA-256: `{summary['expected_verify_capture_sha256']}`",
            f"- Verification status: `{summary['verification_status']}`",
            f"- Authoritative: `{str(summary['authoritative']).lower()}`",
            f"- Promoted: `{str(summary['promoted']).lower()}`",
            f"- Passed: `{str(summary['passed']).lower()}`",
        ]
    )
    + "\n"
)

if not summary["passed"]:
    print(summary["failure"], file=sys.stderr)
    print(bundle / "remint-summary.json", file=sys.stderr)
    sys.exit(1)

print(bundle / "remint-summary.json")
PY

if [[ -z "$EXPECTED_VERIFY_CAPTURE_SHA256" ]]; then
  echo "[remint] unverified candidate retained for review: $CANDIDATE_OUTPUT_PATH"
  echo "[remint] verification evidence: $BUNDLE_ROOT"
  exit 1
fi

mkdir -p "$(dirname -- "$OUTPUT_PATH")"
cp "$CANDIDATE_OUTPUT_PATH" "$OUTPUT_PATH"

python3 - "$BUNDLE_ROOT" <<'PY'
import json
import sys
from pathlib import Path

bundle = Path(sys.argv[1])
summary_path = bundle / "remint-summary.json"
summary = json.loads(summary_path.read_text())
summary["promoted"] = True
summary["output_path"] = summary.get("requested_output_path")
summary["output_written"] = True
summary_path.write_text(json.dumps(summary, indent=2) + "\n")
Path(bundle / "remint-summary.md").write_text(
    "\n".join(
        [
            f"# {summary['game_id']} Cross-Game Boot State Remint",
            "",
            f"- Bundle: `{bundle}`",
            f"- Requested output: `{summary.get('requested_output_path')}`",
            f"- Output: `{summary['output_path']}`",
            f"- Output written: `{str(summary.get('output_written')).lower()}`",
            f"- State SHA-256: `{summary['state_sha256']}`",
            f"- Verify capture SHA-256: `{summary['verify_capture_sha256']}`",
            f"- Expected verify capture SHA-256: `{summary['expected_verify_capture_sha256']}`",
            f"- Verification status: `{summary['verification_status']}`",
            f"- Authoritative: `{str(summary['authoritative']).lower()}`",
            f"- Promoted: `{str(summary['promoted']).lower()}`",
            f"- Passed: `{str(summary['passed']).lower()}`",
        ]
    )
    + "\n"
)
PY

echo "[remint] authoritative state sha256: $STATE_SHA256"
echo "[remint] verify capture sha256: $VERIFY_SHA256"
echo "[remint] complete: $BUNDLE_ROOT"
